// SPDX-License-Identifier: Apache-2.0
// Independent literal 60 Hz cases; expectations do not call the clock algorithm.
#include "omniweft/simulation.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::uint64_t assertions = 0;
struct Failure { std::string label; };
void require(bool condition, const std::string& label) {
  ++assertions;
  if (!condition) throw Failure{label};
}
struct Case {
  std::uint64_t elapsed, due, executed, dropped, remainder;
  bool overloaded;
  const char* label;
};
void apply_case(ow::simulation::FixedStepper& clock, const Case& expected,
                std::uint64_t prior_tick, std::uint64_t prior_overloads,
                std::uint64_t prior_dropped) {
  std::vector<std::uint64_t> called;
  const auto batch = clock.advance(expected.elapsed, [&](std::uint64_t ordinal) { called.push_back(ordinal); });
  const std::string label(expected.label);
  // Check actual callback effects first: a false report cannot hide an extra step.
  require(called.size() == expected.executed, label + ": executed callback count");
  for (std::size_t i = 0; i < called.size(); ++i)
    require(called[i] == prior_tick + i + 1, label + ": actual cumulative callback ordinal");
  require(batch.due_ticks == expected.due, label + ": exact due steps");
  require(batch.executed_ticks == expected.executed, label + ": exact executed steps");
  require(batch.dropped_ticks == expected.dropped, label + ": exact dropped steps");
  require(batch.remainder_units == expected.remainder, label + ": exact rational remainder");
  require(batch.overloaded == expected.overloaded, label + ": overload indication");
  const auto stats = clock.stats();
  require(stats.simulation_tick == prior_tick + expected.executed, label + ": ticks count only executed callbacks");
  require(stats.overload_count == prior_overloads + (expected.overloaded ? 1U : 0U), label + ": cumulative overload events");
  require(stats.dropped_ticks == prior_dropped + expected.dropped, label + ": cumulative discarded whole steps");
  require(stats.remainder_units == expected.remainder, label + ": retained fractional phase");
}
void fresh_cases() {
  const Case cases[] = {
    {0, 0, 0, 0, 0, false, "zero elapsed"},
    {16666666, 0, 0, 0, 999999960, false, "below one step"},
    {16666667, 1, 1, 0, 20, false, "above one step"},
    {83333333, 4, 4, 0, 999999980, false, "four-step boundary"},
    {83333334, 5, 4, 1, 40, true, "clock cap"},
    {1000000000, 60, 4, 56, 0, true, "one-second overload"},
    {std::numeric_limits<std::uint64_t>::max(), 1106804644422ULL, 4, 1106804644418ULL,
     573096900, true, "maximum elapsed integer"},
  };
  for (const auto& item : cases) {
    ow::simulation::FixedStepper clock;
    require(clock.stats() == ow::simulation::StepStats{}, "fresh clock has no hidden debt");
    apply_case(clock, item, 0, 0, 0);
  }
}
void fractional_carry() {
  ow::simulation::FixedStepper clock;
  apply_case(clock, {16666666, 0, 0, 0, 999999960, false, "carry first fragment"}, 0, 0, 0);
  apply_case(clock, {1, 1, 1, 0, 20, false, "carry crosses first boundary"}, 0, 0, 0);
  apply_case(clock, {16666666, 0, 0, 0, 999999980, false, "carry second fragment"}, 1, 0, 0);
  apply_case(clock, {1, 1, 1, 0, 40, false, "carry crosses second boundary"}, 1, 0, 0);
}
void overload_recovery() {
  ow::simulation::FixedStepper clock;
  apply_case(clock, {1000000000, 60, 4, 56, 0, true, "recovery overload"}, 0, 0, 0);
  apply_case(clock, {16666667, 1, 1, 0, 20, false, "recovery next normal step"}, 4, 1, 56);
  apply_case(clock, {0, 0, 0, 0, 20, false, "recovery no hidden whole-step debt"}, 5, 1, 56);
  ow::simulation::FixedStepper fraction;
  apply_case(fraction, {333333337, 20, 4, 16, 220, true, "overload retains fractional time"}, 0, 0, 0);
  apply_case(fraction, {16666663, 1, 1, 0, 0, false, "fractional overload recovery"}, 4, 1, 16);
}
void invalid_callback_recovery() {
  ow::simulation::FixedStepper clock;
  apply_case(clock, {16666666, 0, 0, 0, 999999960, false, "invalid callback prior state"}, 0, 0, 0);
  const auto before = clock.stats();
  bool rejected = false;
  try {
    static_cast<void>(clock.advance(1000000000, std::function<void(std::uint64_t)>{}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "empty callback is rejected");
  require(clock.stats() == before, "empty callback preserves all clock state");
  apply_case(clock, {1, 1, 1, 0, 20, false, "recovery after empty callback"}, 0, 0, 0);
}
}  // namespace

int main() {
  try {
    fresh_cases();
    fractional_carry();
    overload_recovery();
    invalid_callback_recovery();
    std::printf("{\"status\":\"passed\",\"case_set\":\"fixed-clock-v1\",\"assertions\":%llu}\n",
                static_cast<unsigned long long>(assertions));
    return 0;
  } catch (const Failure& error) {
    // Only labels authored above reach this message; no OS paths or process data.
    std::fprintf(stderr, "simulation oracle failed: %s\n", error.label.c_str());
    return 1;
  } catch (...) {
    std::fputs("simulation oracle failed: unexpected exception\n", stderr);
    return 1;
  }
}
