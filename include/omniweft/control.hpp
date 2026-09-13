// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/commands.hpp"
#include "omniweft/transactions.hpp"
#include "omniweft/world.hpp"
#include <cstdint>
#include <functional>
#include <string>

namespace ow::control {
// The bounded initial host serves one fixed fixture world.
struct Config {
  std::string world_id = "workshop";
  std::uint32_t seed = 7;
  std::uint32_t max_slots = 1024;
  std::uint32_t session_ttl_ms = 30000;
  std::uint32_t max_runtime_ms = 60000;
  std::uint32_t max_requests = 1024;
};
struct Callbacks {
  std::function<world::Snapshot()> snapshot;
  std::function<transactions::Receipt(const commands::Envelope&)> apply;
};

// Invokes callbacks synchronously on this calling owner thread only.
// Requires private stdin/stdout pipes; descriptors are the only stdout output.
// Returns 0 for stop/EOF/bounded completion, 3 for unavailable private pipes,
// or 4 for failure. The hard-runtime watchdog exits with 4 on a stalled host.
int run(const Config& config, const Callbacks& callbacks) noexcept;
}  // namespace ow::control
