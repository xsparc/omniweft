// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/transactions.hpp"
#include <array>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace ow::retry {
inline constexpr std::size_t window_size=4, payload_limit=16384, receipt_field_limit=8192;
inline constexpr std::chrono::milliseconds receipt_lifetime{2000};
struct Result {
  // Borrowed until the next execute/reset; caller holds the owner boundary.
  const transactions::Receipt* receipt=nullptr;
  const char* code="";
  bool replayed=false;
};
// Trusted single-owner admission component, not authentication or a write grant.
// The host must authenticate the epoch and hold its policy lease before entry.
// Canonical payloads are generated from validated typed envelopes by the host.
class Window {
 public:
  Result execute(std::uint64_t sequence,std::string_view canonical,
                 const std::function<transactions::Receipt()>& apply);
  std::uint64_t next_sequence() const noexcept {return next_;}
  void reset() noexcept;
 private:
  using Clock=std::chrono::steady_clock;
  struct Entry {
    std::uint64_t sequence=0;
    std::string payload;
    std::optional<transactions::Receipt> receipt;
    Clock::time_point deadline{};
  };
  std::array<Entry,window_size> entries_{};
  std::uint64_t next_=1;
  bool exhausted_=false;
};
} // namespace ow::retry
