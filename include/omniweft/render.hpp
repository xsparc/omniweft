// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/presentation.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ow::render {
struct Diagnostic { std::string code, message; };
struct ValidationMessage { std::string severity, id; };
struct Validation {
  bool enabled = false;
  std::uint64_t errors = 0, warnings = 0;
  std::vector<ValidationMessage> messages;
};
struct DeviceInfo { std::string name, api_version, driver_version, device_type; };
struct WindowEvent {
  std::string kind;
  presentation::Extent pixel_extent;
  bool minimized = false;
  std::uint64_t submission_serial = 0, swapchain_generation = 0;
};
struct LifecycleEvent {
  std::string event;
  std::uint64_t submission_serial = 0, swapchain_generation = 0;
  bool graphics_pending = false, present_pending = false;
};
struct FrameCapture {
  std::string phase;
  std::uint64_t frame_id = 0, source_generation = 0, swapchain_generation = 0, submission_serial = 0;
  std::uint32_t image_index = 0;
  presentation::Extent pixel_extent;
  std::string color_format, present_result;
  presentation::Packet packet;
  std::vector<std::uint8_t> color;
  std::vector<std::uint32_t> object_ids;
  std::vector<float> depth;
};
struct RunResult {
  std::string status = "failed";
  DeviceInfo device;
  Validation validation;
  std::vector<FrameCapture> frames;
  std::vector<WindowEvent> window_events;
  std::vector<LifecycleEvent> lifecycle_events;
  std::vector<Diagnostic> errors;
};
// One bounded windowed fixture; all authoritative world work precedes this call.
RunResult run_world_cube(const std::array<presentation::Packet, 2>& revisions, bool interactive);
}  // namespace ow::render
