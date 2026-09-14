// SPDX-License-Identifier: Apache-2.0
#include "omniweft/simulation.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace ow::simulation {
namespace {
constexpr std::uint64_t catch_up_cap = 4;
constexpr std::uint64_t phase_units = 1000000000;
}
StepBatch FixedStepper::advance(std::uint64_t elapsed_ns, const std::function<void(std::uint64_t)>& on_step) {
  if (!on_step) throw std::invalid_argument("A fixed-step callback is required.");
  const auto partial = stats_.remainder_units + (elapsed_ns % phase_units) * 60;
  const auto due = (elapsed_ns / phase_units) * 60 + partial / phase_units;
  StepBatch batch{due, std::min(due, catch_up_cap), due > catch_up_cap ? due - catch_up_cap : 0,
                  partial % phase_units, due > catch_up_cap};
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  if (stats_.simulation_tick > maximum - batch.executed_ticks ||
      stats_.dropped_ticks > maximum - batch.dropped_ticks ||
      (batch.overloaded && stats_.overload_count == maximum))
    throw std::overflow_error("Fixed-step cumulative counter exhausted.");
  for (std::uint64_t index = 0; index < batch.executed_ticks; ++index) {
    on_step(stats_.simulation_tick + 1);
    ++stats_.simulation_tick;
  }
  stats_.dropped_ticks += batch.dropped_ticks;
  stats_.overload_count += batch.overloaded ? 1U : 0U;
  stats_.remainder_units = batch.remainder_units;
  return batch;
}
}  // namespace ow::simulation
