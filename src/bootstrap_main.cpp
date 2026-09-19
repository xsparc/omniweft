// SPDX-License-Identifier: Apache-2.0
#if !defined(_M_X64) && !defined(__x86_64__)
#error "Omniweft bootstrap requires an x86_64 compiler target; select x64 build tools."
#endif
#include "protocol_example.hpp"
#include "objects_example.hpp"
#include "render_example.hpp"
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr std::string_view usage =
    "omniweft_examples --example platform.bootstrap --headless --seed 7 "
    "[--verify] --output <new-artifact-directory>\n";

struct Options {
  bool headless = false;
  bool verify = false;
  std::uint32_t seed = 0;
  std::filesystem::path output;
};

Options parse_options(int argc, char* argv[]) {
  Options options;
  bool has_example = false;
  bool has_seed = false;
  bool has_output = false;
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg(argv[index]);
    const auto value = [&]() -> std::string_view {
      if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--")) {
        throw std::invalid_argument("missing value for " + std::string(arg));
      }
      return argv[++index];
    };
    if (arg == "--example" && !has_example) {
      has_example = true;
      if (value() != "platform.bootstrap") {
        throw std::invalid_argument("only --example platform.bootstrap is available");
      }
    } else if (arg == "--headless" && !options.headless) {
      options.headless = true;
    } else if (arg == "--verify" && !options.verify) {
      options.verify = true;
    } else if (arg == "--seed" && !has_seed) {
      has_seed = true;
      const auto seed = value();
      const auto result = std::from_chars(seed.data(), seed.data() + seed.size(), options.seed);
      if (result.ec != std::errc{} || result.ptr != seed.data() + seed.size()) {
        throw std::invalid_argument("--seed must be an unsigned 32-bit decimal integer");
      }
      if (options.seed != 7) {
        throw std::invalid_argument("platform.bootstrap defines one fixture; use --seed 7");
      }
    } else if (arg == "--output" && !has_output) {
      has_output = true;
      const auto output = value();
      if (output.empty()) {
        throw std::invalid_argument("--output must name a new artifact directory");
      }
      options.output = std::filesystem::path(output);
    } else {
      throw std::invalid_argument("unknown or duplicate option: " + std::string(arg));
    }
  }
  if (!has_example || !has_seed || !has_output) {
    throw std::invalid_argument("required options: --example, --seed and --output");
  }
  return options;
}

// This fixture exercises only a native process lifecycle, not a world simulation.
class BootstrapSession {
 public:
  explicit BootstrapSession(std::uint32_t seed) : value_(seed) {}

  void start() {
    require(State::created);
    state_ = State::running;
    events_.emplace_back("running");
  }
  void step() {
    require(State::running);
    // The fixture is one 31-bit LCG step, computed with defined unsigned wraparound.
    value_ = (value_ * std::uint32_t{1103515245} + std::uint32_t{12345}) & 0x7fffffffU;
    ++steps_;
  }
  void stop() {
    require(State::running);
    state_ = State::stopped;
    events_.emplace_back("stopped");
  }
  bool verified() const {
    return state_ == State::stopped && steps_ == 1 && value_ == 1282168116U &&
           events_ == std::vector<std::string>{"created", "running", "stopped"};
  }
  void write_json(std::ostream& output, const Options& options) const {
    output << "{\n  \"schema_version\": 1,\n  \"example\": \"platform.bootstrap\",\n"
           << "  \"mode\": \"headless\",\n  \"seed\": " << options.seed
           << ",\n  \"lifecycle\": [";
    for (std::size_t index = 0; index < events_.size(); ++index) {
      if (index != 0) {
        output << ", ";
      }
      output << '"' << events_[index] << '"';
    }
    output << "],\n  \"steps\": " << steps_ << ",\n  \"fixture_checksum\": " << value_
           << ",\n  \"verified\": " << (options.verify ? "true" : "false") << "\n}\n";
  }

 private:
  enum class State { created, running, stopped };
  void require(State expected) const {
    if (state_ != expected) {
      throw std::logic_error("invalid bootstrap lifecycle transition");
    }
  }
  State state_ = State::created;
  std::uint32_t value_;
  std::uint32_t steps_ = 0;
  std::vector<std::string> events_{"created"};
};

void save_result(const Options& options, const BootstrapSession& session) {
  const auto result = options.output / "result.json";
  const auto temporary = options.output / "result.json.tmp";
  // Claim a fresh directory before writing; competing runs cannot share staging files.
  if (!options.output.parent_path().empty()) {
    std::filesystem::create_directories(options.output.parent_path());
  }
  if (!std::filesystem::create_directory(options.output)) {
    throw std::runtime_error("output directory already exists; choose a new --output directory");
  }
  try {
    std::ofstream stream(temporary, std::ios::binary);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    session.write_json(stream, options);
    stream.close();
    std::filesystem::rename(temporary, result);
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw;
  }
}
}  // namespace

int main(int argc, char* argv[]) {
  for (int index = 1; index + 1 < argc; ++index) {
    if (std::string_view(argv[index]) == "--example" &&
        std::string_view(argv[index + 1]) == "protocol.reject_invalid")
      return run_protocol_example(argc, argv);
    if (std::string_view(argv[index]) == "--example" &&
        (std::string_view(argv[index + 1]) == "objects.atomic" ||
         std::string_view(argv[index + 1]) == "objects.hierarchy"))
      return run_objects_example(argc, argv);
    if (std::string_view(argv[index]) == "--example" &&
        std::string_view(argv[index + 1]) == "render.world_cube")
      return run_render_example(argc, argv);
  }
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    std::cout << usage << "Also available: --example protocol.reject_invalid or objects.atomic with repeatable --input <json-file>.\nRendering: --example render.world_cube --gpu (optional build) or --headless [--interactive].\n";
    return 0;
  }
  Options options;
  try {
    options = parse_options(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "invalid_arguments: " << error.what() << '\n' << usage;
    return 2;
  }
  if (!options.headless) {
    std::cerr << "graphics_unavailable: this build has no graphics backend; rerun with --headless. "
                 "Use --example render.world_cube --gpu with the optional Vulkan build for presentation.\n";
    return 3;
  }
  try {
    BootstrapSession session(options.seed);
    session.start();
    session.step();
    session.stop();
    if (options.verify && !session.verified()) {
      std::cerr << "verification_failed: bootstrap lifecycle or seed-7 fixture differs from the contract\n";
      return 4;
    }
    try {
      save_result(options, session);
    } catch (const std::exception& error) {
      std::cerr << "output_error: " << error.what()
                << "; use a writable, new --output directory and rerun\n";
      return 5;
    }
    std::cout << "platform.bootstrap: stopped after 1 step; result written\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "runtime_error: " << error.what() << '\n';
    return 6;
  }
}
