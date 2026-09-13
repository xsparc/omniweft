// SPDX-License-Identifier: Apache-2.0
#include "render_example.hpp"
#include "omniweft/presentation.hpp"
#include "omniweft/transactions.hpp"
#ifdef OW_ENABLE_VULKAN
#include "omniweft/render.hpp"
#endif
#include <nlohmann/json.hpp>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
namespace {
using Json=nlohmann::json;
namespace commands=ow::commands;
namespace world=ow::world;
namespace transactions=ow::transactions;
namespace presentation=ow::presentation;
struct Options {bool gpu=false,headless=false,verify=false,interactive=false;std::filesystem::path output;};
Options options_from(int argc,char* argv[]) {
  Options options;bool example=false,seed=false,output=false;
  for(int i=1;i<argc;++i) {
    const std::string_view arg(argv[i]);
    const auto value=[&]() -> std::string_view {
      if(i+1>=argc || std::string_view(argv[i+1]).starts_with("--")) throw std::invalid_argument("missing option value");
      return argv[++i];
    };
    if(arg=="--example"&&!example) {example=true;if(value()!="render.world_cube") throw std::invalid_argument("expected render.world_cube");}
    else if(arg=="--seed"&&!seed) {seed=true;if(value()!="7") throw std::invalid_argument("use --seed 7");}
    else if(arg=="--output"&&!output) {output=true;options.output=value();if(options.output.empty()) throw std::invalid_argument("use a fresh output directory");}
    else if(arg=="--gpu"&&!options.gpu) options.gpu=true;
    else if(arg=="--headless"&&!options.headless) options.headless=true;
    else if(arg=="--verify"&&!options.verify) options.verify=true;
    else if(arg=="--interactive"&&!options.interactive) options.interactive=true;
    else throw std::invalid_argument("unknown or duplicate option");
  }
  if(!example||!seed||!output||options.gpu==options.headless||(options.interactive&&!options.gpu))
    throw std::invalid_argument("required: --example render.world_cube --seed 7 --gpu or --headless --output fresh-directory");
  return options;
}
Json transform_json(const world::Transform& transform) {
  return {{"position_m",transform.position_m},{"rotation_xyzw",transform.rotation_xyzw},{"scale",transform.scale}};
}
Json camera_json(const presentation::Camera& c) {
  return {{"position_m",c.position_m},{"left",c.left},{"right",c.right},{"bottom",c.bottom},{"top",c.top},{"near_m",c.near_m},{"far_m",c.far_m}};
}
Json objects_json(const presentation::Packet& packet) {
  Json objects=Json::array();
  for(const auto& object:packet.objects)
    objects.push_back({{"object_id",object.object_id},{"entity_uuid",object.entity_uuid},{"generation",object.generation},
      {"authoring_revision",object.authoring_revision},{"transform",transform_json(object.transform)}});
  return objects;
}
Json packet_json(const presentation::Packet& packet,const char* phase) {
  Json vertices=Json::array();
  for(const auto& vertex:packet.vertices) vertices.push_back({{"clip_position",vertex.clip_position},{"color",vertex.color},{"object_id",vertex.object_id}});
  return {{"phase",phase},{"world_id",packet.world_id},{"world_revision",packet.world_revision},{"camera",camera_json(packet.camera)},
    {"objects",objects_json(packet)},{"vertices",std::move(vertices)},{"indices",packet.indices}};
}
Json snapshot_json(const world::Snapshot& snapshot) {
  Json slots = Json::array();
  for (const auto& slot : snapshot.slots) {
    Json entity = nullptr;
    if (slot.entity) {
      const auto& value = *slot.entity;
      entity = {{"prefab", value.prefab}, {"authoring_revision", value.authoring_revision},
        {"transform", {{"position_m", value.transform.position_m},
          {"rotation_xyzw", value.transform.rotation_xyzw}, {"scale", value.transform.scale}}}};
    }
    slots.push_back({{"entity_uuid", slot.entity_uuid}, {"generation", slot.generation},
      {"retired", slot.retired}, {"entity", std::move(entity)}});
  }
  return {{"format_version", snapshot.format_version}, {"world_id", snapshot.world_id},
    {"seed", snapshot.seed}, {"max_slots", snapshot.max_slots},
    {"world_revision", snapshot.world_revision}, {"slots", std::move(slots)}};
}
Json receipt_json(const transactions::Receipt& receipt) {
  Json errors = Json::array(), created = Json::array();
  for (const auto& error : receipt.errors) {
    Json item = {{"code", error.code}, {"path", error.path}, {"message", error.message}};
    if (error.operation_index) item["operation_index"] = *error.operation_index;
    errors.push_back(std::move(item));
  }
  for (const auto& binding : receipt.created)
    created.push_back({{"temporary_id", binding.temporary_id}, {"world_id", binding.world_id},
      {"entity_uuid", binding.entity_uuid}, {"generation", binding.generation}});
  return {{"status", receipt.status}, {"durability", receipt.durability},
    {"transaction_id", receipt.transaction_id}, {"world_revision", receipt.world_revision},
    {"created", std::move(created)}, {"errors", std::move(errors)}};
}

commands::Envelope fixture(std::size_t index) {
  commands::Envelope envelope;
  envelope.world_id="workshop";envelope.transaction_id=index==0?"018f7242-4387-7c98-a114-67787915a401":"018f7242-4387-7c98-a114-67787915a402";
  envelope.idempotency={"fixture-epoch",static_cast<std::uint64_t>(index)+1};
  envelope.expected_world_revision=index;envelope.apply_at={"next_tick",120};
  if(index==0) envelope.operations={
    commands::EntityCreate{"cube","builtin.unit_cube"},
    commands::TransformSet{commands::TemporaryTarget{"cube"},{-1,0,0},{0,.6,0,.8},{1,1,1}}};
  else envelope.operations={commands::TransformSet{
    commands::EntityTarget{"workshop","00000007-0000-4000-8000-000000000001",1},{1,.25,0},{0,.6,0,.8},{.5,1.5,1}}};
  envelope.budget={static_cast<std::uint64_t>(envelope.operations.size()),0};
  return envelope;
}
void claim_output(const std::filesystem::path& directory) {
  if(!directory.parent_path().empty()) std::filesystem::create_directories(directory.parent_path());
  if(!std::filesystem::create_directory(directory)) throw std::runtime_error("output exists");
}
void write_report(const std::filesystem::path& directory,const Json& report) {
  const auto temporary=directory/"result.json.tmp";
  try {
    std::ofstream output(temporary,std::ios::binary);output.exceptions(std::ios::badbit|std::ios::failbit);
    output<<report.dump(2)<<'\n';output.close();std::filesystem::rename(temporary,directory/"result.json");
  } catch(...) {std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}
#ifdef OW_ENABLE_VULKAN
Json extent_json(presentation::Extent extent) {return {{"width",extent.width},{"height",extent.height}};}
void write_bytes(const std::filesystem::path& path,const void* data,std::size_t size) {
  std::ofstream output(path,std::ios::binary);output.exceptions(std::ios::badbit|std::ios::failbit);
  output.write(static_cast<const char*>(data),static_cast<std::streamsize>(size));output.close();
}
void gpu_report(Json& report,const ow::render::RunResult& run,const std::filesystem::path& directory) {
  report["status"]=run.status;
  report["gpu_checks"]=run.status=="passed"?"passed":"failed";
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
    write_bytes(directory/color_path,frame.color.data(),frame.color.size());
    write_bytes(directory/id_path,frame.object_ids.data(),frame.object_ids.size()*sizeof(std::uint32_t));
    write_bytes(directory/depth_path,frame.depth.data(),frame.depth.size()*sizeof(float));
    const auto attachment=[&](const std::string& path,const std::string& format) -> Json {
      return {{"path",path},{"format",format},{"origin","top_left"},{"row_stride_bytes",frame.pixel_extent.width*4U}};
    };
    Json capture={{"phase",frame.phase},{"frame_id",frame.frame_id},{"world_id",frame.packet.world_id},{"world_revision",frame.packet.world_revision},
      {"objects",objects_json(frame.packet)},{"camera",camera_json(frame.packet.camera)},{"pixel_extent",extent_json(frame.pixel_extent)},
      {"source_generation",frame.source_generation},{"swapchain_generation",frame.swapchain_generation},{"image_index",frame.image_index},
      {"submission_serial",frame.submission_serial},{"present_result",frame.present_result},
      {"attachments",{{"color",attachment(color_path,frame.color_format)},{"object_id",attachment(id_path,"R32_UINT")},{"depth",attachment(depth_path,"D32_SFLOAT")}}},
      {"copy",{{"command","vkCmdCopyImage"},{"source_generation",frame.source_generation},{"swapchain_generation",frame.swapchain_generation},
        {"image_index",frame.image_index},{"pixel_extent",extent_json(frame.pixel_extent)},{"source_format",frame.color_format},
        {"destination_format",frame.color_format},{"full_extent",true}}}};
    report["frames"].push_back(std::move(capture));
  }
}
#endif
}  // namespace
int run_render_example(int argc,char* argv[]) {
  Options options;
  try {options=options_from(argc,argv);}
  catch(const std::exception&) {
    std::cerr<<"invalid_arguments: use --example render.world_cube --seed 7 --gpu or --headless [--verify] --output <fresh-directory>\n";return 2;
  }
  try {claim_output(options.output);}
  catch(const std::exception&) {std::cerr<<"output_error: use a writable, fresh artifact directory\n";return 5;}
  Json report={{"schema_version",1},{"example","render.world_cube"},{"seed",7},{"mode",options.gpu?"gpu":"headless"},
    {"status","failed"},{"verified",options.verify},{"gpu_checks","not_run"},{"device",Json::object()},
    {"validation",{{"enabled",false},{"errors",0},{"warnings",0},{"messages",Json::array()}}},
    {"packets",Json::array()},{"receipts",Json::array()},{"snapshots",Json::array()},{"frames",Json::array()},
    {"window_events",Json::array()},{"lifecycle_events",Json::array()},{"errors",Json::array()}};
  try {
    world::World target_world("workshop",7);
    std::array<presentation::Packet,2> packets;
    std::array<world::Snapshot,2> snapshots;
    for(std::size_t index=0;index<2;++index) {
      const auto receipt=transactions::Coordinator::apply_at_boundary(target_world,fixture(index));
      if(receipt.status!="committed") throw std::runtime_error("fixture rejected");
      snapshots[index]=target_world.snapshot();packets[index]=presentation::make_packet(snapshots[index]);
      report["receipts"].push_back(receipt_json(receipt));report["snapshots"].push_back(snapshot_json(snapshots[index]));
      report["packets"].push_back(packet_json(packets[index],index==0?"initial":"transformed"));
    }
    if(options.verify && presentation::make_packet(snapshots[0])!=packets[0]) throw std::runtime_error("detached packet changed");
    report["status"]="passed";
    if(options.gpu) {
#ifdef OW_ENABLE_VULKAN
      const auto result=ow::render::run_world_cube(packets,options.interactive);
      gpu_report(report,result,options.output);
#else
      report["status"]="unsupported";
      report["errors"].push_back({{"code","RENDERER_NOT_BUILT"},{"message","Provision the pinned graphics sources and use the optional Vulkan preset."}});
#endif
    }
  } catch(const std::exception&) {
    report["status"]="failed";report["errors"].push_back({{"code","RENDER_FAILED"},{"message","The bounded presentation fixture could not complete."}});
  }
  try {write_report(options.output,report);}
  catch(const std::exception&) {std::cerr<<"output_error: artifact writing failed; use a fresh writable directory\n";return 5;}
  const std::string status=report["status"];
  std::cout<<"render.world_cube: "<<status<<"; "<<(options.gpu?"presentation":"headless structural")<<" report written\n";
  return status=="passed"?0:status=="unsupported"?3:4;
}
