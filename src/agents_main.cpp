// SPDX-License-Identifier: Apache-2.0
#include "omniweft/control.hpp"
#include "omniweft/simulation.hpp"
#include "omniweft/presentation.hpp"
#ifdef OW_ENABLE_VULKAN
#include "omniweft/render.hpp"
#endif
#include <nlohmann/json.hpp>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>

namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
using Time = Clock::time_point;
using Milliseconds = std::chrono::milliseconds;
namespace control = ow::control;
struct Options {
  control::Config control;
  bool gpu = false, interactive = false;
  std::filesystem::path output;
};
Options options_from(int argc,char* argv[]) {
  if(argc>19 || argc%2==0) throw std::invalid_argument("INVALID_CLI");
  Options result;result.control.max_slots=8;
  std::unordered_set<std::string_view> seen;
  for(int index=1;index<argc;index+=2) {
    const std::string_view key(argv[index]),value(argv[index+1]);
    if(key.size()>32 || value.empty() || value.size()>4096 || !seen.insert(key).second)
      throw std::invalid_argument("INVALID_CLI");
    if(key=="--output") {result.output=std::filesystem::path(value);continue;}
    if(value.size()>32) throw std::invalid_argument("INVALID_CLI");
    if(key=="--world") {if(value!="workshop") throw std::invalid_argument("INVALID_CLI");continue;}
    std::uint32_t number=0;
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
    if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size()) throw std::invalid_argument("INVALID_CLI");
    if(key=="--seed" && number==7) result.control.seed=number;
    else if(key=="--max-slots" && number>=1 && number<=1024) result.control.max_slots=number;
    else if(key=="--session-ttl-ms" && number>=50 && number<=300000) result.control.session_ttl_ms=number;
    else if(key=="--max-runtime-ms" && number>=1000 && number<=600000) result.control.max_runtime_ms=number;
    else if(key=="--max-requests" && number>=1 && number<=4096) result.control.max_requests=number;
    else if(key=="--gpu" && number<=1) result.gpu=number!=0;
    else if(key=="--interactive" && number<=1) result.interactive=number!=0;
    else throw std::invalid_argument("INVALID_CLI");
  }
  if(result.interactive && !result.gpu) throw std::invalid_argument("INVALID_CLI");
  return result;
}
class LifetimeGuard {
 public:
  explicit LifetimeGuard(Time deadline):thread_([this,deadline] {
    std::unique_lock lock(mutex_);
    if(!changed_.wait_until(lock,deadline,[this]{return complete_;})) std::_Exit(4);
  }) {}
  ~LifetimeGuard() {
    {std::lock_guard lock(mutex_);complete_=true;}
    changed_.notify_one();thread_.join();
  }
 private:
  std::mutex mutex_;
  std::condition_variable changed_;
  bool complete_=false;
  std::thread thread_;
};
struct Published {
  ow::simulation::StepStats clock;
  ow::world::Snapshot snapshot;
};
class Owner {
 public:
  Owner(const control::Config& config,Time deadline,std::atomic<bool>& stop,bool gpu)
      :world_(config.world_id,config.seed,config.max_slots),deadline_(deadline),stop_(stop) {
    published_.store(std::make_shared<const Published>(Published{{},world_.snapshot()}));
    auto presentation=std::make_shared<control::PresentationStatus>();presentation->enabled=gpu;
    presentation_.store(presentation);
  }
  void run() noexcept {
    // A claimed closure may already have returned when snapshot allocation
    // fails. Keep its waiter reachable until publication or failure is notified.
    std::shared_ptr<Job> completed;
    try {
      auto previous=Clock::now();
      while(!stopping()) {
        const auto now=Clock::now();
        const auto elapsed=static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(now-previous).count());
        previous=now;
        const auto prior=stepper_.stats();
        const auto phase=prior.remainder_units+(elapsed%1000000000ULL)*60;
        const auto due=(elapsed/1000000000ULL)*60+phase/1000000000ULL;
        const bool overloaded=due>4;
        static_cast<void>(stepper_.advance(elapsed,[&](std::uint64_t ordinal) {
          // No authoring execution from due-zero polls or overload catch-up.
          if(!completed) completed=execute_job(true,overloaded);
          // advance() validated cumulative-counter bounds before invoking us.
          // Every actual completed tick gets its own immutable publication,
          // including each catch-up tick, before a committed request is acked.
          const ow::simulation::StepStats completed_tick{ordinal,
            prior.overload_count+(overloaded?1U:0U),prior.dropped_ticks+(overloaded?due-4:0),
            phase%1000000000ULL};
          published_.store(std::make_shared<const Published>(Published{completed_tick,world_.snapshot()}));
        }));
        finish(completed);completed.reset();
        completed=execute_job(false,false);
        finish(completed);completed.reset();
        std::unique_lock lock(mutex_);
        changed_.wait_for(lock,Milliseconds(1));
      }
    } catch(...) {
      failed_.store(true);stop_.store(true);
      if(completed) {
        // Failed publication cannot acknowledge successful execution. The
        // closure has ended, so failure notification safely releases captures.
        completed->failure=std::current_exception();
        finish(completed);
      }
    }
    cancel_pending();
  }
  bool dispatch(std::function<void()> task,Time deadline,bool authoring) {
    auto job=std::make_shared<Job>();
    job->task=std::move(task);job->deadline=std::min(deadline,deadline_);job->authoring=authoring;
    std::unique_lock lock(mutex_);
    if(stopping() || pending_ || Clock::now()>=job->deadline) return false;
    pending_=job;changed_.notify_all();
    while(job->state==State::pending) {
      if(changed_.wait_until(lock,job->deadline)==std::cv_status::timeout && job->state==State::pending) {
        job->state=State::canceled;
        if(pending_==job) pending_.reset();
        changed_.notify_all();return false;
      }
    }
    // Claimed tasks own captures from the waiting gateway's stack. Never return
    // on timeout while a claimed closure can still execute or use those captures.
    changed_.wait(lock,[&]{return job->state!=State::running;});
    if(job->state==State::canceled) return false;
    if(job->failure) std::rethrow_exception(job->failure);
    return true;
  }
  control::Callbacks callbacks() {
    control::Callbacks result;
    result.snapshot=[this]{return world_.snapshot();};
    result.apply=[this](const ow::commands::Envelope& envelope) {
      return ow::transactions::Coordinator::apply_at_boundary(world_,envelope);
    };
    result.at_boundary=[this](std::function<void()> task,Time deadline,bool authoring) {
      return dispatch(std::move(task),deadline,authoring);
    };
    result.runtime_status=[this]{return runtime();};
    result.should_stop=[this]{return stopping();};
    result.normal_deadline=deadline_;
    return result;
  }
  std::shared_ptr<const Published> latest() const {return published_.load();}
  control::RuntimeStatus runtime() const {
    // Read presentation first. Its publication follows acquisition of the
    // referenced immutable world publication; the subsequent world load is
    // causally at least as new, so the response never mixes old state/new frame.
    const auto presentation=presentation_.load();
    const auto publication=latest();
    const auto& stats=publication->clock;
    control::RuntimeStatus result{stats.simulation_tick,stats.simulation_tick+1,
      stats.overload_count,stats.dropped_ticks,stats.remainder_units,publication->snapshot,*presentation};
    if(result.presentation.snapshot_sequence>result.snapshot_sequence ||
       result.presentation.world_revision>result.snapshot.world_revision) {
      result.presentation={presentation->enabled,presentation->ready,0,0,0};
    }
    return result;
  }
  void presentation_ready() {
    auto next=std::make_shared<control::PresentationStatus>(*presentation_.load());next->ready=true;
    presentation_.store(next);
  }
  void presented(std::uint64_t frame_count,std::uint64_t revision,std::uint64_t sequence) {
    auto next=std::make_shared<control::PresentationStatus>();
    *next={true,true,frame_count,revision,sequence};presentation_.store(next);
  }
  bool failed() const {return failed_.load();}
  bool stopping() const {return stop_.load() || Clock::now()>=deadline_;}
  void wake() {changed_.notify_all();}
 private:
  enum class State {pending,running,completed,canceled};
  struct Job {
    std::function<void()> task;
    Time deadline;
    bool authoring=false;
    State state=State::pending;
    std::exception_ptr failure;
  };
  ow::world::World world_;
  Time deadline_;
  std::atomic<bool>& stop_;
  std::atomic<bool> failed_=false;
  ow::simulation::FixedStepper stepper_;
  std::atomic<std::shared_ptr<const Published>> published_;
  std::atomic<std::shared_ptr<const control::PresentationStatus>> presentation_;
  std::mutex mutex_;
  std::condition_variable changed_;
  std::shared_ptr<Job> pending_;
  std::shared_ptr<Job> execute_job(bool tick,bool overloaded) {
    std::shared_ptr<Job> job;
    {
      std::lock_guard lock(mutex_);
      if(!pending_) return {};
      if(stopping() || Clock::now()>=pending_->deadline) {
        pending_->state=State::canceled;pending_.reset();changed_.notify_all();return {};
      }
      if(pending_->authoring && (!tick || overloaded)) return {};
      job=std::move(pending_);job->state=State::running;
    }
    try {job->task();} catch(...) {job->failure=std::current_exception();}
    return job;
  }
  void finish(const std::shared_ptr<Job>& job) {
    if(!job) return;
    std::lock_guard lock(mutex_);job->state=State::completed;changed_.notify_all();
  }
  void cancel_pending() {
    std::lock_guard lock(mutex_);
    if(pending_) {pending_->state=State::canceled;pending_.reset();}
    changed_.notify_all();
  }
};
Json snapshot_json(const ow::world::Snapshot& snapshot) {
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
Json runtime_json(const control::RuntimeStatus& status) {
  return {{"schema_version",1},{"tick_rate_hz",60},{"max_catch_up_steps",4},
    {"simulation_tick",status.simulation_tick},{"snapshot_sequence",status.snapshot_sequence},
    {"overload_count",status.overload_count},{"dropped_ticks",status.dropped_ticks},{"remainder_units",status.remainder_units},
    {"snapshot",snapshot_json(status.snapshot)},{"presentation",{{"enabled",status.presentation.enabled},{"ready",status.presentation.ready},
      {"frame_count",status.presentation.frame_count},{"world_revision",status.presentation.world_revision},
      {"snapshot_sequence",status.presentation.snapshot_sequence}}}};
}
void claim_output(const std::filesystem::path& directory) {
  if(directory.empty()) return;
  if(!directory.parent_path().empty()) std::filesystem::create_directories(directory.parent_path());
  if(!std::filesystem::create_directory(directory)) throw std::runtime_error("OUTPUT_UNAVAILABLE");
}
void write_report(const std::filesystem::path& directory,const Json& report) {
  if(directory.empty()) return;
  const auto temporary=directory/"result.json.tmp";
  try {
    std::ofstream stream(temporary,std::ios::binary);stream.exceptions(std::ios::badbit|std::ios::failbit);
    stream<<report.dump(2)<<'\n';stream.close();std::filesystem::rename(temporary,directory/"result.json");
  } catch(...) {std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}

#ifdef OW_ENABLE_VULKAN
Json camera_json(const ow::presentation::Camera& c) {
  return {{"position_m",c.position_m},{"left",c.left},{"right",c.right},{"bottom",c.bottom},{"top",c.top},{"near_m",c.near_m},{"far_m",c.far_m}};
}
Json objects_json(const ow::presentation::Packet& packet) {
  Json objects=Json::array();
  for(const auto& object:packet.objects)
    objects.push_back({{"object_id",object.object_id},{"entity_uuid",object.entity_uuid},{"generation",object.generation},
      {"authoring_revision",object.authoring_revision},{"transform",{{"position_m",object.transform.position_m},
        {"rotation_xyzw",object.transform.rotation_xyzw},{"scale",object.transform.scale}}}});
  return objects;
}
Json extent_json(ow::presentation::Extent extent) {return {{"width",extent.width},{"height",extent.height}};}
void write_bytes(const std::filesystem::path& path,const void* data,std::size_t size) {
  std::ofstream output(path,std::ios::binary);output.exceptions(std::ios::badbit|std::ios::failbit);
  output.write(static_cast<const char*>(data),static_cast<std::streamsize>(size));output.close();
}
void gpu_report(Json& report,const ow::render::RunResult& run,const std::filesystem::path& directory) {
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
int execute(const Options& options) {
  const auto deadline=Clock::now()+Milliseconds(options.control.max_runtime_ms);
  LifetimeGuard guard(deadline+Milliseconds(1000));
  claim_output(options.output);
  std::atomic<bool> stop=false;
  std::atomic<int> gateway_result=4;
  Owner owner(options.control,deadline,stop,options.gpu);
  Json report={{"schema_version",1},{"example","agents.mock_builder"},{"status","passed"},{"runtime",Json::object()},
    {"device",Json::object()},{"validation",{{"enabled",false},{"errors",0},{"warnings",0},{"messages",Json::array()}}},
    {"frames",Json::array()},{"window_events",Json::array()},{"lifecycle_events",Json::array()},{"errors",Json::array()}};
  std::thread simulation([&]{owner.run();});
  std::thread gateway;
  std::exception_ptr failure;
#ifdef OW_ENABLE_VULKAN
  ow::render::RunResult rendered;
#endif
  try {
    gateway=std::thread([&] {
      gateway_result.store(control::run(options.control,owner.callbacks()));
      stop.store(true);owner.wake();
    });
    if(options.gpu) {
#ifdef OW_ENABLE_VULKAN
      rendered=ow::render::run_live({options.control.max_slots,options.interactive},{
        [&] {
          const auto publication=owner.latest();
          return ow::render::LiveFrame{ow::presentation::make_packet(publication->snapshot),
            publication->clock.simulation_tick,publication->clock.simulation_tick+1};
        },
        [&]{return owner.stopping();},
        [&]{owner.presentation_ready();},
        [&](const ow::render::LiveFrame& frame,std::uint64_t count) {
          owner.presented(count,frame.packet.world_revision,frame.snapshot_sequence);
        }});
#else
      report["status"]="unsupported";
      report["errors"].push_back({{"code","RENDERER_NOT_BUILT"},{"message","Use the pinned optional Vulkan build for GPU presentation."}});
#endif
    } else {
      while(!owner.stopping()) std::this_thread::sleep_for(Milliseconds(10));
    }
  } catch(...) {failure=std::current_exception();}
  stop.store(true);owner.wake();
  if(gateway.joinable()) gateway.join();
  simulation.join();
  if(failure) std::rethrow_exception(failure);
#ifdef OW_ENABLE_VULKAN
  if(options.gpu) gpu_report(report,rendered,options.output);
#endif
  report["runtime"]=runtime_json(owner.runtime());
  if(owner.failed() || gateway_result.load()!=0) {
    report["status"]="failed";
    report["errors"].push_back({{"code","HOST_FAILED"},{"message","The bounded control or simulation owner did not stop cleanly."}});
  }
  write_report(options.output,report);
  const std::string status=report["status"];
  return status=="passed"?0:status=="unsupported"?3:4;
}
}  // namespace
int main(int argc,char* argv[]) {
  Options options;
  try {options=options_from(argc,argv);}
  catch(...) {std::fputs("AGENTS_INVALID_CLI\n",stderr);return 2;}
  try {return execute(options);}
  catch(...) {std::fputs("AGENTS_FAILED\n",stderr);return 4;}
}
