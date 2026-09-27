// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>
namespace ow::persistence::io {
using Bytes=std::vector<std::uint8_t>;
using Digest=std::array<std::uint8_t,32>;
Digest sha256(std::span<const std::uint8_t>);
std::string random_epoch();
void check_directory(const std::filesystem::path&);
std::uint64_t bytes_used(const std::filesystem::path&);
Bytes read(const std::filesystem::path&,std::size_t limit);
void sync_directory(const std::filesystem::path&);
void replace_checkpoint(const std::filesystem::path&);
class Lock {
 public:
  explicit Lock(const std::filesystem::path&);
  ~Lock();
  Lock(const Lock&)=delete;
  Lock& operator=(const Lock&)=delete;
 private:
  struct State; std::unique_ptr<State> state_;
};
class File {
 public:
  explicit File(const std::filesystem::path&,bool append);
  ~File();
  File(const File&)=delete;
  File& operator=(const File&)=delete;
  void write(std::span<const std::uint8_t>);
  void flush();
  void truncate(std::uint64_t);
 private:
  struct State; std::unique_ptr<State> state_;
};
} // namespace ow::persistence::io
