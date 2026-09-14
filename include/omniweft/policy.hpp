// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/transactions.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>

namespace ow::policy {
inline constexpr std::size_t principal_count = 2, slot_count = 8;
inline constexpr std::size_t body_limit = 16384, operation_limit = 4;
inline constexpr std::size_t skeleton_charge = 128, cube_charge = 256;
struct Grant {
  std::string_view principal;
  std::array<double, 3> lower, upper;
  std::size_t retained_limit, working_limit, observation_limit;
};
const Grant& grant(std::size_t principal);
std::size_t working_charge(std::size_t declared_bytes);
struct Usage {
  std::array<std::size_t, principal_count> retained{}, working{}, requests{};
  std::size_t global_requests = 0;
  std::uint64_t world_revision = 0;
  bool operator==(const Usage&) const = default;
};
struct Denied {
  const char* code;
  const char* path;
};
class Guard;
class Ledger;
// Admission is unforgeable outside Ledger and remains alive through transport
// completion. Moving transfers the refund obligation; destruction refunds once.
class Lease {
 public:
  ~Lease();
  Lease(Lease&& other) noexcept;
  Lease& operator=(Lease&&) = delete;
  Lease(const Lease&) = delete;
  Lease& operator=(const Lease&) = delete;
 private:
  Lease(Ledger& ledger, std::size_t principal, std::size_t bytes) noexcept;
  Ledger* ledger_;
  std::size_t principal_, bytes_;
  bool claimed_ = false;
  friend class Ledger;
  friend class Guard;
};
class Ledger {
 public:
  explicit Ledger(std::size_t global_limit = 2);
  Lease admit(std::size_t principal, std::size_t declared_bytes);
  Usage usage() const;
  Ledger(const Ledger&) = delete;
  Ledger& operator=(const Ledger&) = delete;
 private:
  mutable std::mutex mutex_;
  Usage usage_;
  std::array<int, slot_count> sponsors_;
  std::array<bool, slot_count> live_{};
  const world::World* world_ = nullptr;
  std::size_t global_limit_;
  friend class Lease;
  friend class Guard;
};
// Only the fixture's trusted native host can construct this boundary. All
// untrusted requests reach it through authenticated per-principal admission.
class Guard final : public transactions::StagingGuard {
 public:
  explicit Guard(Lease& lease);
  void begin(const world::World&, const world::Snapshot&, const commands::Envelope&) override;
  void create(std::size_t slot, std::size_t operation) override;
  void transform(std::size_t slot, const world::Transform& before,
                 const world::Transform& after, std::size_t operation) override;
  void erase(std::size_t slot, const world::Transform& before, std::size_t operation) override;
  void finish(const world::Snapshot&) override;
  void commit() noexcept override;
 private:
  Lease& lease_;
  std::unique_lock<std::mutex> lock_;
  std::array<int, slot_count> sponsors_{};
  std::array<bool, slot_count> live_{}, unplaced_{};
  std::array<std::size_t, principal_count> retained_{};
  bool begun_ = false, finished_ = false;
  void account(std::size_t operation);
  void scope(const world::Transform&, std::size_t operation) const;
};
// Independent native admission computes a bounded charge from typed fields.
// It never accepts a caller-supplied working charge or bypasses the guard.
transactions::Receipt apply(world::World&, const commands::Envelope&, Ledger&, std::size_t principal);
}  // namespace ow::policy
