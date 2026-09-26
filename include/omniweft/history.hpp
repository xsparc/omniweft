// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/transactions.hpp"
#include <array>
#include <vector>

namespace ow::history {
inline constexpr std::size_t capacity=8, operation_limit=4, envelope_limit=16384;
enum class Action { undo, redo, rewind_simulation };
struct State {
  std::size_t entries=0, cursor=0;
  std::uint64_t expected_revision=0;
  bool operator==(const State&) const = default;
};
// Trusted single-owner native boundary, not authentication or retry admission.
// World must outlive this history. Outside edits require explicit reconciliation.
class History {
 public:
  explicit History(world::World& world);
  History(const History&)=delete;
  History& operator=(const History&)=delete;
  transactions::Receipt apply_at_boundary(const commands::Envelope&, transactions::StagingGuard* = nullptr);
  // Request contains headers/budget and no operations; inverse operations are private.
  transactions::Receipt navigate_at_boundary(Action, const commands::Envelope&, transactions::StagingGuard* = nullptr);
  void clear_at_boundary();
  State state() const noexcept {return state_;}
 private:
  struct Entry {std::vector<commands::Operation> forward, inverse;};
  world::World& world_;
  std::array<Entry,capacity> entries_{};
  State state_;
};
} // namespace ow::history
