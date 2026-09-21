// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/commands.hpp"
#include "omniweft/policy.hpp"
#include "omniweft/transactions.hpp"
#include "omniweft/world.hpp"
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
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
  bool retained_retries = false; // Explicit policy.retry.v1 host opt-in only.
};
struct PresentationStatus {
  bool enabled = false, ready = false;
  std::uint64_t frame_count = 0, world_revision = 0, snapshot_sequence = 0;
};
struct RuntimeStatus {
  std::uint64_t simulation_tick = 0, snapshot_sequence = 1, overload_count = 0, dropped_ticks = 0, remainder_units = 0;
  world::Snapshot snapshot;
  PresentationStatus presentation;
};
struct Callbacks {
  std::function<world::Snapshot()> snapshot;
  std::function<transactions::Receipt(const commands::Envelope&)> apply;
  // Synchronously dispatch onto an owner. False means canceled before claim;
  // true means finished. Claimed tasks must finish before returning, including
  // exception propagation. The gateway never accesses session state concurrently.
  std::function<bool(std::function<void()>, std::chrono::steady_clock::time_point, bool)> at_boundary = {};
  std::function<RuntimeStatus()> runtime_status = {};
  std::function<bool()> should_stop = {};
  std::optional<std::chrono::steady_clock::time_point> normal_deadline = {};
};

// Invokes callbacks inline unless an explicit synchronous owner dispatcher is supplied.
// Requires private stdin/stdout pipes; descriptors are the only stdout output.
// Returns 0 for stop/EOF/bounded completion, 3 for unavailable private pipes,
// or 4 for failure. Normal I/O/admission stops at max_runtime_ms. The watchdog
// exits with 4 on a stalled host after a fixed additional 1000ms cleanup grace.
int run(const Config& config, const Callbacks& callbacks) noexcept;
// Opt-in fixed two-principal profile; no legacy descriptor/capability changes.
struct PolicyCallbacks {
  Callbacks shared;
  policy::Ledger* ledger = nullptr;
  std::function<transactions::Receipt(const commands::Envelope&, policy::Lease&)> apply;
};
int run_policy(const Config& config, const PolicyCallbacks& callbacks) noexcept;
}  // namespace ow::control
