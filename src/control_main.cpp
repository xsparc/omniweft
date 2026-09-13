// SPDX-License-Identifier: Apache-2.0
#include "omniweft/control.hpp"
#include <charconv>
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace {
ow::control::Config options(int argc, char* argv[]) {
  if (argc > 13 || argc % 2 == 0) throw std::invalid_argument("INVALID_CLI");
  ow::control::Config result;
  std::unordered_set<std::string_view> seen;
  for (int index = 1; index < argc; index += 2) {
    const std::string_view key(argv[index]), value(argv[index + 1]);
    if (key.size() > 32 || value.empty() || value.size() > 32 || !seen.insert(key).second)
      throw std::invalid_argument("INVALID_CLI");
    if (key == "--world") {
      if (value != "workshop") throw std::invalid_argument("INVALID_CLI");
      continue;
    }
    std::uint32_t number = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
      throw std::invalid_argument("INVALID_CLI");
    if (key == "--seed" && number == 7) result.seed = number;
    else if (key == "--max-slots" && number >= 1 && number <= 1024) result.max_slots = number;
    else if (key == "--session-ttl-ms" && number >= 50 && number <= 300000) result.session_ttl_ms = number;
    else if (key == "--max-runtime-ms" && number >= 1000 && number <= 600000) result.max_runtime_ms = number;
    else if (key == "--max-requests" && number >= 1 && number <= 4096) result.max_requests = number;
    else throw std::invalid_argument("INVALID_CLI");
  }
  return result;
}
}  // namespace

int main(int argc, char* argv[]) {
  try {
    const auto config = options(argc, argv);
    ow::world::World world(config.world_id, config.seed, config.max_slots);
    return ow::control::run(config, {
      [&world] { return world.snapshot(); },
      [&world](const ow::commands::Envelope& envelope) {
        return ow::transactions::Coordinator::apply_at_boundary(world, envelope);
      }});
  } catch (...) {
    std::fputs("CONTROL_INVALID_CLI\n", stderr);
    return 2;
  }
}
