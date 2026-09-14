// SPDX-License-Identifier: Apache-2.0
#define VK_NO_PROTOTYPES
#define SDL_MAIN_HANDLED
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include "omniweft/render.hpp"
#include "world_cube_spirv.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace ow::render {
namespace {
using presentation::Extent;
using presentation::Packet;
using presentation::Vertex;
struct Failure : std::runtime_error {
  std::string code;
  bool unsupported;
  Failure(std::string c, const char* message, bool u=false) : std::runtime_error(message), code(std::move(c)), unsupported(u) {}
};
[[noreturn]] void fail(const char* code, const char* message, bool unsupported=false) { throw Failure(code,message,unsupported); }
void check(VkResult result, const char* operation) {
  if (result != VK_SUCCESS) fail(result == VK_ERROR_DEVICE_LOST ? "DEVICE_LOST" : "VULKAN_ERROR", operation);
}
template<class T> T info(VkStructureType type) { T result{}; result.sType=type; return result; }
std::string version(std::uint32_t value) {
  return std::to_string(VK_VERSION_MAJOR(value))+"."+std::to_string(VK_VERSION_MINOR(value))+"."+std::to_string(VK_VERSION_PATCH(value));
}
std::string safe_text(const char* text) {
  std::string out;
  if (!text) return out;
  for (std::size_t i=0;text[i] && i<160;++i) {
    const char c=text[i];
    if ((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') ||
        c==' ' || c=='_' || c=='-' || c=='.' || c=='(' || c==')') out.push_back(c);
  }
  return out;
}
#define OW_INSTANCE_FUNCTIONS(X) \
 X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) \
 X(vkGetPhysicalDeviceFeatures2) X(vkGetPhysicalDeviceQueueFamilyProperties) \
 X(vkGetPhysicalDeviceSurfaceSupportKHR) X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
 X(vkGetPhysicalDeviceSurfaceFormatsKHR) X(vkEnumerateDeviceExtensionProperties) \
 X(vkGetPhysicalDeviceFormatProperties) X(vkGetPhysicalDeviceMemoryProperties) \
 X(vkCreateDevice) X(vkGetDeviceProcAddr) X(vkDestroySurfaceKHR)
#define OW_DEVICE_FUNCTIONS(X) \
 X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR) \
 X(vkGetSwapchainImagesKHR) X(vkAcquireNextImageKHR) X(vkQueuePresentKHR) \
 X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) \
 X(vkCreateImageView) X(vkDestroyImageView) X(vkAllocateMemory) X(vkFreeMemory) \
 X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) \
 X(vkMapMemory) X(vkUnmapMemory) X(vkCreateRenderPass) X(vkDestroyRenderPass) \
 X(vkCreateFramebuffer) X(vkDestroyFramebuffer) X(vkCreateShaderModule) X(vkDestroyShaderModule) \
 X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) X(vkCreateGraphicsPipelines) X(vkDestroyPipeline) \
 X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkResetCommandBuffer) \
 X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCmdBeginRenderPass) X(vkCmdEndRenderPass) \
 X(vkCmdBindPipeline) X(vkCmdBindVertexBuffers) X(vkCmdBindIndexBuffer) X(vkCmdDrawIndexed) \
 X(vkCmdSetViewport) X(vkCmdSetScissor) X(vkCmdPipelineBarrier) X(vkCmdCopyImage) X(vkCmdCopyImageToBuffer) \
 X(vkCreateSemaphore) X(vkDestroySemaphore) X(vkCreateFence) X(vkDestroyFence) \
 X(vkWaitForFences) X(vkResetFences) X(vkQueueSubmit)
struct Api {
  PFN_vkGetInstanceProcAddr get_instance=nullptr;
  PFN_vkCreateInstance vkCreateInstance=nullptr;
  PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion=nullptr;
  PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties=nullptr;
  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties=nullptr;
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT=nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT=nullptr;
#define OW_DECLARE(name) PFN_##name name=nullptr;
  OW_INSTANCE_FUNCTIONS(OW_DECLARE)
  OW_DEVICE_FUNCTIONS(OW_DECLARE)
#undef OW_DECLARE
  template<class T> void global(T& function,const char* name) {
    function=reinterpret_cast<T>(get_instance(VK_NULL_HANDLE,name));
    if (!function) fail("UNSUPPORTED_DEVICE","Required Vulkan loader entry point is unavailable.",true);
  }
  void load_instance(VkInstance instance) {
#define OW_LOAD_INSTANCE(name) name=reinterpret_cast<PFN_##name>(get_instance(instance,#name)); if(!name) fail("UNSUPPORTED_DEVICE","Required Vulkan instance entry point is unavailable.",true);
    OW_INSTANCE_FUNCTIONS(OW_LOAD_INSTANCE)
#undef OW_LOAD_INSTANCE
    vkCreateDebugUtilsMessengerEXT=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(get_instance(instance,"vkCreateDebugUtilsMessengerEXT"));
    vkDestroyDebugUtilsMessengerEXT=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(get_instance(instance,"vkDestroyDebugUtilsMessengerEXT"));
  }
  void load_device(VkDevice device) {
#define OW_LOAD_DEVICE(name) name=reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device,#name)); if(!name) fail("UNSUPPORTED_DEVICE","Required Vulkan device entry point is unavailable.",true);
    OW_DEVICE_FUNCTIONS(OW_LOAD_DEVICE)
#undef OW_LOAD_DEVICE
  }
};
struct Image { VkImage image=VK_NULL_HANDLE; VkDeviceMemory memory=VK_NULL_HANDLE; VkImageView view=VK_NULL_HANDLE; VkDeviceSize size=0; };
struct Buffer { VkBuffer buffer=VK_NULL_HANDLE; VkDeviceMemory memory=VK_NULL_HANDLE; VkDeviceSize size=0; };
class Context {
 public:
  explicit Context(RunResult& result) : result_(result), start_(std::chrono::steady_clock::now()) {}
  ~Context() { cleanup(); }
  void initialize(const std::array<Packet,2>& packets);
  void execute(const std::array<Packet,2>& packets,bool interactive);
  void initialize_capacity(std::size_t max_vertices,std::size_t max_indices);
  void execute_live(const LiveConfig& config,const LiveCallbacks& callbacks);
 private:
  Api api_;
  RunResult& result_;
  std::mutex validation_mutex_;
  std::chrono::steady_clock::time_point start_;
  SDL_Window* window_=nullptr;
  bool sdl_=false, minimized_=false, closed_=false, graphics_pending_=false, present_pending_=false, acquire_pending_=false, interactive_=false;
  bool maintenance_khr_=false;
  VkInstance instance_=VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger_=VK_NULL_HANDLE;
  VkPhysicalDevice physical_=VK_NULL_HANDLE;
  VkDevice device_=VK_NULL_HANDLE;
  VkQueue queue_=VK_NULL_HANDLE;
  VkSurfaceKHR surface_=VK_NULL_HANDLE;
  std::uint32_t queue_family_=0;
  VkPhysicalDeviceMemoryProperties memory_properties_{};
  presentation::Capabilities capabilities_;
  VkSwapchainKHR swapchain_=VK_NULL_HANDLE;
  std::vector<VkImage> swap_images_;
  VkFormat color_format_=VK_FORMAT_UNDEFINED;
  Extent extent_;
  std::uint64_t generation_=0, serial_=0, frame_count_=0, allocated_=0, max_frames_=120;
  bool live_=false;
  Image color_, ids_, depth_;
  Buffer vertices_, indices_, color_read_, id_read_, depth_read_;
  VkRenderPass render_pass_=VK_NULL_HANDLE;
  VkFramebuffer framebuffer_=VK_NULL_HANDLE;
  VkShaderModule vertex_shader_=VK_NULL_HANDLE,fragment_shader_=VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_=VK_NULL_HANDLE;
  VkPipeline pipeline_=VK_NULL_HANDLE;
  VkCommandPool command_pool_=VK_NULL_HANDLE;
  VkCommandBuffer command_=VK_NULL_HANDLE;
  VkSemaphore rendered_=VK_NULL_HANDLE;
  VkFence graphics_fence_=VK_NULL_HANDLE,present_fence_=VK_NULL_HANDLE,acquire_fence_=VK_NULL_HANDLE;
  static VKAPI_ATTR VkBool32 VKAPI_CALL validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void* user) {
    auto& self=*static_cast<Context*>(user);
    std::lock_guard lock(self.validation_mutex_);
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++self.result_.validation.errors;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ++self.result_.validation.warnings;
    try {
      if (self.result_.validation.messages.size()<128)
        self.result_.validation.messages.push_back({severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "error":"warning",safe_text(data->pMessageIdName)});
    } catch (...) {}
    return VK_FALSE;
  }
  void deadline() {
    if (!interactive_ && std::chrono::steady_clock::now()-start_ > std::chrono::seconds(30))
      fail("DEADLINE_EXCEEDED","Window verification exceeded its 30-second deadline.");
  }
  void lifecycle(const char* event) { result_.lifecycle_events.push_back({event,serial_,generation_,graphics_pending_,present_pending_}); }
  void pump();
  Extent pixels();
  void wait_work();
  void recreate();
  void retire_swapchain();
  void cleanup() noexcept;
  void make_image(Image& image,VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect);
  void make_buffer(Buffer& buffer,VkDeviceSize bytes,VkBufferUsageFlags usage);
  void free_image(Image& image);
  void free_buffer(Buffer& buffer);
  std::uint32_t memory_type(std::uint32_t bits,VkMemoryPropertyFlags required);
  void allocate(VkMemoryRequirements requirements,VkMemoryPropertyFlags flags,VkDeviceMemory& memory,VkDeviceSize& size);
  void pipeline();
  bool draw(const Packet& packet,const char* phase,bool capture);
  void capture(const Packet& packet,const char* phase,std::uint32_t image_index,VkResult presented);
};
void Context::initialize(const std::array<Packet,2>& packets) {
  initialize_capacity(std::max(packets[0].vertices.size(),packets[1].vertices.size()),
                      std::max(packets[0].indices.size(),packets[1].indices.size()));
}
void Context::initialize_capacity(std::size_t max_vertices,std::size_t max_indices) {
  SDL_SetMainReady();
  if (!SDL_Init(SDL_INIT_VIDEO)) fail("WINDOW_UNAVAILABLE","SDL video initialization failed; use an available desktop session.",true);
  sdl_=true;
  window_=SDL_CreateWindow("Omniweft: committed world cube",320,240,SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE);
  if (!window_) fail("UNSUPPORTED_DEVICE","Vulkan window creation failed; install a compatible Vulkan driver and use a desktop session.",true);
  api_.get_instance=reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
  if (!api_.get_instance) fail("UNSUPPORTED_DEVICE","Vulkan loader is unavailable; install a compatible graphics driver.",true);
  api_.global(api_.vkCreateInstance,"vkCreateInstance");
  api_.global(api_.vkEnumerateInstanceVersion,"vkEnumerateInstanceVersion");
  api_.global(api_.vkEnumerateInstanceExtensionProperties,"vkEnumerateInstanceExtensionProperties");
  api_.global(api_.vkEnumerateInstanceLayerProperties,"vkEnumerateInstanceLayerProperties");
  std::uint32_t runtime=0;
  check(api_.vkEnumerateInstanceVersion(&runtime),"Vulkan runtime version query failed.");
  if (runtime<VK_API_VERSION_1_3) fail("UNSUPPORTED_DEVICE","A Vulkan 1.3 or newer runtime is required.",true);
  std::uint32_t count=0;
  check(api_.vkEnumerateInstanceExtensionProperties(nullptr,&count,nullptr),"Instance extension query failed.");
  if(count>4096) fail("UNSUPPORTED_DEVICE","Instance extension count exceeds the bounded startup policy.",true);
  std::vector<VkExtensionProperties> extensions(count);
  check(api_.vkEnumerateInstanceExtensionProperties(nullptr,&count,extensions.data()),"Instance extension enumeration failed.");
  const auto has=[&](const char* name){return std::any_of(extensions.begin(),extensions.end(),[&](const auto& e){return std::strcmp(e.extensionName,name)==0;});};
  const bool surface_khr=has(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
  const bool surface_ext=has(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
  if ((!surface_khr&&!surface_ext) || !has(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
    fail("UNSUPPORTED_DEVICE","Presentation requires surface maintenance1 and get_surface_capabilities2.",true);
  if (!has(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) fail("VALIDATION_UNAVAILABLE","Vulkan debug-utils support is required for verification.",true);
  check(api_.vkEnumerateInstanceLayerProperties(&count,nullptr),"Instance layer query failed.");
  if(count>256) fail("VALIDATION_UNAVAILABLE","Instance layer count exceeds the bounded startup policy.",true);
  std::vector<VkLayerProperties> layers(count);
  check(api_.vkEnumerateInstanceLayerProperties(&count,layers.data()),"Instance layer enumeration failed.");
  const char* layer="VK_LAYER_KHRONOS_validation";
  if (!std::any_of(layers.begin(),layers.end(),[&](const auto& l){return std::strcmp(l.layerName,layer)==0;}))
    fail("VALIDATION_UNAVAILABLE","Install the pinned Khronos validation tools and set a process-local layer path.",true);
  Uint32 sdl_count=0;
  const char* const* sdl_extensions=SDL_Vulkan_GetInstanceExtensions(&sdl_count);
  if(!sdl_extensions) fail("UNSUPPORTED_DEVICE","SDL could not identify the required Vulkan surface extensions.",true);
  std::vector<const char*> enabled(sdl_extensions,sdl_extensions+sdl_count);
  enabled.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
  if(surface_khr) enabled.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
  if(surface_ext) enabled.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
  enabled.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  check(api_.vkEnumerateInstanceExtensionProperties(layer,&count,nullptr),"Validation extension query failed.");
  if(count>256) fail("VALIDATION_UNAVAILABLE","Validation extension count exceeds startup policy.",true);
  std::vector<VkExtensionProperties> layer_extensions(count);
  check(api_.vkEnumerateInstanceExtensionProperties(layer,&count,layer_extensions.data()),"Validation extension enumeration failed.");
  if(!std::any_of(layer_extensions.begin(),layer_extensions.end(),[](const auto& e){return std::strcmp(e.extensionName,VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME)==0;}))
    fail("VALIDATION_UNAVAILABLE","Pinned validation feature controls are unavailable.",true);
  enabled.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
  auto application=info<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
  application.pApplicationName="Omniweft"; application.apiVersion=VK_API_VERSION_1_3;
  auto debug=info<VkDebugUtilsMessengerCreateInfoEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
  debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug.pfnUserCallback=validation; debug.pUserData=this;
  const VkValidationFeatureEnableEXT synchronization=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
  auto validation_features=info<VkValidationFeaturesEXT>(VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT);
  validation_features.enabledValidationFeatureCount=1; validation_features.pEnabledValidationFeatures=&synchronization;
  validation_features.pNext=&debug;
  auto create=info<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
  create.pApplicationInfo=&application; create.enabledExtensionCount=static_cast<std::uint32_t>(enabled.size());
  create.ppEnabledExtensionNames=enabled.data(); create.enabledLayerCount=1; create.ppEnabledLayerNames=&layer;
  create.pNext=&validation_features;
  check(api_.vkCreateInstance(&create,nullptr,&instance_),"Vulkan instance creation failed.");
  api_.load_instance(instance_);
  if(!api_.vkCreateDebugUtilsMessengerEXT || !api_.vkDestroyDebugUtilsMessengerEXT)
    fail("VALIDATION_UNAVAILABLE","Vulkan debug callback entry points are unavailable.",true);
  check(api_.vkCreateDebugUtilsMessengerEXT(instance_,&debug,nullptr,&messenger_),"Validation callback creation failed.");
  result_.validation.enabled=true;
  if (!SDL_Vulkan_CreateSurface(window_,instance_,nullptr,&surface_))
    fail("UNSUPPORTED_DEVICE","Window surface creation failed.",true);
  check(api_.vkEnumeratePhysicalDevices(instance_,&count,nullptr),"Device query failed.");
  if(!count || count>32) fail("UNSUPPORTED_DEVICE","No bounded list of Vulkan devices is available.",true);
  std::vector<VkPhysicalDevice> devices(count);
  check(api_.vkEnumeratePhysicalDevices(instance_,&count,devices.data()),"Device enumeration failed.");
  // Prefer a discrete device, while preserving a truthful software-device classification.
  std::stable_sort(devices.begin(),devices.end(),[&](auto a,auto b){
    VkPhysicalDeviceProperties pa{},pb{};api_.vkGetPhysicalDeviceProperties(a,&pa);api_.vkGetPhysicalDeviceProperties(b,&pb);
    return (pa.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)>(pb.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
  });
  for (auto candidate:devices) {
    VkPhysicalDeviceProperties properties{};api_.vkGetPhysicalDeviceProperties(candidate,&properties);
    if(properties.apiVersion<VK_API_VERSION_1_3) continue;
    check(api_.vkEnumerateDeviceExtensionProperties(candidate,nullptr,&count,nullptr),"Device extension query failed.");
    if(count>4096) continue;
    std::vector<VkExtensionProperties> device_extensions(count);
    check(api_.vkEnumerateDeviceExtensionProperties(candidate,nullptr,&count,device_extensions.data()),"Device extension enumeration failed.");
    const auto available=[&](const char* name){return std::any_of(device_extensions.begin(),device_extensions.end(),[&](const auto& e){return std::strcmp(e.extensionName,name)==0;});};
    const bool candidate_khr=surface_khr&&available(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    const bool candidate_ext=surface_ext&&available(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    if(!available(VK_KHR_SWAPCHAIN_EXTENSION_NAME) || (!candidate_khr&&!candidate_ext)) continue;
    auto maintenance=info<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR);
    auto features=info<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    features.pNext=&maintenance;api_.vkGetPhysicalDeviceFeatures2(candidate,&features);
    if(!maintenance.swapchainMaintenance1) continue;
    api_.vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,nullptr);
    if(count>256) continue;
    std::vector<VkQueueFamilyProperties> families(count);
    api_.vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,families.data());
    for(std::uint32_t family=0;family<count;++family) {
      VkBool32 supported=VK_FALSE;
      check(api_.vkGetPhysicalDeviceSurfaceSupportKHR(candidate,family,surface_,&supported),"Present support query failed.");
      if(supported && (families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)) {
        physical_=candidate;queue_family_=family;break;
      }
    }
    if(physical_) {
      maintenance_khr_=candidate_khr;
      capabilities_.runtime_1_3=properties.apiVersion>=VK_API_VERSION_1_3;
      capabilities_.graphics_present_queue=true;
      capabilities_.maintenance_extensions=true;
      capabilities_.maintenance_feature=maintenance.swapchainMaintenance1!=0;
      result_.device={safe_text(properties.deviceName),version(properties.apiVersion),std::to_string(properties.driverVersion),
        properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU?"cpu":properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?"discrete_gpu":properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?"integrated_gpu":"other"};
      break;
    }
  }
  if(!physical_) fail("UNSUPPORTED_DEVICE","No Vulkan 1.3 graphics/present device supports the required swapchain maintenance feature.",true);
  api_.vkGetPhysicalDeviceMemoryProperties(physical_,&memory_properties_);
  const float priority=1;
  auto queue=info<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
  queue.queueFamilyIndex=queue_family_;queue.queueCount=1;queue.pQueuePriorities=&priority;
  const char* device_extensions[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME,maintenance_khr_?VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME:VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
  auto maintenance=info<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR);
  maintenance.swapchainMaintenance1=VK_TRUE;
  auto device_create=info<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
  device_create.queueCreateInfoCount=1;device_create.pQueueCreateInfos=&queue;
  device_create.enabledExtensionCount=2;device_create.ppEnabledExtensionNames=device_extensions;device_create.pNext=&maintenance;
  check(api_.vkCreateDevice(physical_,&device_create,nullptr,&device_),"Logical device creation failed.");
  api_.load_device(device_);api_.vkGetDeviceQueue(device_,queue_family_,0,&queue_);
  auto pool=info<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
  pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pool.queueFamilyIndex=queue_family_;
  check(api_.vkCreateCommandPool(device_,&pool,nullptr,&command_pool_),"Command pool creation failed.");
  auto command=info<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  command.commandPool=command_pool_;command.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;command.commandBufferCount=1;
  check(api_.vkAllocateCommandBuffers(device_,&command,&command_),"Command buffer allocation failed.");
  auto semaphore=info<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  check(api_.vkCreateSemaphore(device_,&semaphore,nullptr,&rendered_),"Present semaphore creation failed.");
  auto fence=info<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  check(api_.vkCreateFence(device_,&fence,nullptr,&acquire_fence_),"Acquisition fence creation failed.");
  check(api_.vkCreateFence(device_,&fence,nullptr,&graphics_fence_),"Graphics fence creation failed.");
  check(api_.vkCreateFence(device_,&fence,nullptr,&present_fence_),"Presentation fence creation failed.");
  if(max_vertices>24*1024 || max_indices>36*1024) fail("BUDGET_EXCEEDED","Presentation packet exceeds the mesh budget.");
  make_buffer(vertices_,std::max<std::size_t>(1,max_vertices)*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  make_buffer(indices_,std::max<std::size_t>(1,max_indices)*sizeof(std::uint32_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  recreate();
}
std::uint32_t Context::memory_type(std::uint32_t bits,VkMemoryPropertyFlags required) {
  for(std::uint32_t i=0;i<memory_properties_.memoryTypeCount;++i)
    if((bits&(1U<<i)) && (memory_properties_.memoryTypes[i].propertyFlags&required)==required) return i;
  fail("UNSUPPORTED_DEVICE","Required coherent host or device memory type is unavailable.",true);
}
void Context::allocate(VkMemoryRequirements requirements,VkMemoryPropertyFlags flags,VkDeviceMemory& memory,VkDeviceSize& size) {
  if(!presentation::allocation_fits(allocated_,requirements.size)) fail("BUDGET_EXCEEDED","Owned GPU allocation budget exceeds 64 MiB.");
  auto allocation=info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=memory_type(requirements.memoryTypeBits,flags);
  check(api_.vkAllocateMemory(device_,&allocation,nullptr,&memory),"GPU memory allocation failed.");
  size=requirements.size;allocated_+=size;
}
void Context::make_buffer(Buffer& buffer,VkDeviceSize bytes,VkBufferUsageFlags usage) {
  if(!presentation::allocation_fits(allocated_,bytes)) fail("BUDGET_EXCEEDED","Buffer allocation would exceed 64 MiB.");
  auto create=info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  create.size=bytes;create.usage=usage;create.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  check(api_.vkCreateBuffer(device_,&create,nullptr,&buffer.buffer),"Buffer creation failed.");
  VkMemoryRequirements requirements{};api_.vkGetBufferMemoryRequirements(device_,buffer.buffer,&requirements);
  allocate(requirements,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,buffer.memory,buffer.size);
  check(api_.vkBindBufferMemory(device_,buffer.buffer,buffer.memory,0),"Buffer memory binding failed.");
}
void Context::make_image(Image& image,VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect) {
  const auto pixels=presentation::checked_pixel_count(extent_);
  if(!presentation::allocation_fits(allocated_,pixels*4)) fail("BUDGET_EXCEEDED","Image allocation would exceed 64 MiB.");
  auto create=info<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
  create.imageType=VK_IMAGE_TYPE_2D;create.format=format;create.extent={extent_.width,extent_.height,1};
  create.mipLevels=1;create.arrayLayers=1;create.samples=VK_SAMPLE_COUNT_1_BIT;
  create.tiling=VK_IMAGE_TILING_OPTIMAL;create.usage=usage;create.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  check(api_.vkCreateImage(device_,&create,nullptr,&image.image),"Attachment image creation failed.");
  VkMemoryRequirements requirements{};api_.vkGetImageMemoryRequirements(device_,image.image,&requirements);
  allocate(requirements,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,image.memory,image.size);
  check(api_.vkBindImageMemory(device_,image.image,image.memory,0),"Attachment memory binding failed.");
  auto view=info<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  view.image=image.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;
  view.subresourceRange={aspect,0,1,0,1};
  check(api_.vkCreateImageView(device_,&view,nullptr,&image.view),"Attachment view creation failed.");
}

Extent Context::pixels() {
  int width=0,height=0;
  if(!SDL_GetWindowSizeInPixels(window_,&width,&height)) fail("WINDOW_ERROR","Window pixel extent query failed.");
  if(width<=0 || height<=0) return {};
  return {static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height)};
}
void Context::pump() {
  SDL_Event event;
  while(SDL_PollEvent(&event)) {
    if(event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) closed_=true;
    const char* kind=nullptr;
    if(event.type==SDL_EVENT_WINDOW_MINIMIZED) { minimized_=true;kind="minimized"; }
    if(event.type==SDL_EVENT_WINDOW_RESTORED) { minimized_=false;kind="restored"; }
    if(event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) kind="resized";
    if(kind) result_.window_events.push_back({kind,pixels(),minimized_,serial_,generation_});
  }
  minimized_=(SDL_GetWindowFlags(window_)&SDL_WINDOW_MINIMIZED)!=0;
}
void Context::wait_work() {
  if(acquire_pending_) {
    const auto result=api_.vkWaitForFences(device_,1,&acquire_fence_,VK_TRUE,5000000000ULL);
    if(result==VK_TIMEOUT) fail("GPU_TIMEOUT","Acquisition fence did not signal within five seconds.");
    check(result,"Acquisition fence wait failed.");
    acquire_pending_=false;
    check(api_.vkResetFences(device_,1,&acquire_fence_),"Acquisition fence reset failed.");
  }
  if(graphics_pending_) {
    const auto result=api_.vkWaitForFences(device_,1,&graphics_fence_,VK_TRUE,5000000000ULL);
    if(result==VK_TIMEOUT) fail("GPU_TIMEOUT","Graphics fence did not signal within five seconds.");
    check(result,"Graphics fence wait failed.");
    graphics_pending_=false;lifecycle("graphics_complete");
  }
  if(present_pending_) {
    const auto result=api_.vkWaitForFences(device_,1,&present_fence_,VK_TRUE,5000000000ULL);
    if(result==VK_TIMEOUT) fail("GPU_TIMEOUT","Presentation fence did not signal within five seconds.");
    check(result,"Presentation fence wait failed.");
    present_pending_=false;lifecycle("present_complete");
  }
}
void Context::free_image(Image& image) {
  if(image.view) api_.vkDestroyImageView(device_,image.view,nullptr);
  if(image.image) api_.vkDestroyImage(device_,image.image,nullptr);
  if(image.memory) { api_.vkFreeMemory(device_,image.memory,nullptr);allocated_-=image.size; }
  image={};
}
void Context::free_buffer(Buffer& buffer) {
  if(buffer.buffer) api_.vkDestroyBuffer(device_,buffer.buffer,nullptr);
  if(buffer.memory) { api_.vkFreeMemory(device_,buffer.memory,nullptr);allocated_-=buffer.size; }
  buffer={};
}
void Context::retire_swapchain() {
  wait_work();
  if(pipeline_) api_.vkDestroyPipeline(device_,pipeline_,nullptr);
  if(pipeline_layout_) api_.vkDestroyPipelineLayout(device_,pipeline_layout_,nullptr);
  if(vertex_shader_) api_.vkDestroyShaderModule(device_,vertex_shader_,nullptr);
  if(fragment_shader_) api_.vkDestroyShaderModule(device_,fragment_shader_,nullptr);
  pipeline_=VK_NULL_HANDLE;pipeline_layout_=VK_NULL_HANDLE;vertex_shader_=VK_NULL_HANDLE;fragment_shader_=VK_NULL_HANDLE;
  if(framebuffer_) api_.vkDestroyFramebuffer(device_,framebuffer_,nullptr);
  framebuffer_=VK_NULL_HANDLE;
  if(render_pass_) api_.vkDestroyRenderPass(device_,render_pass_,nullptr);
  render_pass_=VK_NULL_HANDLE;
  free_image(color_);free_image(ids_);free_image(depth_);
  free_buffer(color_read_);free_buffer(id_read_);free_buffer(depth_read_);
  if(swapchain_) {
    api_.vkDestroySwapchainKHR(device_,swapchain_,nullptr);swapchain_=VK_NULL_HANDLE;
    lifecycle("retired");
  }
  swap_images_.clear();
}
void Context::recreate() {
  wait_work();
  const auto actual=pixels();
  if(!actual.width || !actual.height || minimized_) return;
  static_cast<void>(presentation::checked_pixel_count(actual));
  VkSurfaceCapabilitiesKHR capabilities{};
  check(api_.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_,surface_,&capabilities),"Surface capability query failed.");
  capabilities_.swapchain_transfer_destination=(capabilities.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)!=0;
  std::uint32_t count=0;
  check(api_.vkGetPhysicalDeviceSurfaceFormatsKHR(physical_,surface_,&count,nullptr),"Surface format query failed.");
  if(!count || count>256) fail("UNSUPPORTED_DEVICE","No bounded list of surface formats is available.",true);
  std::vector<VkSurfaceFormatKHR> formats(count);
  check(api_.vkGetPhysicalDeviceSurfaceFormatsKHR(physical_,surface_,&count,formats.data()),"Surface format enumeration failed.");
  auto found=std::find_if(formats.begin(),formats.end(),[](const auto& f){return f.format==VK_FORMAT_R8G8B8A8_UNORM;});
  if(found==formats.end()) found=std::find_if(formats.begin(),formats.end(),[](const auto& f){return f.format==VK_FORMAT_B8G8R8A8_UNORM;});
  capabilities_.unorm_surface=found!=formats.end();
  capabilities_.color_attachment=false;capabilities_.id_attachment=false;capabilities_.depth_attachment=false;
  capabilities_.transfer_source=capabilities_.unorm_surface;
  const std::array<VkFormat,3> checked_formats={found==formats.end()?VK_FORMAT_UNDEFINED:found->format,VK_FORMAT_R32_UINT,VK_FORMAT_D32_SFLOAT};
  for(std::size_t i=0;i<checked_formats.size();++i) {
    if(checked_formats[i]==VK_FORMAT_UNDEFINED) continue;
    VkFormatProperties properties{};api_.vkGetPhysicalDeviceFormatProperties(physical_,checked_formats[i],&properties);
    const auto attachment=i==2?VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT:VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    const bool supported=(properties.optimalTilingFeatures&attachment)!=0;
    if(i==0) capabilities_.color_attachment=supported;
    if(i==1) capabilities_.id_attachment=supported;
    if(i==2) capabilities_.depth_attachment=supported;
    capabilities_.transfer_source=capabilities_.transfer_source&&((properties.optimalTilingFeatures&VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)!=0);
  }
  const auto missing=presentation::missing_capabilities(capabilities_);
  if(!missing.empty()) fail("UNSUPPORTED_DEVICE",("Required capability is unavailable: "+missing.front()).c_str(),true);
  Extent next=capabilities.currentExtent.width==std::numeric_limits<std::uint32_t>::max()
    ?actual:Extent{capabilities.currentExtent.width,capabilities.currentExtent.height};
  static_cast<void>(presentation::checked_pixel_count(next));
  if(next.width<capabilities.minImageExtent.width || next.height<capabilities.minImageExtent.height ||
     next.width>capabilities.maxImageExtent.width || next.height>capabilities.maxImageExtent.height)
    fail("UNSUPPORTED_DEVICE","The actual window extent is outside supported surface limits.",true);
  if(capabilities.minImageCount>3) fail("UNSUPPORTED_DEVICE","The surface requires more than three swapchain images.",true);
  std::uint32_t desired=std::min(3U,capabilities.minImageCount+1);
  if(capabilities.maxImageCount) desired=std::min(desired,capabilities.maxImageCount);
  retire_swapchain();
  extent_=next;color_format_=found->format;++generation_;
  auto create=info<VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
  create.surface=surface_;create.minImageCount=desired;create.imageFormat=color_format_;create.imageColorSpace=found->colorSpace;
  create.imageExtent={extent_.width,extent_.height};create.imageArrayLayers=1;create.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  create.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;create.preTransform=capabilities.currentTransform;
  for(const auto alpha:{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
    if(capabilities.supportedCompositeAlpha&alpha) {create.compositeAlpha=alpha;break;}
  create.presentMode=VK_PRESENT_MODE_FIFO_KHR;create.clipped=VK_TRUE;
  check(api_.vkCreateSwapchainKHR(device_,&create,nullptr,&swapchain_),"Swapchain creation failed.");
  check(api_.vkGetSwapchainImagesKHR(device_,swapchain_,&count,nullptr),"Swapchain image query failed.");
  if(!count || count>3) fail("UNSUPPORTED_DEVICE","The created swapchain exceeds the three-image policy.",true);
  swap_images_.resize(count);
  check(api_.vkGetSwapchainImagesKHR(device_,swapchain_,&count,swap_images_.data()),"Swapchain image enumeration failed.");
  make_image(color_,color_format_,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
  make_image(ids_,VK_FORMAT_R32_UINT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
  make_image(depth_,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
  const auto bytes=presentation::checked_pixel_count(extent_)*4;
  make_buffer(color_read_,bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  make_buffer(id_read_,bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  make_buffer(depth_read_,bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  pipeline();
}
void Context::pipeline() {
  std::array<VkAttachmentDescription,3> attachments{};
  const VkFormat formats[]={color_format_,VK_FORMAT_R32_UINT,VK_FORMAT_D32_SFLOAT};
  for(std::size_t i=0;i<attachments.size();++i) {
    auto& a=attachments[i];a.format=formats[i];a.samples=VK_SAMPLE_COUNT_1_BIT;
    a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  }
  const VkAttachmentReference colors[]={{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
  const VkAttachmentReference depth{2,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount=2;subpass.pColorAttachments=colors;subpass.pDepthStencilAttachment=&depth;
  const VkPipelineStageFlags attachment_stages=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  const VkAccessFlags attachment_writes=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  const VkSubpassDependency dependencies[]={
    {VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,attachment_stages,0,attachment_writes,0},
    {0,VK_SUBPASS_EXTERNAL,attachment_stages,VK_PIPELINE_STAGE_TRANSFER_BIT,attachment_writes,VK_ACCESS_TRANSFER_READ_BIT,0}};
  auto pass=info<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
  pass.attachmentCount=3;pass.pAttachments=attachments.data();pass.subpassCount=1;pass.pSubpasses=&subpass;
  pass.dependencyCount=2;pass.pDependencies=dependencies;
  check(api_.vkCreateRenderPass(device_,&pass,nullptr,&render_pass_),"Render pass creation failed.");
  const VkImageView views[]={color_.view,ids_.view,depth_.view};
  auto framebuffer=info<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
  framebuffer.renderPass=render_pass_;framebuffer.attachmentCount=3;framebuffer.pAttachments=views;
  framebuffer.width=extent_.width;framebuffer.height=extent_.height;framebuffer.layers=1;
  check(api_.vkCreateFramebuffer(device_,&framebuffer,nullptr,&framebuffer_),"Framebuffer creation failed.");
  auto shader=info<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
  shader.codeSize=sizeof(shaders::vertex);shader.pCode=shaders::vertex;
  check(api_.vkCreateShaderModule(device_,&shader,nullptr,&vertex_shader_),"Vertex shader creation failed.");
  shader.codeSize=sizeof(shaders::fragment);shader.pCode=shaders::fragment;
  check(api_.vkCreateShaderModule(device_,&shader,nullptr,&fragment_shader_),"Fragment shader creation failed.");
  auto layout=info<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
  check(api_.vkCreatePipelineLayout(device_,&layout,nullptr,&pipeline_layout_),"Pipeline layout creation failed.");
  auto vertex_stage=info<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
  vertex_stage.stage=VK_SHADER_STAGE_VERTEX_BIT;vertex_stage.module=vertex_shader_;vertex_stage.pName="main";
  auto fragment_stage=vertex_stage;fragment_stage.stage=VK_SHADER_STAGE_FRAGMENT_BIT;fragment_stage.module=fragment_shader_;
  const VkPipelineShaderStageCreateInfo stages[]={vertex_stage,fragment_stage};
  static_assert(sizeof(Vertex)==36 && offsetof(Vertex,color)==16 && offsetof(Vertex,object_id)==32);
  const VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
  const VkVertexInputAttributeDescription attributes[]={
    {0,0,VK_FORMAT_R32G32B32A32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32A32_SFLOAT,16},{2,0,VK_FORMAT_R32_UINT,32}};
  auto vertex=info<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
  vertex.vertexBindingDescriptionCount=1;vertex.pVertexBindingDescriptions=&binding;
  vertex.vertexAttributeDescriptionCount=3;vertex.pVertexAttributeDescriptions=attributes;
  auto assembly=info<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
  assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  auto viewport=info<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
  viewport.viewportCount=1;viewport.scissorCount=1;
  auto raster=info<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
  raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
  auto multisample=info<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
  auto depth_state=info<VkPipelineDepthStencilStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
  depth_state.depthTestEnable=VK_TRUE;depth_state.depthWriteEnable=VK_TRUE;depth_state.depthCompareOp=VK_COMPARE_OP_LESS;
  std::array<VkPipelineColorBlendAttachmentState,2> blends{};
  for(auto& blend:blends) blend.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
  auto blend=info<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
  blend.attachmentCount=2;blend.pAttachments=blends.data();
  const VkDynamicState dynamic_states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
  auto dynamic=info<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamic_states;
  auto create=info<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
  create.stageCount=2;create.pStages=stages;create.pVertexInputState=&vertex;create.pInputAssemblyState=&assembly;
  create.pViewportState=&viewport;create.pRasterizationState=&raster;create.pMultisampleState=&multisample;
  create.pDepthStencilState=&depth_state;create.pColorBlendState=&blend;create.pDynamicState=&dynamic;
  create.layout=pipeline_layout_;create.renderPass=render_pass_;
  check(api_.vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&create,nullptr,&pipeline_),"Graphics pipeline creation failed.");
}

bool Context::draw(const Packet& packet,const char* phase,bool retain) {
  deadline();pump();
  if(closed_) fail("WINDOW_CLOSED","The window closed before verification completed.");
  if(minimized_) return false;
  const auto actual=pixels();
  if(!actual.width || !actual.height) return false;
  static_cast<void>(presentation::checked_pixel_count(actual));
  wait_work();
  if(actual!=extent_ || !swapchain_) recreate();
  if((!interactive_ || live_) && frame_count_>=max_frames_) fail("FRAME_BUDGET_EXCEEDED","Presentation exceeded its submitted-frame budget.");
  std::uint32_t image_index=0;
  auto acquired=api_.vkAcquireNextImageKHR(device_,swapchain_,100000000ULL,VK_NULL_HANDLE,acquire_fence_,&image_index);
  if(acquired==VK_TIMEOUT || acquired==VK_NOT_READY) return false;
  if(acquired==VK_ERROR_OUT_OF_DATE_KHR) {recreate();return false;}
  if(acquired!=VK_SUCCESS && acquired!=VK_SUBOPTIMAL_KHR) check(acquired,"Swapchain acquisition failed.");
  acquire_pending_=true;wait_work();
  if(image_index>=swap_images_.size()) fail("VULKAN_ERROR","Acquired image index is outside the bounded swapchain.");
  if(packet.vertices.size()*sizeof(Vertex)>vertices_.size || packet.indices.size()*sizeof(std::uint32_t)>indices_.size)
    fail("BUDGET_EXCEEDED","Packet exceeds its allocated mesh buffers.");
  for(auto index:packet.indices) if(index>=packet.vertices.size()) fail("INVALID_PACKET","Packet index is outside its vertex array.");
  void* mapped=nullptr;
  check(api_.vkMapMemory(device_,vertices_.memory,0,vertices_.size,0,&mapped),"Vertex mapping failed.");
  if(!packet.vertices.empty()) std::memcpy(mapped,packet.vertices.data(),packet.vertices.size()*sizeof(Vertex));
  api_.vkUnmapMemory(device_,vertices_.memory);
  check(api_.vkMapMemory(device_,indices_.memory,0,indices_.size,0,&mapped),"Index mapping failed.");
  if(!packet.indices.empty()) std::memcpy(mapped,packet.indices.data(),packet.indices.size()*sizeof(std::uint32_t));
  api_.vkUnmapMemory(device_,indices_.memory);
  check(api_.vkResetCommandBuffer(command_,0),"Command buffer reset failed.");
  auto begin=info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(api_.vkBeginCommandBuffer(command_,&begin),"Command recording failed.");
  std::array<VkClearValue,3> clears{};
  clears[0].color.float32[0]=16.0F/255;clears[0].color.float32[1]=20.0F/255;
  clears[0].color.float32[2]=24.0F/255;clears[0].color.float32[3]=1;
  clears[1].color.uint32[0]=0;clears[2].depthStencil={1,0};
  auto pass=info<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
  pass.renderPass=render_pass_;pass.framebuffer=framebuffer_;pass.renderArea.extent={extent_.width,extent_.height};
  pass.clearValueCount=3;pass.pClearValues=clears.data();
  api_.vkCmdBeginRenderPass(command_,&pass,VK_SUBPASS_CONTENTS_INLINE);
  api_.vkCmdBindPipeline(command_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_);
  const VkViewport viewport{0,0,static_cast<float>(extent_.width),static_cast<float>(extent_.height),0,1};
  const VkRect2D scissor{{0,0},{extent_.width,extent_.height}};
  api_.vkCmdSetViewport(command_,0,1,&viewport);api_.vkCmdSetScissor(command_,0,1,&scissor);
  const VkDeviceSize offset=0;
  api_.vkCmdBindVertexBuffers(command_,0,1,&vertices_.buffer,&offset);
  api_.vkCmdBindIndexBuffer(command_,indices_.buffer,0,VK_INDEX_TYPE_UINT32);
  api_.vkCmdDrawIndexed(command_,static_cast<std::uint32_t>(packet.indices.size()),1,0,0,0);
  api_.vkCmdEndRenderPass(command_);
  // Acquisition was completed on the host fence. Discard the previous presented contents.
  auto barrier=info<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
  barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.image=swap_images_[image_index];
  barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
  api_.vkCmdPipelineBarrier(command_,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
  VkImageCopy copy{};copy.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.dstSubresource=copy.srcSubresource;
  copy.extent={extent_.width,extent_.height,1};
  // The exact readback color source is copied, without conversion or filtering, to the presented image.
  api_.vkCmdCopyImage(command_,color_.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,swap_images_[image_index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
  const auto readback=[&](VkImage image,VkBuffer buffer,VkImageAspectFlags aspect) {
    VkBufferImageCopy region{};region.imageSubresource={aspect,0,0,1};region.imageExtent={extent_.width,extent_.height,1};
    api_.vkCmdCopyImageToBuffer(command_,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&region);
  };
  readback(color_.image,color_read_.buffer,VK_IMAGE_ASPECT_COLOR_BIT);
  readback(ids_.image,id_read_.buffer,VK_IMAGE_ASPECT_COLOR_BIT);
  readback(depth_.image,depth_read_.buffer,VK_IMAGE_ASPECT_DEPTH_BIT);
  barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=0;
  api_.vkCmdPipelineBarrier(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,0,nullptr,1,&barrier);
  auto host=info<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
  host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  api_.vkCmdPipelineBarrier(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
  check(api_.vkEndCommandBuffer(command_),"Command recording completion failed.");
  check(api_.vkResetFences(device_,1,&graphics_fence_),"Graphics fence reset failed.");
  auto submit=info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
  submit.commandBufferCount=1;submit.pCommandBuffers=&command_;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&rendered_;
  check(api_.vkQueueSubmit(queue_,1,&submit,graphics_fence_),"Graphics submission failed.");
  graphics_pending_=true;++serial_;++frame_count_;lifecycle("submit");
  check(api_.vkResetFences(device_,1,&present_fence_),"Presentation fence reset failed.");
  auto fence=info<VkSwapchainPresentFenceInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR);
  fence.swapchainCount=1;fence.pFences=&present_fence_;
  auto present=info<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
  present.pNext=&fence;present.waitSemaphoreCount=1;present.pWaitSemaphores=&rendered_;
  present.swapchainCount=1;present.pSwapchains=&swapchain_;present.pImageIndices=&image_index;
  const auto presented=api_.vkQueuePresentKHR(queue_,&present);
  if(presented==VK_SUCCESS || presented==VK_SUBOPTIMAL_KHR || presented==VK_ERROR_OUT_OF_DATE_KHR || presented==VK_ERROR_SURFACE_LOST_KHR) {
    present_pending_=true;lifecycle("present_queued");
  }
  if(presented==VK_ERROR_OUT_OF_DATE_KHR) {recreate();return false;}
  if(presented!=VK_SUCCESS && presented!=VK_SUBOPTIMAL_KHR) check(presented,"Presentation failed.");
  if(retain) capture(packet,phase,image_index,presented);
  return true;
}
void Context::capture(const Packet& packet,const char* phase,std::uint32_t image_index,VkResult presented) {
  wait_work();
  const auto pixels_count=presentation::checked_pixel_count(extent_);
  FrameCapture frame;
  frame.phase=phase;frame.frame_id=frame_count_;frame.source_generation=generation_;frame.swapchain_generation=generation_;
  frame.submission_serial=serial_;frame.image_index=image_index;frame.pixel_extent=extent_;frame.packet=packet;
  frame.present_result=presented==VK_SUCCESS?"VK_SUCCESS":"VK_SUBOPTIMAL_KHR";
  frame.color_format=color_format_==VK_FORMAT_R8G8B8A8_UNORM?"R8G8B8A8_UNORM":"B8G8R8A8_UNORM";
  frame.color.resize(static_cast<std::size_t>(pixels_count)*4);
  frame.object_ids.resize(static_cast<std::size_t>(pixels_count));
  frame.depth.resize(static_cast<std::size_t>(pixels_count));
  const auto copy=[&](const Buffer& buffer,void* destination) {
    void* data=nullptr;check(api_.vkMapMemory(device_,buffer.memory,0,buffer.size,0,&data),"Readback mapping failed.");
    std::memcpy(destination,data,static_cast<std::size_t>(pixels_count)*4);api_.vkUnmapMemory(device_,buffer.memory);
  };
  copy(color_read_,frame.color.data());copy(id_read_,frame.object_ids.data());copy(depth_read_,frame.depth.data());
  result_.frames.push_back(std::move(frame));
}
void Context::execute(const std::array<Packet,2>& packets,bool interactive) {
  const auto present_until=[&](const Packet& packet,const char* phase,bool retain) {
    while(!draw(packet,phase,retain)) {deadline();SDL_Delay(5);}
  };
  present_until(packets[0],"initial",true);
  present_until(packets[1],"transformed",true);
  // Leave real work queued while asking the OS to resize.
  present_until(packets[1],"",false);
  lifecycle("resize_requested");
  const auto event_start=result_.window_events.size();
  if(!SDL_SetWindowSize(window_,400,300)) fail("WINDOW_ERROR","Window resize request failed.");
  const auto wait_event=[&](const char* kind,std::size_t from,bool expected_minimized) {
    const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for(;;) {
      deadline();pump();
      if(closed_) fail("WINDOW_CLOSED","The window closed during lifecycle verification.");
      const bool observed=std::any_of(result_.window_events.begin()+static_cast<std::ptrdiff_t>(from),result_.window_events.end(),
        [&](const WindowEvent& event){return event.kind==kind;});
      if(observed && minimized_==expected_minimized) return;
      if(std::chrono::steady_clock::now()>=limit) fail("WINDOW_TRANSITION_UNAVAILABLE","The desktop did not confirm the requested window transition.");
      SDL_Delay(10);
    }
  };
  wait_event("resized",event_start,false);
  if(pixels()==extent_) fail("WINDOW_TRANSITION_UNAVAILABLE","The resize did not change the actual pixel extent.");
  recreate();
  present_until(packets[1],"resized",true);
  const auto minimize_start=result_.window_events.size();
  lifecycle("minimize_requested");
  if(!SDL_MinimizeWindow(window_)) fail("WINDOW_ERROR","Window minimize request failed.");
  wait_event("minimized",minimize_start,true);
  // No acquisition, allocation or submission occurs while the actual minimized state is set.
  SDL_Delay(50);pump();
  const auto restore_start=result_.window_events.size();
  lifecycle("restore_requested");
  if(!SDL_RestoreWindow(window_)) fail("WINDOW_ERROR","Window restore request failed.");
  wait_event("restored",restore_start,false);
  present_until(packets[1],"restored",true);
  if(interactive) {
    interactive_=true;
    while(!closed_) {pump();if(closed_) break;if(!minimized_) draw(packets[1],"",false);SDL_Delay(8);}
  } else {
    // Final teardown also retires a real queued graphics/present operation.
    present_until(packets[1],"",false);
  }
}
void Context::execute_live(const LiveConfig& config,const LiveCallbacks& callbacks) {
  live_=true;interactive_=config.interactive;max_frames_=interactive_?40000:600;
  bool initial=false,final=false;
  callbacks.ready();
  while(!callbacks.should_stop()) {
    deadline();pump();if(closed_) break;
    const auto current=callbacks.latest();
    const auto revision=current.packet.world_revision;
    const char* phase=nullptr;
    if(!initial && revision==1) phase="initial";
    if(initial && !final && revision==3) phase="final";
    if(!initial && revision>1) fail("MISSED_LIVE_REVISION","The initial live revision was not presented.");
    if(!minimized_ && draw(current.packet,phase?phase:"",phase!=nullptr)) {
      // Publish presentation progress only after actual graphics AND present
      // completion. A queued present alone is not visible-completion evidence.
      wait_work();
      if(phase) {
        auto& capture=result_.frames.back();
        capture.simulation_tick=current.simulation_tick;capture.snapshot_sequence=current.snapshot_sequence;
        if(revision==1) initial=true;else final=true;
      }
      callbacks.presented(current,frame_count_);
    }
    SDL_Delay(16);
  }
  if(!initial || !final) fail("INCOMPLETE_LIVE_SCENE","Both live committed scene revisions must be presented.");
}
void Context::cleanup() noexcept {
  try {
    if(device_ && api_.vkWaitForFences) wait_work();
  } catch(...) {
    // An unsignaled fence is not permission to free in-flight objects. Abort this run;
    // process teardown owns the abandoned device after disabling this stack callback.
    result_.status="failed";
    try {result_.errors.push_back({"UNRETIRED_RESOURCES","GPU completion was unavailable; the failed process must exit before another run."});} catch(...) {}
    if(messenger_ && api_.vkDestroyDebugUtilsMessengerEXT) api_.vkDestroyDebugUtilsMessengerEXT(instance_,messenger_,nullptr);
    return;
  }
  try {
    if(device_ && api_.vkDestroyBuffer) {
      retire_swapchain();free_buffer(vertices_);free_buffer(indices_);
      if(rendered_) api_.vkDestroySemaphore(device_,rendered_,nullptr);
      if(graphics_fence_) api_.vkDestroyFence(device_,graphics_fence_,nullptr);
      if(present_fence_) api_.vkDestroyFence(device_,present_fence_,nullptr);
      if(acquire_fence_) api_.vkDestroyFence(device_,acquire_fence_,nullptr);
      if(command_pool_) api_.vkDestroyCommandPool(device_,command_pool_,nullptr);
    }
    if(device_ && api_.vkDestroyDevice) api_.vkDestroyDevice(device_,nullptr);
    if(surface_ && api_.vkDestroySurfaceKHR) api_.vkDestroySurfaceKHR(instance_,surface_,nullptr);
    if(messenger_ && api_.vkDestroyDebugUtilsMessengerEXT) api_.vkDestroyDebugUtilsMessengerEXT(instance_,messenger_,nullptr);
    if(instance_ && api_.vkDestroyInstance) api_.vkDestroyInstance(instance_,nullptr);
    if(window_) SDL_DestroyWindow(window_);
    if(sdl_) SDL_Quit();
  } catch(...) { result_.status="failed"; }
}
}  // namespace
RunResult run_world_cube(const std::array<Packet,2>& revisions,bool interactive) {
  static_assert(std::endian::native==std::endian::little && sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
  RunResult result;
  {
    Context context(result);
    bool initialized=false;
    try {
      context.initialize(revisions);initialized=true;context.execute(revisions,interactive);result.status="passed";
    } catch(const Failure& error) {
      result.status=(error.unsupported||!initialized)?"unsupported":"failed";
      result.errors.push_back({error.code,error.what()});
    } catch(const std::invalid_argument&) {
      result.errors.push_back({"INVALID_PRESENTATION_DATA","Presentation data or pixel extent exceeds the bounded contract."});
    } catch(const std::exception&) {
      result.errors.push_back({"RUNTIME_ERROR","The bounded presentation run could not complete."});
    }
  }
  if(result.validation.errors || result.validation.warnings) {
    result.status="failed";result.errors.push_back({"VALIDATION_FAILED","Khronos validation reported a warning or error."});
  }
  return result;
}
RunResult run_live(const LiveConfig& config,const LiveCallbacks& callbacks) {
  RunResult result;
  {
    Context context(result);bool initialized=false;
    try {
      if(config.max_slots<1 || config.max_slots>1024 || !callbacks.latest || !callbacks.should_stop ||
         !callbacks.ready || !callbacks.presented) throw std::invalid_argument("Invalid live renderer configuration.");
      context.initialize_capacity(static_cast<std::size_t>(config.max_slots)*24,
                                  static_cast<std::size_t>(config.max_slots)*36);
      initialized=true;context.execute_live(config,callbacks);result.status="passed";
    } catch(const Failure& error) {
      result.status=(error.unsupported||!initialized)?"unsupported":"failed";
      result.errors.push_back({error.code,error.what()});
    } catch(const std::exception&) {
      result.errors.push_back({"LIVE_RENDER_FAILED","Live presentation could not complete within its bounded contract."});
    }
  }
  if(result.validation.errors || result.validation.warnings) {
    result.status="failed";result.errors.push_back({"VALIDATION_FAILED","Khronos validation reported a warning or error."});
  }
  return result;
}
}  // namespace ow::render
