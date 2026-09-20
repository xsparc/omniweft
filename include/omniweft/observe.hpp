// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/world.hpp"
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ow::observe {
struct Bounds {
  std::array<double,3> lower{}, upper{};
  bool operator==(const Bounds&) const = default;
};
struct ReadIdentity {
  std::string entity_uuid;
  std::uint64_t generation=0;
  bool operator==(const ReadIdentity&) const = default;
};
// Trusted host configuration, never a field accepted from an untrusted query.
// Default empty allowlist denies all. World identity is the exact Session World.
struct ReadGrant {
  bool whole_world=false;
  std::vector<ReadIdentity> entities;
  std::optional<Bounds> region;
  std::size_t response_bytes=4096, retained_bytes=1048576;
  std::chrono::milliseconds lifetime{60000}, snapshot_lifetime{30000};
};
struct Query {
  std::vector<std::string> all_tags;
  std::optional<Bounds> intersects;
  std::size_t page_size=16, response_bytes=4096;
};
class Session;
// Native opaque handle. It owns no snapshot data and has no wire representation.
class Cursor {
 public:
  Cursor() = default;
 private:
  friend class Session;
  std::weak_ptr<const int> owner_;
  std::uint64_t snapshot_=0;
  std::size_t offset_=0;
};
struct Page { std::string json; std::optional<Cursor> next; };
struct Result { std::optional<Page> page; std::string code; };
// Single-owner API, like World/Coordinator. World must outlive Session.
// Hosts bound their session count.
// Mutation is not performed; grant/filter state cannot be supplied to next().
class Session {
 public:
  Session(const world::World& world, const ReadGrant& grant);
  Session(const Session&)=delete;
  Session& operator=(const Session&)=delete;
  Session(Session&&)=delete;
  Session& operator=(Session&&)=delete;
  Result start(const Query& query);
  Result next(const Cursor& cursor);
  void close() noexcept;
 private:
  using Clock=std::chrono::steady_clock;
  struct Snapshot {
    std::vector<std::string> rows;
    std::string prefix;
    std::size_t page_size=0, response_bytes=0;
    Clock::time_point deadline;
  };
  const world::World& world_;
  ReadGrant grant_;
  std::shared_ptr<const int> identity_=std::make_shared<const int>(0);
  Clock::time_point expires_;
  std::optional<Snapshot> snapshot_;
  std::uint64_t serial_=0;
  bool closed_=false;
  Result page(const Snapshot& snapshot, std::size_t offset, std::uint64_t serial) const;
  bool expire() noexcept;
};
}  // namespace ow::observe
