// SPDX-License-Identifier: Apache-2.0
#include "omniweft/control.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <memory>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

namespace ow::control {
namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
using Time = Clock::time_point;
using Milliseconds = std::chrono::milliseconds;
constexpr std::size_t max_header_bytes = 16384;
constexpr std::size_t max_header_count = 64;
constexpr std::size_t max_body_bytes = 1048576;
constexpr std::size_t max_response_bytes = 4194304;
constexpr std::size_t max_request_line_bytes = 1024;
constexpr auto request_timeout = Milliseconds(1000);
// Normal completion owns the configured deadline; only emergency termination
// gets this fixed cleanup allowance. It never extends request/admission time.
constexpr auto shutdown_grace = Milliseconds(1000);
struct Failure {};
struct Closed {};
struct Rejection {
  int status;
  std::string code;
  std::string path;
};
[[noreturn]] void reject(int status, std::string code, std::string path = "") {
  throw Rejection{status, std::move(code), std::move(path)};
}
void config_valid(const Config& config) {
  if (config.world_id != "workshop" || config.seed != 7 ||
      config.max_slots < 1 || config.max_slots > 1024 ||
      config.session_ttl_ms < 50 || config.session_ttl_ms > 300000 ||
      config.max_runtime_ms < 1000 || config.max_runtime_ms > 600000 ||
      config.max_requests < 1 || config.max_requests > 4096) throw Failure{};
}

// This thread has no callbacks, world, credentials, socket, or pipe access.
// Its only exceptional action is process termination at the hard deadline,
// including a launcher that stops draining the private descriptor pipe.
class Watchdog {
 public:
  explicit Watchdog(Time deadline) : thread_([this, deadline] {
    std::unique_lock lock(mutex_);
    if (!condition_.wait_until(lock, deadline, [this] { return finished_; }))
      std::_Exit(4);
  }) {}
  ~Watchdog() {
    { std::lock_guard lock(mutex_); finished_ = true; }
    condition_.notify_one();
    thread_.join();
  }
  Watchdog(const Watchdog&) = delete;
  Watchdog& operator=(const Watchdog&) = delete;
 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  bool finished_ = false;
  std::thread thread_;
};

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket invalid_socket = INVALID_SOCKET;
void close_socket(NativeSocket value) { closesocket(value); }
bool interrupted() { return WSAGetLastError() == WSAEINTR; }
bool would_block() { return WSAGetLastError() == WSAEWOULDBLOCK; }
void nonblocking(NativeSocket value) {
  u_long enabled = 1;
  if (ioctlsocket(value, FIONBIO, &enabled) != 0) throw Failure{};
}
class SocketRuntime {
 public:
  SocketRuntime() { WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw Failure{}; }
  ~SocketRuntime() { WSACleanup(); }
};
#else
using NativeSocket = int;
constexpr NativeSocket invalid_socket = -1;
void close_socket(NativeSocket value) { close(value); }
bool interrupted() { return errno == EINTR; }
bool would_block() { return errno == EAGAIN || errno == EWOULDBLOCK; }
void nonblocking(NativeSocket value) {
  const auto flags = fcntl(value, F_GETFL);
  if (flags == -1 || fcntl(value, F_SETFL, flags | O_NONBLOCK) == -1 ||
      fcntl(value, F_SETFD, FD_CLOEXEC) == -1) throw Failure{};
}
class SocketRuntime {
 public:
  SocketRuntime() {
    // All network sends use MSG_NOSIGNAL; the private output pipe also must
    // report closure instead of letting SIGPIPE bypass orderly owner cleanup.
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) throw Failure{};
  }
};
#endif
class Socket {
 public:
  explicit Socket(NativeSocket value = invalid_socket) : value_(value) {}
  ~Socket() { if (value_ != invalid_socket) close_socket(value_); }
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
  NativeSocket get() const { return value_; }
 private:
  NativeSocket value_;
};
bool ready(NativeSocket socket, bool writing, Time deadline) {
  while (Clock::now() < deadline) {
    const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - Clock::now());
    if (remaining.count() <= 0) return false;
    #ifndef _WIN32
    if (socket >= FD_SETSIZE) throw Failure{};
    #endif
    fd_set set;
    FD_ZERO(&set);
    FD_SET(socket, &set);
    timeval timeout{};
    timeout.tv_sec = static_cast<long>(remaining.count() / 1000000);
    timeout.tv_usec = static_cast<long>(remaining.count() % 1000000);
#ifdef _WIN32
    const int result = select(0, writing ? nullptr : &set, writing ? &set : nullptr, nullptr, &timeout);
#else
    const int result = select(socket + 1, writing ? nullptr : &set, writing ? &set : nullptr, nullptr, &timeout);
#endif
    if (result > 0) return true;
    if (result == 0) return false;
    if (!interrupted()) throw Closed{};
  }
  return false;
}
std::size_t receive(NativeSocket socket, char* bytes, std::size_t capacity, Time deadline) {
  while (ready(socket, false, deadline)) {
    const auto count = recv(socket, bytes, static_cast<int>(capacity), 0);
    if (count > 0) return static_cast<std::size_t>(count);
    if (count == 0) throw Closed{};
    if (!interrupted() && !would_block()) throw Closed{};
  }
  throw Closed{};
}
void send_all(NativeSocket socket, std::string_view bytes, Time deadline) {
  std::size_t offset = 0;
  while (offset < bytes.size() && ready(socket, true, deadline)) {
#ifdef _WIN32
    const auto count = send(socket, bytes.data() + offset, static_cast<int>(bytes.size() - offset), 0);
#else
    const auto count = send(socket, bytes.data() + offset, bytes.size() - offset, MSG_NOSIGNAL);
#endif
    if (count > 0) offset += static_cast<std::size_t>(count);
    else if (count == 0 || (!interrupted() && !would_block())) throw Closed{};
  }
  if (offset != bytes.size()) throw Closed{};
}
// Complete a rejected legacy request without racing a separately sent body.
// Half-close only after the response is queued; discarded bytes never reach
// parsing/admission. Both work and waiting remain bounded by the original limits.
void finish_rejection(NativeSocket socket, Time deadline) {
#ifdef _WIN32
  if (shutdown(socket, SD_SEND) != 0) return;
#else
  if (shutdown(socket, SHUT_WR) != 0) return;
#endif
  std::array<char, 4096> discarded{};
  std::size_t remaining = max_body_bytes;
  try {
    while (remaining != 0) {
      remaining -= receive(socket, discarded.data(), std::min(remaining, discarded.size()), deadline);
    }
  } catch (const Closed&) { /* EOF, reset or the original deadline ends discard. */ }
}
NativeSocket create_listener(std::uint16_t& port) {
#ifdef _WIN32
  const auto raw = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT);
#else
  const auto raw = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
#endif
  if (raw == invalid_socket) throw Failure{};
  try {
#ifdef _WIN32
    const BOOL exclusive = TRUE;
    if (setsockopt(raw, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0) throw Failure{};
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(raw, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(raw, 8) != 0) throw Failure{};
#ifdef _WIN32
    int size = sizeof(address);
#else
    socklen_t size = sizeof(address);
#endif
    if (getsockname(raw, reinterpret_cast<sockaddr*>(&address), &size) != 0 ||
        address.sin_addr.s_addr != htonl(INADDR_LOOPBACK) || address.sin_port == 0) throw Failure{};
    port = ntohs(address.sin_port);
    nonblocking(raw);
    return raw;
  } catch (...) {
    close_socket(raw);
    throw;
  }
}
std::string random_secret() {
  std::array<unsigned char, 32> bytes{};
#ifdef _WIN32
  if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                     BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) throw Failure{};
#else
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto count = getrandom(bytes.data() + offset, bytes.size() - offset, 0);
    if (count > 0) offset += static_cast<std::size_t>(count);
    else if (count == 0 || errno != EINTR) throw Failure{};
  }
#endif
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (const auto value : bytes) { result += hex[value >> 4U]; result += hex[value & 15U]; }
  std::fill(bytes.begin(), bytes.end(), static_cast<unsigned char>(0));
  return result;
}
// The public request length is checked separately; equal-length token comparison
// examines all 64 bytes without a content-dependent early return.
bool same_secret(std::string_view supplied, std::string_view expected) {
  if (supplied.size() != expected.size()) return false;
  unsigned difference = 0;
  for (std::size_t index = 0; index < expected.size(); ++index)
    difference |= static_cast<unsigned char>(supplied[index]) ^ static_cast<unsigned char>(expected[index]);
  return difference == 0;
}

class PrivatePipes {
 public:
  PrivatePipes() {
#ifdef _WIN32
    input_ = GetStdHandle(STD_INPUT_HANDLE);
    output_ = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input_ == nullptr || output_ == nullptr || input_ == INVALID_HANDLE_VALUE ||
        output_ == INVALID_HANDLE_VALUE || GetFileType(input_) != FILE_TYPE_PIPE ||
        GetFileType(output_) != FILE_TYPE_PIPE) throw Rejection{3, "", ""};
#else
    struct stat input{}, output{};
    if (fstat(STDIN_FILENO, &input) != 0 || fstat(STDOUT_FILENO, &output) != 0 ||
        !S_ISFIFO(input.st_mode) || !S_ISFIFO(output.st_mode)) throw Rejection{3, "", ""};
#endif
  }
  // nullopt means no bytes currently available; empty string means pipe EOF.
  std::optional<std::string> read() {
    std::array<char, 8> bytes{};
#ifdef _WIN32
    DWORD available = 0, count = 0;
    if (!PeekNamedPipe(input_, nullptr, 0, nullptr, &available, nullptr)) {
      if (GetLastError() == ERROR_BROKEN_PIPE) return std::string{};
      throw Failure{};
    }
    if (available == 0) return std::nullopt;
    if (!ReadFile(input_, bytes.data(), std::min(available, static_cast<DWORD>(bytes.size())), &count, nullptr))
      throw Failure{};
#else
    pollfd descriptor{STDIN_FILENO, POLLIN, 0};
    const int result = poll(&descriptor, 1, 0);
    if (result < 0) { if (errno == EINTR) return std::nullopt; throw Failure{}; }
    if (result == 0) return std::nullopt;
    const auto count = ::read(STDIN_FILENO, bytes.data(), bytes.size());
    if (count < 0) { if (errno == EINTR) return std::nullopt; throw Failure{}; }
#endif
    return std::string(bytes.data(), static_cast<std::size_t>(count));
  }
  void descriptor(const Json& value, std::size_t limit = 513) {
    const std::string bytes = value.dump() + "\n";
    if (bytes.size() > limit || limit > 2048) throw Failure{};
    std::size_t offset = 0;
    while (offset < bytes.size()) {
#ifdef _WIN32
      DWORD count = 0;
      if (!WriteFile(output_, bytes.data() + offset, static_cast<DWORD>(bytes.size() - offset), &count, nullptr) ||
          count == 0) throw Failure{};
#else
      const auto count = ::write(STDOUT_FILENO, bytes.data() + offset, bytes.size() - offset);
      if (count < 0) { if (errno == EINTR) continue; throw Failure{}; }
      if (count == 0) throw Failure{};
#endif
      offset += static_cast<std::size_t>(count);
    }
  }
 private:
#ifdef _WIN32
  HANDLE input_ = nullptr;
  HANDLE output_ = nullptr;
#endif
};

struct Request {
  std::string method;
  std::string route;
  std::map<std::string, std::string> headers;
  std::string initial_body;
  std::size_t content_length = 0;
};
bool token_character(char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9') || std::string_view("!#$%&'*+-.^_`|~").find(value) != std::string_view::npos;
}
std::string lowercase(std::string_view text) {
  std::string result(text);
  for (char& value : result) if (value >= 'A' && value <= 'Z') value = static_cast<char>(value + ('a' - 'A'));
  return result;
}
std::string_view trim_spaces(std::string_view text) {
  while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
  while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
  return text;
}
Request read_headers(NativeSocket socket, Time deadline, bool header_only = false) {
  std::string bytes;
  bytes.reserve(max_header_bytes + 2048);
  std::size_t head_end = std::string::npos;
  while ((head_end = bytes.find("\r\n\r\n")) == std::string::npos) {
    if (bytes.size() >= max_header_bytes) reject(413, "BUDGET_EXCEEDED");
    std::array<char, 2048> chunk{};
    bytes.append(chunk.data(), receive(socket, chunk.data(), header_only ? 1 : chunk.size(), deadline));
    const auto line_end = bytes.find("\r\n");
    if ((line_end == std::string::npos && bytes.size() > max_request_line_bytes) ||
        (line_end != std::string::npos && line_end + 2 > max_request_line_bytes))
      reject(413, "BUDGET_EXCEEDED");
    const auto boundary = bytes.find("\r\n\r\n");
    const std::size_t inspected = boundary == std::string::npos ? bytes.size() : boundary + 4;
    for (std::size_t index = 0; index < inspected; ++index) {
      const auto value = static_cast<unsigned char>(bytes[index]);
      if (value == '\n') { if (index == 0 || bytes[index - 1] != '\r') reject(400, "INVALID_SCHEMA"); }
      else if (value == '\r') {
        if (index + 1 < inspected && bytes[index + 1] != '\n') reject(400, "INVALID_SCHEMA");
      } else if (value < 32 || value > 126) reject(400, "INVALID_SCHEMA");
    }
  }
  head_end += 4;
  if (head_end > max_header_bytes) reject(413, "BUDGET_EXCEEDED");
  const auto first_end = bytes.find("\r\n");
  const std::string_view first(bytes.data(), first_end);
  const auto first_space = first.find(' '), last_space = first.rfind(' ');
  if (first_space == 0 || first_space == std::string_view::npos || last_space == first_space ||
      first.substr(last_space + 1) != "HTTP/1.1") reject(400, "INVALID_SCHEMA");
  Request request;
  request.method = first.substr(0, first_space);
  request.route = first.substr(first_space + 1, last_space - first_space - 1);
  if (request.route.empty() || request.route.front() != '/' ||
      request.route.find(' ') != std::string::npos ||
      !std::all_of(request.method.begin(), request.method.end(), token_character)) reject(400, "INVALID_SCHEMA");
  std::size_t position = first_end + 2, count = 0;
  while (position + 2 < head_end) {
    const auto end = bytes.find("\r\n", position);
    if (end == position) break;
    if (++count > max_header_count) reject(413, "BUDGET_EXCEEDED");
    const std::string_view line(bytes.data() + position, end - position);
    const auto colon = line.find(':');
    if (colon == 0 || colon == std::string_view::npos ||
        !std::all_of(line.begin(), line.begin() + static_cast<std::ptrdiff_t>(colon), token_character))
      reject(400, "INVALID_SCHEMA");
    auto name = lowercase(line.substr(0, colon));
    if (!request.headers.emplace(name, trim_spaces(line.substr(colon + 1))).second) reject(400, "INVALID_SCHEMA");
    if (name == "transfer-encoding" || name == "expect" || name == "content-encoding" || name == "cookie")
      reject(400, "INVALID_SCHEMA");
    position = end + 2;
  }
  if (const auto found = request.headers.find("content-length"); found != request.headers.end()) {
    const auto& value = found->second;
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](char item) { return item >= '0' && item <= '9'; }))
      reject(400, "INVALID_SCHEMA");
    std::uint64_t length = 0;
    const auto converted = std::from_chars(value.data(), value.data() + value.size(), length);
    if (converted.ec != std::errc{} || converted.ptr != value.data() + value.size()) reject(400, "INVALID_SCHEMA");
    if (length > max_body_bytes) reject(413, "BUDGET_EXCEEDED");
    request.content_length = static_cast<std::size_t>(length);
  } else if (request.method == "POST") reject(400, "INVALID_SCHEMA");
  if (request.method == "GET" && request.content_length != 0) reject(400, "INVALID_SCHEMA");
  if (request.method == "POST") {
    const auto type = request.headers.find("content-type");
    if (type == request.headers.end() || type->second != "application/json") reject(400, "INVALID_SCHEMA");
  }
  request.initial_body = bytes.substr(head_end, request.content_length);
  return request;
}
std::string read_body(NativeSocket socket, Request& request, Time deadline) {
  std::string bytes = std::move(request.initial_body);
  bytes.reserve(request.content_length);
  while (bytes.size() < request.content_length) {
    std::array<char, 8192> chunk{};
    const auto count = receive(socket, chunk.data(), std::min(chunk.size(), request.content_length - bytes.size()), deadline);
    bytes.append(chunk.data(), count);
  }
  if (Clock::now() >= deadline) throw Closed{};
  return bytes;
}
void respond(NativeSocket socket, int status, const Json& value, Time deadline) {
  const std::string body = value.dump();
  if (body.size() > max_response_bytes) throw Failure{};
  const std::string bytes = "HTTP/1.1 " + std::to_string(status) + " Response\r\n"
    "Content-Type: application/json\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: " +
    std::to_string(body.size()) + "\r\n\r\n" + body;
  send_all(socket, bytes, deadline);
}

class ObserveSax final : public nlohmann::json_sax<Json> {
 public:
  bool budget = false;
  bool null() override { return true; }
  bool boolean(bool) override { return true; }
  bool number_integer(number_integer_t) override { return true; }
  bool number_unsigned(number_unsigned_t) override { return true; }
  bool number_float(number_float_t value, const string_t&) override { return std::isfinite(value); }
  bool string(string_t&) override { return true; }
  bool binary(binary_t&) override { return false; }
  bool start_object(std::size_t) override { return open(true); }
  bool start_array(std::size_t) override { return open(false); }
  bool key(string_t& value) override {
    return !frames_.empty() && frames_.back().object && frames_.back().keys.insert(value).second;
  }
  bool end_object() override { frames_.pop_back(); return true; }
  bool end_array() override { frames_.pop_back(); return true; }
  bool parse_error(std::size_t, const std::string&, const Json::exception&) override { return false; }
 private:
  struct Frame { bool object; std::unordered_set<std::string> keys; };
  std::vector<Frame> frames_;
  bool open(bool object) {
    if (frames_.size() >= commands::max_nesting_depth) { budget = true; return false; }
    frames_.push_back({object, {}});
    return true;
  }
};
void validate_observe(std::string_view body, const std::string& world_id) {
  ObserveSax sax;
  if (!Json::sax_parse(body.begin(), body.end(), &sax))
    reject(sax.budget ? 413 : 400, sax.budget ? "BUDGET_EXCEEDED" : "INVALID_SCHEMA");
  if (body.find('\0') != std::string_view::npos) reject(400, "INVALID_SCHEMA");
  const auto value = Json::parse(body.begin(), body.end());
  if (!value.is_object() || value.size() != 2) reject(400, "INVALID_SCHEMA");
  if (!value.contains("protocol_version") || !value["protocol_version"].is_string())
    reject(400, "INVALID_SCHEMA", "/protocol_version");
  if (value["protocol_version"] != "0.1") reject(400, "UNSUPPORTED_VERSION", "/protocol_version");
  if (!value.contains("world_id") || !value["world_id"].is_string()) reject(400, "INVALID_SCHEMA", "/world_id");
  if (value["world_id"] != world_id) reject(403, "NOT_AUTHORIZED", "/world_id");
}
// Unknown JSON keys can contain credentials or arbitrary attacker text.
// Preserve useful fixed schema pointers, never echo unknown path components.
std::string safe_path(std::string_view source) {
  constexpr std::array<std::string_view, 26> names{
    "protocol_version", "world_id", "transaction_id", "idempotency", "epoch", "sequence",
    "expected_world_revision", "apply_at", "mode", "expires_after_ticks", "budget", "max_operations",
    "max_blob_bytes", "operations", "type", "temporary_id", "prefab", "target", "entity_uuid",
    "generation", "position_m", "rotation_xyzw", "scale", "child_policy", "principal", "grant"};
  std::string result;
  while (source.starts_with('/') && result.size() < 192) {
    source.remove_prefix(1);
    const auto end = source.find('/');
    const auto part = source.substr(0, end);
    bool allowed = std::find(names.begin(), names.end(), part) != names.end();
    if (!allowed && !part.empty() && part.size() <= 3) {
      unsigned index = 0;
      const auto parsed = std::from_chars(part.data(), part.data() + part.size(), index);
      allowed = parsed.ec == std::errc{} && parsed.ptr == part.data() + part.size() && index < 256;
    }
    if (!allowed) break;
    result += '/';
    result += part;
    if (end == std::string_view::npos) break;
    source.remove_prefix(end);
  }
  return result;
}
[[noreturn]] void command_rejection(const commands::ValidationError& error) {
  if (error.code == "BUDGET_EXCEEDED") reject(413, "BUDGET_EXCEEDED", safe_path(error.path));
  if (error.code == "UNSUPPORTED_VERSION") reject(400, "UNSUPPORTED_VERSION", safe_path(error.path));
  if (error.code == "UNSUPPORTED_OPERATION") reject(405, "UNSUPPORTED_OPERATION", safe_path(error.path));
  reject(400, "INVALID_SCHEMA", safe_path(error.path));
}
Json snapshot_json(const world::Snapshot& snapshot) {
  Json slots = Json::array();
  for (const auto& slot : snapshot.slots) {
    Json entity = nullptr;
    if (slot.entity) {
      const auto& value = *slot.entity;
      entity = {{"prefab", value.prefab}, {"authoring_revision", value.authoring_revision},
        {"transform", {{"position_m", value.transform.position_m},
          {"rotation_xyzw", value.transform.rotation_xyzw}, {"scale", value.transform.scale}}}};
    }
    slots.push_back({{"entity_uuid", slot.entity_uuid}, {"generation", slot.generation},
      {"retired", slot.retired}, {"entity", std::move(entity)}});
  }
  return {{"format_version", snapshot.format_version}, {"world_id", snapshot.world_id},
    {"seed", snapshot.seed}, {"max_slots", snapshot.max_slots},
    {"world_revision", snapshot.world_revision}, {"slots", std::move(slots)}};
}
Json receipt_json(const transactions::Receipt& receipt) {
  Json errors = Json::array(), created = Json::array();
  for (const auto& error : receipt.errors) {
    Json item = {{"code", error.code}, {"path", error.path}, {"message", error.message}};
    if (error.operation_index) item["operation_index"] = *error.operation_index;
    errors.push_back(std::move(item));
  }
  for (const auto& binding : receipt.created)
    created.push_back({{"temporary_id", binding.temporary_id}, {"world_id", binding.world_id},
      {"entity_uuid", binding.entity_uuid}, {"generation", binding.generation}});
  return {{"status", receipt.status}, {"durability", receipt.durability},
    {"transaction_id", receipt.transaction_id}, {"world_revision", receipt.world_revision},
    {"created", std::move(created)}, {"errors", std::move(errors)}};
}

class Host {
 public:
  Host(const Config& config, const Callbacks& callbacks, PrivatePipes& pipes, std::uint16_t port)
      : config_(config), callbacks_(callbacks), pipes_(pipes), port_(port) {}
  bool renew(Time deadline) {
    if (Clock::now() >= deadline) return false;
    auto token = random_secret(), epoch = random_secret();
    if (Clock::now() >= deadline) return false;
    token_ = std::move(token);
    epoch_ = std::move(epoch);
    next_sequence_ = 1;
    exhausted_ = false;
    expires_ = Clock::now() + Milliseconds(config_.session_ttl_ms);
    pipes_.descriptor({{"schema_version", 1}, {"protocol_version", "0.1"}, {"host", "127.0.0.1"},
      {"port", port_}, {"token", token_}, {"epoch", epoch_}, {"session_ttl_ms", config_.session_ttl_ms}});
    return true;
  }
  bool host_commands(Time deadline) {
    while (Clock::now() < deadline) {
      const auto chunk = pipes_.read();
      if (!chunk) return true;
      if (chunk->empty()) {
        if (!pending_command_.empty()) throw Failure{};
        return false;
      }
      for (const char character : *chunk) {
        if (Clock::now() >= deadline) return false;
        if (character == '\n') {
          if (pending_command_ == "stop") return false;
          if (pending_command_ != "renew" || renewals_ >= 64) throw Failure{};
          ++renewals_;
          pending_command_.clear();
          if (!renew(deadline)) return false;
        } else {
          if (pending_command_.size() >= 5 || character < 'a' || character > 'z') throw Failure{};
          pending_command_ += character;
          if (!std::string_view("renew").starts_with(pending_command_) &&
              !std::string_view("stop").starts_with(pending_command_)) throw Failure{};
        }
      }
    }
    return false;
  }
  void connection(NativeSocket socket, Time deadline) {
    try {
      auto request = read_headers(socket, deadline);
      // Single real authentication gate shared by every route, before any
      // world observation or parsing/admission. Independent mutation proof
      // disables this condition only in its disposable source copy.
      if (!authorized(request))
        reject(401, "NOT_AUTHORIZED");
      check_expiry();
      const auto host = request.headers.find("host");
      if (host == request.headers.end() || host->second != "127.0.0.1:" + std::to_string(port_) ||
          request.headers.contains("origin")) reject(403, "NOT_AUTHORIZED");
      const auto protocol = request.headers.find("x-omniweft-protocol");
      if (protocol == request.headers.end() || protocol->second != "0.1")
        reject(400, "UNSUPPORTED_VERSION", "/protocol_version");
      if (request.route != "/v0/capabilities" && request.route != "/v0/observe" &&
          request.route != "/v0/transactions" &&
          !(request.route == "/v0/runtime" && callbacks_.runtime_status)) reject(404, "NOT_FOUND");
      const bool capabilities = request.route == "/v0/capabilities";
      const bool runtime_status = request.route == "/v0/runtime";
      if (request.method != (capabilities || runtime_status ? "GET" : "POST")) reject(405, "UNSUPPORTED_OPERATION");
      const auto body = read_body(socket, request, deadline);
      if (capabilities) {
        world::Snapshot snapshot;
        boundary(deadline, false, [&] {
          check_deadline(deadline); check_expiry(); snapshot = bounded_snapshot();
        });
        respond(socket, 200, {
          {"protocol_version", "0.1"}, {"epoch", epoch_}, {"next_sequence", next_sequence_},
          {"world_id", config_.world_id}, {"world_revision", snapshot.world_revision},
          {"operations", {"entity.create", "transform.set", "entity.delete"}},
          {"limits", {{"max_header_bytes", max_header_bytes}, {"max_body_bytes", max_body_bytes},
            {"max_response_bytes", max_response_bytes}, {"max_operations", commands::max_operations},
            {"max_slots", config_.max_slots}, {"request_timeout_ms", request_timeout.count()},
            {"session_ttl_ms", config_.session_ttl_ms}}},
          {"admission", "synchronous"}, {"durability", "volatile"}, {"retry_mode", "resync_only"}}, deadline);
      } else if (runtime_status) {
        RuntimeStatus status;
        boundary(deadline, false, [&] {
          check_deadline(deadline); check_expiry(); status = callbacks_.runtime_status();
        });
        respond(socket, 200, {{"protocol_version", "0.1"}, {"epoch", epoch_}, {"next_sequence", next_sequence_},
          {"runtime", {{"schema_version", 1}, {"tick_rate_hz", 60}, {"max_catch_up_steps", 4},
            {"simulation_tick", status.simulation_tick}, {"snapshot_sequence", status.snapshot_sequence},
            {"overload_count", status.overload_count}, {"dropped_ticks", status.dropped_ticks},
            {"remainder_units", status.remainder_units}, {"snapshot", snapshot_json(status.snapshot)},
            {"presentation", {{"enabled", status.presentation.enabled}, {"ready", status.presentation.ready},
              {"frame_count", status.presentation.frame_count}, {"world_revision", status.presentation.world_revision},
              {"snapshot_sequence", status.presentation.snapshot_sequence}}}}}}, deadline);
      } else if (request.route == "/v0/observe") {
        validate_observe(body, config_.world_id);
        world::Snapshot snapshot;
        boundary(deadline, false, [&] {
          check_deadline(deadline); check_expiry(); snapshot = bounded_snapshot();
        });
        respond(socket, 200, {{"protocol_version", "0.1"}, {"epoch", epoch_}, {"next_sequence", next_sequence_},
          {"snapshot", snapshot_json(snapshot)}}, deadline);
      } else {
        const auto parsed = commands::parse(body);
        if (!parsed.envelope) {
          if (parsed.errors.empty()) reject(400, "INVALID_SCHEMA");
          command_rejection(parsed.errors.front());
        }
        const auto& envelope = *parsed.envelope;
        if (envelope.world_id != config_.world_id) reject(403, "NOT_AUTHORIZED", "/world_id");
        for (std::size_t index = 0; index < envelope.operations.size(); ++index) {
          std::visit([&](const auto& operation) {
            using Operation = std::decay_t<decltype(operation)>;
            if constexpr (!std::is_same_v<Operation, commands::EntityCreate>) {
              if (const auto* target = std::get_if<commands::EntityTarget>(&operation.target);
                  target != nullptr && target->world_id != config_.world_id)
                reject(403, "NOT_AUTHORIZED", "/operations/" + std::to_string(index) + "/target/world_id");
            }
          }, envelope.operations[index]);
        }
        transactions::Receipt receipt;
        boundary(deadline, true, [&] {
          check_deadline(deadline);
          check_expiry();
          if (!same_secret(envelope.idempotency.epoch, epoch_))
            reject(409, "REQUIRES_RESYNC", "/idempotency/epoch");
          if (exhausted_ || envelope.idempotency.sequence != next_sequence_)
            reject(409, "REQUIRES_RESYNC", "/idempotency/sequence");
          // Final session/deadline checks and sequence consumption occur together
          // at the actual owner boundary, after any queue delay.
          check_deadline(deadline);
          check_expiry();
          if (callbacks_.should_stop && callbacks_.should_stop()) throw Closed{};
          if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) exhausted_ = true;
          else ++next_sequence_;
          receipt = callbacks_.apply(envelope);
        });
        if (receipt.created.size() > commands::max_operations || receipt.errors.size() > commands::max_operations)
          throw Failure{};
        respond(socket, 200, {{"protocol_version", "0.1"}, {"epoch", epoch_}, {"next_sequence", next_sequence_},
          {"receipt", receipt_json(receipt)}}, deadline);
      }
    } catch (const Rejection& error) {
      respond(socket, error.status, {{"protocol_version", "0.1"}, {"status", "rejected"},
        {"error", {{"code", error.code}, {"path", error.path}}}}, deadline);
      finish_rejection(socket, deadline);
    }
  }
 private:
  Config config_;
  const Callbacks& callbacks_;
  PrivatePipes& pipes_;
  std::uint16_t port_;
  std::string token_, epoch_;
  std::string pending_command_;
  std::uint64_t next_sequence_ = 1;
  std::uint32_t renewals_ = 0;
  bool exhausted_ = false;
  Time expires_{};

  void boundary(Time deadline, bool authoring, std::function<void()> action) {
    if (callbacks_.at_boundary) {
      if (!callbacks_.at_boundary(std::move(action), deadline, authoring)) throw Closed{};
    } else action();
  }
  bool authorized(const Request& request) const {
    const auto found = request.headers.find("authorization");
    if (found == request.headers.end() || !std::string_view(found->second).starts_with("Bearer ")) return false;
    return same_secret(std::string_view(found->second).substr(7), token_);
  }
  void check_expiry() const {
    if (Clock::now() >= expires_) reject(401, "SESSION_EXPIRED");
  }
  static void check_deadline(Time deadline) {
    if (Clock::now() >= deadline) throw Closed{};
  }
  world::Snapshot bounded_snapshot() const {
    auto snapshot = callbacks_.snapshot();
    if (snapshot.world_id != config_.world_id || snapshot.seed != config_.seed ||
        snapshot.max_slots != config_.max_slots || snapshot.slots.size() > config_.max_slots) throw Failure{};
    return snapshot;
  }
};
// Shares framing, loopback restrictions, cryptographic tokens and JSON codecs
// with the legacy host, while keeping the new session/profile opt-in.
class PolicyHost {
 public:
  PolicyHost(const Config& config, const PolicyCallbacks& callbacks, PrivatePipes& pipes, std::uint16_t port)
    : config_(config), callbacks_(callbacks), pipes_(pipes), port_(port) {}
  bool renew(Time deadline) {
    if (Clock::now() >= deadline) return false;
    Json descriptors = Json::object();
    {
      std::lock_guard lock(mutex_);
      for (std::size_t i=0;i<policy::principal_count;++i) {
        auto& session=sessions_[i];
        session.token=random_secret(); session.epoch=random_secret(); session.sequence=1;
        session.exhausted=false; session.expires=Clock::now()+Milliseconds(config_.session_ttl_ms);
        descriptors[std::string(policy::grant(i).principal)]={{"schema_version",1},{"protocol_version","0.1"},
          {"host","127.0.0.1"},{"port",port_},{"token",session.token},{"epoch",session.epoch},
          {"session_ttl_ms",config_.session_ttl_ms}};
      }
    }
    if (Clock::now() >= deadline) return false;
    pipes_.descriptor({{"schema_version",2},{"profile","policy.v1"},{"principals",std::move(descriptors)}},2048);
    return true;
  }
  bool host_commands(Time deadline) {
    while (Clock::now()<deadline) {
      const auto chunk=pipes_.read();
      if(!chunk) return true;
      if(chunk->empty()) {if(!pending_.empty()) throw Failure{}; return false;}
      for(const char c:*chunk) {
        if(c=='\n') {
          if(pending_=="stop") return false;
          if(pending_!="renew" || renewals_>=64) throw Failure{};
          ++renewals_;pending_.clear();if(!renew(deadline)) return false;
        } else {
          if(pending_.size()>=5 || c<'a' || c>'z') throw Failure{};
          pending_+=c;
          if(!std::string_view("renew").starts_with(pending_) &&
             !std::string_view("stop").starts_with(pending_)) throw Failure{};
        }
      }
    }
    return false;
  }
  void connection(NativeSocket socket, Time deadline) {
    // Kept outside try: even an error response holds its accepted lease until
    // the socket operation ends, including timeout, exception and disconnect.
    std::optional<policy::Lease> lease;
    try {
      // Fixture policy ingress stops exactly at the header delimiter. Body
      // bytes remain in the socket until their principal reservation succeeds.
      auto request=read_headers(socket,deadline,true);
      std::size_t principal=policy::principal_count;
      std::string epoch;
      {
        std::lock_guard lock(mutex_);
        const auto auth=request.headers.find("authorization");
        if(auth!=request.headers.end() && std::string_view(auth->second).starts_with("Bearer ")) {
          for(std::size_t i=0;i<policy::principal_count;++i)
            if(same_secret(std::string_view(auth->second).substr(7),sessions_[i].token)) principal=i;
        }
        if(principal==policy::principal_count) reject(401,"NOT_AUTHORIZED");
        epoch=sessions_[principal].epoch;
        current(principal,epoch,deadline);
      }
      const auto host=request.headers.find("host");
      if(host==request.headers.end() || host->second!="127.0.0.1:"+std::to_string(port_) ||
         request.headers.contains("origin")) reject(403,"NOT_AUTHORIZED");
      const auto protocol=request.headers.find("x-omniweft-protocol");
      if(protocol==request.headers.end() || protocol->second!="0.1")
        reject(400,"UNSUPPORTED_VERSION","/protocol_version");
      const bool capabilities=request.route=="/v0/capabilities";
      const bool status=request.route=="/v0/policy";
      const bool runtime=request.route=="/v0/runtime";
      const bool observe=request.route=="/v0/observe";
      const bool transaction=request.route=="/v0/transactions";
      if(!capabilities && !status && !runtime && !observe && !transaction) reject(404,"NOT_FOUND");
      if(request.method!=(capabilities||status||runtime?"GET":"POST")) reject(405,"UNSUPPORTED_OPERATION");
      if(capabilities || status) {
        // These fixed-size control routes do not wait for or allocate a body.
        if(request.content_length!=0 || !request.initial_body.empty()) reject(400,"INVALID_SCHEMA");
        Json response;
        {
          std::lock_guard lock(mutex_);current(principal,epoch,deadline);
          const auto usage=callbacks_.ledger->usage();
          const auto& session=sessions_[principal];
          response={{"protocol_version","0.1"},{"epoch",session.epoch},{"next_sequence",session.sequence},
            {"world_id",config_.world_id},{"world_revision",usage.world_revision}};
          if(capabilities) {
            response["operations"]={"entity.create","transform.set","entity.delete"};
            response["limits"]={{"max_header_bytes",max_header_bytes},{"max_body_bytes",max_body_bytes},
              {"max_response_bytes",max_response_bytes},{"max_operations",commands::max_operations},
              {"max_slots",config_.max_slots},{"request_timeout_ms",request_timeout.count()},
              {"session_ttl_ms",config_.session_ttl_ms}};
            response["admission"]="synchronous";response["durability"]="volatile";response["retry_mode"]="resync_only";
          } else {
            const auto& grant=policy::grant(principal);
            response["policy"]={{"schema_version",1},{"principal",grant.principal},{"read_scope","whole_world"},
              {"write_bounds_m",{{"lower",grant.lower},{"upper",grant.upper}}},
              {"memory_model","charged-resource-bytes-v1"},
              {"limits",{{"max_operations",policy::operation_limit},{"max_body_bytes",policy::body_limit},
                {"retained_bytes",grant.retained_limit},{"working_bytes",grant.working_limit},
                {"observation_bytes",grant.observation_limit},{"requests",1},{"global_requests",2}}},
              {"usage",{{"retained_bytes",usage.retained[principal]},{"working_bytes",usage.working[principal]},
                {"requests",usage.requests[principal]},{"global_requests",usage.global_requests}}}};
          }
        }
        respond(socket,200,response,deadline);
        return;
      }
      try {lease.emplace(callbacks_.ledger->admit(principal,request.content_length));}
      catch(const policy::Denied& error) {reject(413,error.code,error.path);}
      const auto body=read_body(socket,request,deadline);
      Json response;
      if(observe || runtime) {
        if(observe) validate_observe(body,config_.world_id);
        boundary(deadline,false,[&] {
          std::lock_guard lock(mutex_);current(principal,epoch,deadline);
          const auto& session=sessions_[principal];
          response={{"protocol_version","0.1"},{"epoch",session.epoch},{"next_sequence",session.sequence}};
          if(observe) response["snapshot"]=snapshot_json(callbacks_.shared.snapshot());
          else {
            const auto value=callbacks_.shared.runtime_status();
            response["runtime"]={{"schema_version",1},{"tick_rate_hz",60},{"max_catch_up_steps",4},
              {"simulation_tick",value.simulation_tick},{"snapshot_sequence",value.snapshot_sequence},
              {"overload_count",value.overload_count},{"dropped_ticks",value.dropped_ticks},
              {"remainder_units",value.remainder_units},{"snapshot",snapshot_json(value.snapshot)},
              {"presentation",{{"enabled",value.presentation.enabled},{"ready",value.presentation.ready},
                {"frame_count",value.presentation.frame_count},{"world_revision",value.presentation.world_revision},
                {"snapshot_sequence",value.presentation.snapshot_sequence}}}};
          }
        });
        // The fixed working reservation includes the bounded full fixture
        // representation. No success headers/body are sent before this limit.
        if(response.dump().size()>policy::grant(principal).observation_limit)
          reject(413,"BUDGET_EXCEEDED","/observation");
      } else {
        const auto parsed=commands::parse(body);
        if(!parsed.envelope) {
          if(parsed.errors.empty()) reject(400,"INVALID_SCHEMA");
          command_rejection(parsed.errors.front());
        }
        const auto& envelope=*parsed.envelope;
        if(envelope.operations.size()>policy::operation_limit)
          reject(413,"BUDGET_EXCEEDED","/operations");
        if(envelope.world_id!=config_.world_id) reject(403,"NOT_AUTHORIZED","/world_id");
        for(const auto& item:envelope.operations) std::visit([&](const auto& operation) {
          using T=std::decay_t<decltype(operation)>;
          if constexpr (!std::is_same_v<T,commands::EntityCreate>) {
            const auto* target=std::get_if<commands::EntityTarget>(&operation.target);
            if(target && target->world_id!=config_.world_id) reject(403,"NOT_AUTHORIZED","/operations");
          }
        },item);
        boundary(deadline,true,[&] {
          std::lock_guard lock(mutex_);current(principal,epoch,deadline);
          auto& session=sessions_[principal];
          if(!same_secret(envelope.idempotency.epoch,session.epoch))
            reject(409,"REQUIRES_RESYNC","/idempotency/epoch");
          if(session.exhausted || envelope.idempotency.sequence!=session.sequence)
            reject(409,"REQUIRES_RESYNC","/idempotency/sequence");
          if(session.sequence==std::numeric_limits<std::uint64_t>::max()) session.exhausted=true;
          else ++session.sequence;
          const auto receipt=callbacks_.apply(envelope,*lease);
          response={{"protocol_version","0.1"},{"epoch",session.epoch},{"next_sequence",session.sequence},
            {"receipt",receipt_json(receipt)}};
        });
      }
      respond(socket,200,response,deadline);
    } catch(const Rejection& error) {
      respond(socket,error.status,{{"protocol_version","0.1"},{"status","rejected"},
        {"error",{{"code",error.code},{"path",error.path}}}},deadline);
    }
  }
 private:
  struct Session {std::string token,epoch;std::uint64_t sequence=1;bool exhausted=false;Time expires{};};
  Config config_;
  const PolicyCallbacks& callbacks_;
  PrivatePipes& pipes_;
  std::uint16_t port_;
  std::mutex mutex_;
  std::array<Session,policy::principal_count> sessions_;
  std::string pending_;
  unsigned renewals_=0;
  void current(std::size_t principal,const std::string& epoch,Time deadline) const {
    if(Clock::now()>=deadline || (callbacks_.shared.should_stop && callbacks_.shared.should_stop())) throw Closed{};
    if(!same_secret(sessions_[principal].epoch,epoch)) reject(401,"SESSION_EXPIRED");
    if(Clock::now()>=sessions_[principal].expires) reject(401,"SESSION_EXPIRED");
  }
  void boundary(Time deadline,bool authoring,std::function<void()> action) {
    if(!callbacks_.shared.at_boundary(std::move(action),deadline,authoring)) throw Closed{};
  }
};
}  // namespace

int run(const Config& config, const Callbacks& callbacks) noexcept {
  try {
    config_valid(config);
    if (!callbacks.snapshot || !callbacks.apply) throw Failure{};
    PrivatePipes pipes;
    const auto deadline = callbacks.normal_deadline.value_or(Clock::now() + Milliseconds(config.max_runtime_ms));
    Watchdog watchdog(deadline + shutdown_grace);
    SocketRuntime runtime;
    std::uint16_t port = 0;
    Socket listener(create_listener(port));
    Host host(config, callbacks, pipes, port);
    if (!host.renew(deadline)) return 0;
    std::uint32_t count = 0;
    while (Clock::now() < deadline && count < config.max_requests &&
           !(callbacks.should_stop && callbacks.should_stop())) {
      if (!host.host_commands(deadline)) return 0;
      if (!ready(listener.get(), false, std::min(deadline, Clock::now() + Milliseconds(20)))) continue;
      sockaddr_in peer{};
#ifdef _WIN32
      int peer_size = sizeof(peer);
#else
      socklen_t peer_size = sizeof(peer);
#endif
      const auto accepted = accept(listener.get(), reinterpret_cast<sockaddr*>(&peer), &peer_size);
      if (accepted == invalid_socket) {
        if (would_block() || interrupted()) continue;
        throw Failure{};
      }
      Socket connection(accepted);
      ++count;
      if (peer.sin_family != AF_INET || peer.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) continue;
      nonblocking(connection.get());
      try { host.connection(connection.get(), std::min(deadline, Clock::now() + request_timeout)); }
      catch (const Closed&) { /* Incomplete/expired transport has no further admission or output. */ }
    }
    return 0;
  } catch (const Rejection& error) {
    if (error.status == 3) { std::fputs("CONTROL_PRIVATE_PIPES_REQUIRED\n", stderr); return 3; }
  } catch (...) {
    // Deliberately suppress exception strings: OS/JSON failures can contain
    // credentials, input bytes, paths or other private process information.
  }
  std::fputs("CONTROL_FAILED\n", stderr);
  return 4;
}

int run_policy(const Config& config,const PolicyCallbacks& callbacks) noexcept {
  try {
    config_valid(config);
    if(config.max_slots!=policy::slot_count || !callbacks.ledger || !callbacks.apply ||
       !callbacks.shared.snapshot || !callbacks.shared.runtime_status || !callbacks.shared.at_boundary) throw Failure{};
    PrivatePipes pipes;
    const auto deadline=callbacks.shared.normal_deadline.value_or(Clock::now()+Milliseconds(config.max_runtime_ms));
    Watchdog watchdog(deadline+shutdown_grace);
    SocketRuntime runtime;
    std::uint16_t port=0;Socket listener(create_listener(port));
    PolicyHost host(config,callbacks,pipes,port);
    if(!host.renew(deadline)) return 0;
    std::array<std::atomic<bool>,3> available{true,true,true};
    std::atomic<bool> failed=false;
    // Join workers before destroying any state captured by reference, even on exceptions.
    std::array<std::jthread,3> workers;
    std::uint32_t count=0;
    while(Clock::now()<deadline && count<config.max_requests && !failed.load() &&
          !(callbacks.shared.should_stop && callbacks.shared.should_stop())) {
      if(!host.host_commands(deadline)) break;
      std::size_t worker=0;
      while(worker<workers.size() && !available[worker].load()) ++worker;
      if(worker==workers.size()) {std::this_thread::sleep_for(Milliseconds(1));continue;}
      if(!ready(listener.get(),false,std::min(deadline,Clock::now()+Milliseconds(10)))) continue;
      sockaddr_in peer{};
#ifdef _WIN32
      int peer_size=sizeof(peer);
#else
      socklen_t peer_size=sizeof(peer);
#endif
      const auto accepted=accept(listener.get(),reinterpret_cast<sockaddr*>(&peer),&peer_size);
      if(accepted==invalid_socket) {if(would_block()||interrupted()) continue;throw Failure{};}
      auto connection=std::make_shared<Socket>(accepted);
      ++count;
      if(peer.sin_family!=AF_INET || peer.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)) continue;
      nonblocking(connection->get());
      if(workers[worker].joinable()) workers[worker].join();
      available[worker].store(false);
      const auto request_deadline=std::min(deadline,Clock::now()+request_timeout);
      workers[worker]=std::jthread([&,worker,connection,request_deadline] {
        try {host.connection(connection->get(),request_deadline);}
        catch(const Closed&) {}
        catch(...) {failed.store(true);}
        available[worker].store(true);
      });
    }
    for(auto& worker:workers) if(worker.joinable()) worker.join();
    if(failed.load()) throw Failure{};
    return 0;
  } catch(const Rejection& error) {
    if(error.status==3) {std::fputs("CONTROL_PRIVATE_PIPES_REQUIRED\n",stderr);return 3;}
  } catch(...) {}
  std::fputs("POLICY_FAILED\n",stderr);return 4;
}
}  // namespace ow::control
