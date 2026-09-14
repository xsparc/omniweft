// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <functional>
namespace ow::simulation {
struct StepStats {
  std::uint64_t simulation_tick = 0, overload_count = 0, dropped_ticks = 0, remainder_units = 0;
  bool operator==(const StepStats&) const = default;
};
struct StepBatch {
  std::uint64_t due_ticks = 0, executed_ticks = 0, dropped_ticks = 0, remainder_units = 0;
  bool overloaded = false;
  bool operator==(const StepBatch&) const = default;
};
class FixedStepper {
 public:
  // elapsed_ns supports the full uint64 domain. Overflow of cumulative counters
  // rejects before callbacks or state changes. A throwing callback is fatal to
  // its caller; there is no rollback promise for callback-owned external state.
  StepBatch advance(std::uint64_t elapsed_ns, const std::function<void(std::uint64_t)>& on_step);
  StepStats stats() const noexcept { return stats_; }
 private:
  StepStats stats_;
};
}  // namespace ow::simulation
