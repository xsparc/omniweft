// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/transactions.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>

namespace ow::persistence {
inline constexpr std::size_t commit_limit=8, operation_limit=4, receipt_limit=4;
inline constexpr std::uint64_t maximum_quota=8U*1024U*1024U;
struct Config {
  std::string world_id="workshop";
  std::uint32_t seed=7, max_slots=8;
  std::uint64_t byte_quota=4U*1024U*1024U;
};
enum class Phase {
  before_blob_flush, before_journal_flush, after_journal_flush,
  before_ack, after_ack, before_checkpoint_flush, after_checkpoint_flush,
  before_checkpoint_replace, after_checkpoint_replace,
  before_journal_reclaim, after_journal_reclaim
};
// Trusted owner-only fault injection, never supplied by a stored package/client.
using FaultHook=std::function<void(Phase)>;
class Failure final:public std::runtime_error {
 public:
  explicit Failure(const char* code):std::runtime_error(code) {}
  const char* code() const noexcept {return what();}
};
struct Result {
  std::optional<transactions::Receipt> receipt;
  bool replayed=false;
  std::uint64_t durable_revision=0, high_water=0, low_water=1;
  bool recovery_required=false;
  std::string code;
};
// Opt-in, single-owner native store. Existing network hosts remain volatile.
// Construction/recovery errors throw fixed-code Failure. Ambiguous writes seal
// the store; destroy and reopen before further use. No silent older-state fallback.
class Store {
 public:
  static std::unique_ptr<Store> create(const std::filesystem::path&, const Config& = {}, FaultHook = {});
  static std::unique_ptr<Store> open(const std::filesystem::path&, FaultHook={});
  ~Store();
  Store(const Store&)=delete;
  Store& operator=(const Store&)=delete;
  world::Snapshot snapshot() const;
  const std::string& epoch() const noexcept; // Private capability domain: never export.
  std::uint64_t durable_revision() const noexcept;
  std::uint64_t high_water() const noexcept;
  std::uint64_t low_water() const noexcept;
  bool recovery_required() const noexcept;
  Result apply_await_durable(const commands::Envelope&, std::uint64_t boundary_tick);
  Result lookup(std::uint64_t sequence) const;
  Result compact();
  // Call only after an owner actually delivered/recorded a successful receipt.
  void notify_acknowledged();
 private:
  struct Impl;
  explicit Store(std::unique_ptr<Impl>);
  std::unique_ptr<Impl> impl_;
};
} // namespace ow::persistence
