// SPDX-License-Identifier: Apache-2.0
// Test host uses public typed construction and production remote callbacks only.
#include <omniweft/control.hpp>
#include <mutex>
int main() {
  ow::world::World world("workshop",7,8);
  ow::commands::Envelope e;
  e.world_id="workshop"; e.transaction_id="018f7242-4387-7c98-a114-67787915a399";
  e.idempotency={"fixture-epoch",1}; e.apply_at={"next_tick",120}; e.budget={3,0};
  e.operations={ow::commands::EntityCreate{"P","builtin.unit_cube"},
    ow::commands::EntityCreate{"C","builtin.unit_cube"},
    ow::commands::EntityReparent{ow::commands::TemporaryTarget{"C"},ow::commands::TemporaryTarget{"P"},"preserve_world"}};
  if (ow::transactions::Coordinator::apply_at_boundary(world,e).status!="committed") return 2;
  ow::control::Config config; config.max_slots=8;
  ow::control::Callbacks callbacks;
  std::timed_mutex owner;
  callbacks.at_boundary=[&](std::function<void()> action, std::chrono::steady_clock::time_point deadline, bool) {
    std::unique_lock lock(owner,std::defer_lock);
    if (!lock.try_lock_until(deadline) || std::chrono::steady_clock::now() >= deadline) return false;
    action();  // Synchronous completion, including exception propagation.
    return true;
  };
  callbacks.snapshot=[&] { return world.snapshot(); };
  callbacks.runtime_status=[&] { ow::control::RuntimeStatus r; r.snapshot=world.snapshot(); return r; };
  callbacks.apply=[&](const auto& input) { return ow::transactions::Coordinator::apply_at_boundary(world,input); };
#ifdef OW_TEST_POLICY
  ow::policy::Ledger ledger;
  return ow::control::run_policy(config,{callbacks,&ledger,[&](const auto& input, auto& lease) {
    ow::policy::Guard guard(lease);
    return ow::transactions::Coordinator::apply_at_boundary(world,input,&guard);
  }});
#else
  return ow::control::run(config,callbacks);
#endif
}
