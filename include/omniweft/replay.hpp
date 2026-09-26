// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/transactions.hpp"
#include <array>
#include <memory>
#include <span>

namespace ow::replay {
inline constexpr std::size_t entry_limit=8, operation_limit=4, command_limit=16384, checkpoint_limit=8192;
inline constexpr std::string_view builtin_manifest=R"asset({"schema":1,"prefab":"builtin.unit_cube","revision":1,"faces_m":[[[0.5,-0.5,-0.5],[0.5,0.5,-0.5],[0.5,0.5,0.5],[0.5,-0.5,0.5]],[[-0.5,-0.5,0.5],[-0.5,0.5,0.5],[-0.5,0.5,-0.5],[-0.5,-0.5,-0.5]],[[-0.5,0.5,-0.5],[-0.5,0.5,0.5],[0.5,0.5,0.5],[0.5,0.5,-0.5]],[[-0.5,-0.5,0.5],[-0.5,-0.5,-0.5],[0.5,-0.5,-0.5],[0.5,-0.5,0.5]],[[-0.5,-0.5,0.5],[0.5,-0.5,0.5],[0.5,0.5,0.5],[-0.5,0.5,0.5]],[[0.5,-0.5,-0.5],[-0.5,-0.5,-0.5],[-0.5,0.5,-0.5],[0.5,0.5,-0.5]]],"face_rgba8":[[255,64,64,255],[128,32,32,255],[64,255,64,255],[32,128,32,255],[64,64,255,255],[32,32,128,255]],"face_triangles":[0,1,2,2,3,0]})asset";
inline constexpr std::string_view builtin_sha256="d2af4f6d7d72763bdb9c693423e2ac2d29927f3c7a44f1d45180268e9f9fbd69";
struct Asset {
  std::string prefab="builtin.unit_cube";
  std::uint32_t revision=1;
  std::string manifest{builtin_manifest}, content_sha256{builtin_sha256};
  bool operator==(const Asset&) const = default;
};
struct Record {
  std::uint64_t sequence=0, boundary_tick=0, expected_revision=0;
  commands::Budget budget;
  std::vector<commands::Operation> operations;
  std::vector<transactions::CreatedBinding> created;
  std::vector<std::uint8_t> checkpoint;
};
struct Log {
  std::uint32_t schema=1;
  std::string command_schema="0.1", checkpoint_schema="OWOBJ001-003", world_id;
  std::uint32_t seed=7, max_slots=8;
  std::vector<Asset> assets{Asset{}};
  std::vector<Record> records;
};
struct Result {
  std::unique_ptr<world::World> world;
  std::vector<transactions::Error> errors;
};
// Opt-in trusted owner boundary. World must outlive Recorder and start empty.
// Export is typed/in-memory; it provides no disk durability or authentication.
class Recorder {
 public:
  explicit Recorder(world::World&);
  Recorder(const Recorder&)=delete;
  Recorder& operator=(const Recorder&)=delete;
  transactions::Receipt apply_at_boundary(const commands::Envelope&, std::uint64_t boundary_tick,
                                          transactions::StagingGuard* = nullptr);
  Log export_log() const;
  std::size_t size() const noexcept {return count_;}
 private:
  world::World& world_;
  std::array<Record,entry_limit> records_{};
  std::size_t count_=0;
  std::uint64_t head_=0;
};
// Reconstruct only a fresh private world; failure never returns an applied prefix.
Result reconstruct(const Log&, std::span<const Asset> available_assets);
} // namespace ow::replay
