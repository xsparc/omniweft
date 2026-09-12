// SPDX-License-Identifier: Apache-2.0
// Uses only public construction, typed coordinator calls, and detached observations.
#include <omniweft/commands.hpp>
#include <omniweft/transactions.hpp>
#include <omniweft/world.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace {
unsigned checks = 0;
void require(bool value, const std::string& message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
ow::commands::Envelope envelope(std::uint64_t revision, std::vector<ow::commands::Operation> operations) {
    ow::commands::Envelope value;
    value.protocol_version = "0.1";
    value.world_id = "workshop";
    value.transaction_id = "018f7242-4387-7c98-a114-67787915a399";
    value.idempotency = {"fixture-epoch", 1};
    value.expected_world_revision = revision;
    value.apply_at = {"next_tick", 120};
    value.budget = {static_cast<std::uint64_t>(operations.size()), 0};
    value.operations = std::move(operations);
    return value;
}
ow::commands::TransformSet move(ow::commands::Target target, std::array<double, 3> position) {
    ow::commands::TransformSet value;
    value.target = std::move(target);
    value.position_m = position;
    return value;
}
}

int main() {
    using namespace ow::commands;
    using ow::transactions::Coordinator;
    using ow::world::World;
    try {
        for (const std::uint32_t capacity : {0U, 1025U}) {
            bool rejected = false;
            try { World invalid("workshop", 7, capacity); }
            catch (const std::exception&) { rejected = true; }
            require(rejected, "invalid host capacity must fail construction");
        }
        for (const std::string& name : {std::string{}, std::string("not a world"), std::string(129, 'a')}) {
            bool rejected = false;
            try { World invalid(name, 7, 4); }
            catch (const std::exception&) { rejected = true; }
            require(rejected, "invalid host world ID must fail construction");
        }
        World upper("workshop", 0, 1024);
        require(upper.snapshot().max_slots == 1024 && upper.snapshot().slots.empty(),
                "maximum legal capacity is usable without fabricated slots");
        World world("workshop", 7, 4);
        const auto initial = world.snapshot();
        require(initial.world_revision == 0 && initial.slots.empty(), "world begins empty at revision zero");
        const auto first = Coordinator::apply_at_boundary(world,
            envelope(0, {EntityCreate{"A", "builtin.unit_cube"}, move(TemporaryTarget{"A"}, {1, 2, 3})}));
        require(first.status == "committed" && first.world_revision == 1 && first.created.size() == 1,
                "public typed create-transform commits once");
        const auto captured = world.snapshot();
        require(captured.slots.size() == 1 && captured.slots[0].entity.has_value(), "created entity is observed");
        require(captured.slots[0].entity_uuid == "00000007-0000-4000-8000-000000000001" &&
                captured.slots[0].generation == 1 && !captured.slots[0].retired,
                "literal first identity and generation");
        require(captured.slots[0].entity->authoring_revision == 1 &&
                captured.slots[0].entity->transform.position_m == std::array<double,3>{1,2,3},
                "literal authored transform and resource revision");
        auto detached = world.snapshot();
        detached.slots[0].entity->transform.position_m[0] = 999;
        detached.slots[0].generation = 99;
        detached.world_revision = 99;
        require(world.snapshot() == captured, "editing a detached observation cannot mutate world state");
        const EntityTarget durable{"workshop", "00000007-0000-4000-8000-000000000001", 1};
        const auto changed = Coordinator::apply_at_boundary(world, envelope(1, {move(durable, {2,3,4})}));
        require(changed.status == "committed" && changed.world_revision == 2, "second authoring commit");
        require(captured.world_revision == 1 &&
                captured.slots[0].entity->authoring_revision == 1 &&
                captured.slots[0].entity->transform.position_m == std::array<double,3>{1,2,3},
                "earlier snapshot remains a deep value after later world mutation");
        const auto before = world.snapshot();
        const auto bytes_before = world.canonical_bytes();
        const auto failed = Coordinator::apply_at_boundary(world, envelope(2,
            {move(durable, {99,98,97}), EntityCreate{"B","builtin.unit_cube"}, move(TemporaryTarget{"missing"},{0,0,0})}));
        require(world.snapshot() == before, "staged-prefix-rollback: entire native snapshot unchanged");
        require(world.canonical_bytes() == bytes_before, "staged-prefix-rollback: native bytes unchanged");
        require(failed.status == "rejected" && failed.created.empty() && failed.world_revision == 2 &&
                failed.errors.size() == 1 && failed.errors[0].code == "NOT_FOUND",
                "rejected typed receipt has no committed creation mapping");

        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                                     std::numeric_limits<double>::infinity(),
                                     -std::numeric_limits<double>::infinity()}) {
            auto nonfinite = move(durable, {invalid,0,0});
            const auto result = Coordinator::apply_at_boundary(world,
                envelope(2, {EntityCreate{"B","builtin.unit_cube"}, nonfinite}));
            require(result.status == "rejected" && result.errors.size() == 1 &&
                    result.errors[0].code == "NONFINITE_VALUE" && result.created.empty(),
                    "coordinator independently rejects nonfinite typed input");
            require(world.snapshot() == before && world.canonical_bytes() == bytes_before,
                    "nonfinite typed input changes no authoritative state");
        }
        const auto recovery = Coordinator::apply_at_boundary(world,
            envelope(2, {EntityCreate{"B","builtin.unit_cube"}}));
        require(recovery.status == "committed" && recovery.created.size() == 1 &&
                recovery.created[0].entity_uuid == "00000007-0000-4000-8000-000000000002" &&
                recovery.created[0].generation == 1 && recovery.world_revision == 3,
                "typed recovery uses the unconsumed next identity");

        World limited("workshop", 7, 1);
        const auto limited_before = limited.snapshot();
        const auto capacity = Coordinator::apply_at_boundary(limited,
            envelope(0, {EntityCreate{"A","builtin.unit_cube"}, EntityCreate{"B","builtin.unit_cube"}}));
        require(capacity.status == "rejected" && capacity.errors.size() == 1 &&
                capacity.errors[0].code == "BUDGET_EXCEEDED", "real host capacity rejects staged excess");
        require(limited.snapshot() == limited_before, "capacity failure preserves all allocation state");
        std::cout << "{\"status\":\"passed\",\"assertions\":" << checks
                  << ",\"uint64_exhaustion\":\"source_review_only\"}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "objects_native_test failed: " << error.what() << '\n';
        return 1;
    }
}
