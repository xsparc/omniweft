// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/control.hpp"
#ifdef OW_ENABLE_VULKAN
#include "omniweft/render.hpp"
#endif
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace ow::live_report {
using Json=nlohmann::json;
namespace control=ow::control;
inline Json snapshot_json(const ow::world::Snapshot& snapshot) {
  Json slots=Json::array();
  for(const auto& slot:snapshot.slots) {
    Json entity=nullptr;
    if(slot.entity) {
      const auto& value=*slot.entity;
      entity={{"prefab",value.prefab},{"authoring_revision",value.authoring_revision},
        {"transform",{{"position_m",value.transform.position_m},{"rotation_xyzw",value.transform.rotation_xyzw},{"scale",value.transform.scale}}}};
    }
    slots.push_back({{"entity_uuid",slot.entity_uuid},{"generation",slot.generation},{"retired",slot.retired},{"entity",std::move(entity)}});
  }
  return {{"format_version",snapshot.format_version},{"world_id",snapshot.world_id},{"seed",snapshot.seed},
    {"max_slots",snapshot.max_slots},{"world_revision",snapshot.world_revision},{"slots",std::move(slots)}};
}
inline Json runtime_json(const control::RuntimeStatus& status) {
  return {{"schema_version",1},{"tick_rate_hz",60},{"max_catch_up_steps",4},
    {"simulation_tick",status.simulation_tick},{"snapshot_sequence",status.snapshot_sequence},
    {"overload_count",status.overload_count},{"dropped_ticks",status.dropped_ticks},{"remainder_units",status.remainder_units},
    {"snapshot",snapshot_json(status.snapshot)},{"presentation",{{"enabled",status.presentation.enabled},{"ready",status.presentation.ready},
      {"frame_count",status.presentation.frame_count},{"world_revision",status.presentation.world_revision},
      {"snapshot_sequence",status.presentation.snapshot_sequence}}}};
}
inline void claim_output(const std::filesystem::path& directory) {
  if(directory.empty()) return;
  if(!directory.parent_path().empty()) std::filesystem::create_directories(directory.parent_path());
  if(!std::filesystem::create_directory(directory)) throw std::runtime_error("OUTPUT_UNAVAILABLE");
}
inline void write_report(const std::filesystem::path& directory,const Json& report) {
  if(directory.empty()) return;
  const auto temporary=directory/"result.json.tmp";
  try {
    std::ofstream stream(temporary,std::ios::binary);stream.exceptions(std::ios::badbit|std::ios::failbit);
    stream<<report.dump(2)<<'\n';stream.close();std::filesystem::rename(temporary,directory/"result.json");
  } catch(...) {std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}

#ifdef OW_ENABLE_VULKAN
inline Json camera_json(const ow::presentation::Camera& c) {
  return {{"position_m",c.position_m},{"left",c.left},{"right",c.right},{"bottom",c.bottom},{"top",c.top},{"near_m",c.near_m},{"far_m",c.far_m}};
}
inline Json objects_json(const ow::presentation::Packet& packet) {
  Json objects=Json::array();
  for(const auto& object:packet.objects)
    objects.push_back({{"object_id",object.object_id},{"entity_uuid",object.entity_uuid},{"generation",object.generation},
      {"authoring_revision",object.authoring_revision},{"transform",{{"position_m",object.transform.position_m},
        {"rotation_xyzw",object.transform.rotation_xyzw},{"scale",object.transform.scale}}}});
  return objects;
}
inline Json extent_json(ow::presentation::Extent extent) {return {{"width",extent.width},{"height",extent.height}};}
inline void write_bytes(const std::filesystem::path& path,const void* data,std::size_t size) {
  std::ofstream output(path,std::ios::binary);output.exceptions(std::ios::badbit|std::ios::failbit);
  output.write(static_cast<const char*>(data),static_cast<std::streamsize>(size));output.close();
}
inline void gpu_report(Json& report,const ow::render::RunResult& run,const std::filesystem::path& directory) {
  report["status"]=run.status;
  report["device"]={{"name",run.device.name},{"api_version",run.device.api_version},{"driver_version",run.device.driver_version},{"device_type",run.device.device_type}};
  report["validation"]={{"enabled",run.validation.enabled},{"errors",run.validation.errors},{"warnings",run.validation.warnings},{"messages",Json::array()}};
  for(const auto& message:run.validation.messages) report["validation"]["messages"].push_back({{"severity",message.severity},{"id",message.id}});
  for(const auto& error:run.errors) report["errors"].push_back({{"code",error.code},{"message",error.message}});
  for(const auto& event:run.window_events) report["window_events"].push_back({{"kind",event.kind},{"pixel_extent",extent_json(event.pixel_extent)},
    {"minimized",event.minimized},{"submission_serial",event.submission_serial},{"swapchain_generation",event.swapchain_generation}});
  for(const auto& event:run.lifecycle_events) report["lifecycle_events"].push_back({{"event",event.event},{"submission_serial",event.submission_serial},
    {"swapchain_generation",event.swapchain_generation},{"graphics_pending",event.graphics_pending},{"present_pending",event.present_pending}});
  for(const auto& frame:run.frames) {
    const auto prefix=frame.phase+"-"+std::to_string(frame.frame_id);
    const std::string color_path=prefix+".color.bin",id_path=prefix+".id.bin",depth_path=prefix+".depth.bin";
    if(!directory.empty()) {
      write_bytes(directory/color_path,frame.color.data(),frame.color.size());
      write_bytes(directory/id_path,frame.object_ids.data(),frame.object_ids.size()*sizeof(std::uint32_t));
      write_bytes(directory/depth_path,frame.depth.data(),frame.depth.size()*sizeof(float));
    }
    const auto attachment=[&](const std::string& path,const std::string& format) -> Json {
      return {{"path",path},{"format",format},{"origin","top_left"},{"row_stride_bytes",frame.pixel_extent.width*4U}};
    };
    report["frames"].push_back({{"phase",frame.phase},{"frame_id",frame.frame_id},{"world_id",frame.packet.world_id},
      {"world_revision",frame.packet.world_revision},{"simulation_tick",frame.simulation_tick},{"snapshot_sequence",frame.snapshot_sequence},
      {"objects",objects_json(frame.packet)},{"camera",camera_json(frame.packet.camera)},{"pixel_extent",extent_json(frame.pixel_extent)},
      {"source_generation",frame.source_generation},{"swapchain_generation",frame.swapchain_generation},{"image_index",frame.image_index},
      {"submission_serial",frame.submission_serial},{"present_result",frame.present_result},
      {"attachments",{{"color",attachment(color_path,frame.color_format)},{"object_id",attachment(id_path,"R32_UINT")},{"depth",attachment(depth_path,"D32_SFLOAT")}}},
      {"copy",{{"command","vkCmdCopyImage"},{"source_generation",frame.source_generation},{"swapchain_generation",frame.swapchain_generation},
        {"image_index",frame.image_index},{"pixel_extent",extent_json(frame.pixel_extent)},{"source_format",frame.color_format},
        {"destination_format",frame.color_format},{"full_extent",true}}}});
  }
}
#endif
}  // namespace ow::live_report
