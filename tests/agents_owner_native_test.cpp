// SPDX-License-Identifier: Apache-2.0
// This harness includes the actual Owner with only its clock alias replaced.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <thread>

struct OwnerTestClock {
  using time_point = std::chrono::steady_clock::time_point;
  static inline time_point origin;
  static inline std::atomic<std::int64_t> elapsed_ns = 0;
  static inline std::atomic<std::uint64_t> owner_calls = 0;
  static inline std::atomic<bool> dispatch_entered = false;
  static inline thread_local bool on_owner = false, signal_dispatch = false;
  static void reset() {
    origin = std::chrono::steady_clock::now();
    elapsed_ns.store(0); owner_calls.store(0); dispatch_entered.store(false);
  }
  static time_point now() {
    if (on_owner) owner_calls.fetch_add(1);
    if (signal_dispatch) {
      // Owner::dispatch first samples Clock while holding its real job mutex.
      // A subsequent dispatch must acquire that mutex after slot installation.
      signal_dispatch = false; dispatch_entered.store(true);
    }
    return origin + std::chrono::nanoseconds(elapsed_ns.load());
  }
  static void advance(std::int64_t nanoseconds) { elapsed_ns.fetch_add(nanoseconds); }
};

#define main omniweft_agents_entry_for_owner_test
#include "../src/agents_main.cpp"
#undef main

namespace owner_test {
using RealClock = std::chrono::steady_clock;
using namespace std::chrono_literals;
std::uint64_t assertions = 0;
struct TestFailure { const char* label; };
void require(bool condition, const char* label) {
  ++assertions;
  if (!condition) throw TestFailure{label};
}
template<class Predicate>
void until(Predicate predicate, const char* label, std::chrono::milliseconds timeout = 750ms) {
  const auto deadline = RealClock::now() + timeout;
  while (!predicate() && RealClock::now() < deadline) std::this_thread::sleep_for(1ms);
  require(predicate(), label);
}
class RealWatchdog {
 public:
  RealWatchdog() : thread_([this] {
    std::unique_lock lock(mutex_);
    if (!changed_.wait_for(lock, 8s, [this] { return done_; })) {
      std::fputs("{\"status\":\"failed\",\"case_set\":\"owner-dispatch-v1\",\"assertion\":\"real watchdog expired\"}\n", stdout);
      std::fflush(stdout); std::_Exit(1);
    }
  }) {}
  ~RealWatchdog() {
    { std::lock_guard lock(mutex_); done_ = true; }
    changed_.notify_one(); thread_.join();
  }
 private:
  std::mutex mutex_;
  std::condition_variable changed_;
  bool done_ = false;
  std::thread thread_;
};
class RunningOwner {
 public:
  std::atomic<bool> stop = false;
  Owner owner;
  std::thread simulation;
  RunningOwner() : owner(configuration(), RealClock::now() + 4s, stop, false), simulation([this] {
    OwnerTestClock::on_owner = true; owner.run();
  }) {}
  ~RunningOwner() { stop.store(true); owner.wake(); simulation.join(); }
  static ow::control::Config configuration() {
    ow::control::Config result; result.max_slots = 8; return result;
  }
};
class Pending {
 public:
  std::atomic<bool> finished = false, accepted = false, exception = false;
  std::shared_ptr<const Published> acknowledged;
  std::thread caller;
  Pending(Owner& owner, RealClock::time_point deadline, std::function<void()> action) {
    OwnerTestClock::dispatch_entered.store(false);
    caller = std::thread([this, &owner, deadline, action = std::move(action)] {
      OwnerTestClock::signal_dispatch = true;
      try {
        const auto outcome = owner.dispatch(action, deadline, true);
        acknowledged = owner.latest();
        accepted.store(outcome);
      }
      catch (...) { exception.store(true); }
      finished.store(true);
    });
  }
  ~Pending() { caller.join(); }
};
ow::commands::Envelope create_envelope() {
  const auto parsed = ow::commands::parse(R"({"protocol_version":"0.1","world_id":"workshop",
    "transaction_id":"018f7242-4387-7c98-a114-67787915a612",
    "idempotency":{"epoch":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","sequence":1},
    "expected_world_revision":0,"apply_at":{"mode":"next_tick","expires_after_ticks":120},
    "budget":{"max_operations":1,"max_blob_bytes":0},
    "operations":[{"type":"entity.create","temporary_id":"cube","prefab":"builtin.unit_cube"}]})");
  require(parsed.envelope.has_value(), "literal typed create envelope parses");
  return *parsed.envelope;
}
void initial_poll(Owner& owner) {
  bool called = false;
  require(owner.dispatch([&] { called = true; }, RealClock::now() + 500ms, false) && called,
          "actual owner services a read while controlled time is frozen");
  require(owner.latest()->clock.simulation_tick == 0, "initial frozen owner has zero actual ticks");
}
void pending_slot(Owner& owner) {
  until([] { return OwnerTestClock::dispatch_entered.load(); }, "authoring caller enters actual dispatch mutex");
  bool called = false;
  require(!owner.dispatch([&] { called = true; }, RealClock::now() + 500ms, false) && !called,
          "authoring pending slot rejects a second dispatch without side effects");
}
void frozen_polls() {
  const auto before = OwnerTestClock::owner_calls.load();
  // After a final tick publication, twenty new owner clock reads require the
  // actual loop to revisit its frozen-time zero-due advances several times.
  until([&] { return OwnerTestClock::owner_calls.load() >= before + 20; },
        "actual owner continues polling while controlled time is frozen");
}
void overload_then_admit() {
  OwnerTestClock::reset();
  RunningOwner running;
  initial_poll(running.owner);
  const auto envelope = create_envelope();
  const auto callbacks = running.owner.callbacks();
  std::atomic<unsigned> applied = 0;
  Pending pending(running.owner, RealClock::now() + 2s, [&] {
    const auto receipt = callbacks.apply(envelope);
    if (receipt.status != "committed") throw std::runtime_error("typed create rejected");
    applied.fetch_add(1);
  });
  pending_slot(running.owner);
  frozen_polls();
  require(applied.load() == 0 && !pending.finished.load() &&
          running.owner.latest()->clock.simulation_tick == 0,
          "zero-due polls cannot admit pending authoring");
  OwnerTestClock::advance(100000000); running.owner.wake();
  until([&] { return running.owner.latest()->clock.simulation_tick == 4; },
        "100ms overload executes exactly four actual owner ticks");
  const auto overloaded = running.owner.latest();
  require(overloaded->clock.overload_count == 1 && overloaded->clock.dropped_ticks == 2 &&
          overloaded->clock.remainder_units == 0, "actual overload drops two whole ticks with zero remainder");
  require(applied.load() == 0 && !pending.finished.load() && overloaded->snapshot.world_revision == 0 &&
          overloaded->snapshot.slots.empty(), "no callback in an overloaded advance admits authoring");
  frozen_polls();
  require(applied.load() == 0 && !pending.finished.load() &&
          running.owner.latest()->clock == overloaded->clock,
          "post-overload zero-due polls preserve deferred authoring and published clock");
  OwnerTestClock::advance(16666667); running.owner.wake();
  until([&] { return pending.finished.load(); }, "next non-overloaded tick completes pending authoring");
  const auto committed = pending.acknowledged;
  require(pending.accepted.load() && !pending.exception.load() && applied.load() == 1,
          "next normal actual tick admits the typed create exactly once");
  require(committed != nullptr, "dispatch caller captures its publication before signaling completion" );
  require(committed->clock.simulation_tick == 5 && committed->clock.overload_count == 1 &&
          committed->clock.dropped_ticks == 2 && committed->clock.remainder_units == 20,
          "normal recovery publication has exact rational clock state");
  require(committed->snapshot.world_revision == 1 && committed->snapshot.slots.size() == 1 &&
          committed->snapshot.slots[0].entity.has_value(), "acknowledged typed create has its full detached publication");
  OwnerTestClock::advance(16666667); running.owner.wake();
  until([&] { return running.owner.latest()->clock.simulation_tick == 6; }, "owner continues after admitted authoring");
  require(applied.load() == 1 && running.owner.latest()->snapshot.world_revision == 1,
          "later actual ticks never replay the completed authoring job");
  require(!running.owner.failed(), "overload recovery owner remains healthy");
}
void cancellation_before_claim() {
  OwnerTestClock::reset();
  RunningOwner running;
  initial_poll(running.owner);
  const auto envelope = create_envelope();
  const auto callbacks = running.owner.callbacks();
  std::atomic<unsigned> applied = 0;
  Pending pending(running.owner, RealClock::now() + 250ms, [&] {
    static_cast<void>(callbacks.apply(envelope)); applied.fetch_add(1);
  });
  pending_slot(running.owner);
  until([&] { return pending.finished.load(); }, "real request deadline cancels the frozen pending job");
  require(!pending.accepted.load() && !pending.exception.load() && applied.load() == 0,
          "cancellation before claim returns false without running captured authoring");
  require(running.owner.latest()->clock.simulation_tick == 0 &&
          running.owner.latest()->snapshot.world_revision == 0, "canceled job preserves initial authoring and clock");
  OwnerTestClock::advance(16666667); running.owner.wake();
  until([&] { return running.owner.latest()->clock.simulation_tick == 1; }, "owner executes a future tick after cancellation");
  frozen_polls();
  require(applied.load() == 0 && running.owner.latest()->snapshot.world_revision == 0 &&
          running.owner.latest()->snapshot.slots.empty(), "canceled captures never apply on a future actual tick");
  bool read = false;
  require(running.owner.dispatch([&] { read = true; }, RealClock::now() + 500ms, false) && read,
          "canceled slot is released for later owner work");
  require(!running.owner.failed(), "cancellation recovery owner remains healthy");
}
}  // namespace owner_test
int main() {
  owner_test::RealWatchdog watchdog;
  try {
    owner_test::overload_then_admit(); owner_test::cancellation_before_claim();
    std::printf("{\"status\":\"passed\",\"case_set\":\"owner-dispatch-v1\",\"assertions\":%llu}\n",
                static_cast<unsigned long long>(owner_test::assertions));
    return 0;
  } catch (const owner_test::TestFailure& error) {
    std::printf("{\"status\":\"failed\",\"case_set\":\"owner-dispatch-v1\",\"assertion\":\"%s\"}\n", error.label);
  } catch (...) {
    std::fputs("{\"status\":\"failed\",\"case_set\":\"owner-dispatch-v1\",\"assertion\":\"unexpected test exception\"}\n", stdout);
  }
  return 1;
}