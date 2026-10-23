// Vulkan backend for the GPU layer (same API as DeviceD3D12.cpp).
//
// Vulkan 1.3 with dynamic rendering and synchronization2, push descriptors
// (VK_KHR_push_descriptor) for the universal binding layout, null descriptors
// (VK_EXT_robustness2) for unbound slots. Every image lives in the GENERAL
// layout after its first use; consecutive GPU operations are separated by a
// full memory barrier (simple and correct; finer tracking can come later).
// Sparse textures use sparse residency images (residencyNonResidentStrict:
// unbound areas read as zero), like D3D12 tiled resources tier 2.
#include "gpu/Device.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <cstring>

namespace sx::gpu {

namespace {
constexpr size_t kUploadPerFrame = 64ull << 20;
constexpr u32 kBindSrv = 0, kBindUav = 16, kBindCb = 33, kBindSampler = 48;
constexpr VkDeviceSize kTileBytes = 65536;
constexpr int kTilesPerBlock = 512;  // 32 MB memory blocks

Device g_device;

#define VK_CHECK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) Log("Vulkan error %d at %s:%d: %s", (int)r_, __FILE__, __LINE__, #x); } while (0)

VkFormat ToVk(Format f) {
    switch (f) {
        case Format::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::R8: return VK_FORMAT_R8_UNORM;
        case Format::R16F: return VK_FORMAT_R16_SFLOAT;
        case Format::R32F: return VK_FORMAT_R32_SFLOAT;
        case Format::R32U: return VK_FORMAT_R32_UINT;
        case Format::D32F: return VK_FORMAT_D32_SFLOAT;
        case Format::RGBA16: return VK_FORMAT_R16G16B16A16_UNORM;
    }
    return VK_FORMAT_R8G8B8A8_UNORM;
}

VkImageAspectFlags Aspect(Format f) { return f == Format::D32F ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT; }
}  // namespace

Device& Dev() { return g_device; }
const char* BackendName() { return "Vulkan"; }

u32 BytesPerPixel(Format f) {
    switch (f) {
        case Format::RGBA8: return 4;
        case Format::RGBA16F: return 8;
        case Format::RGBA32F: return 16;
        case Format::R8: return 1;
        case Format::R16F: return 2;
        case Format::R32F: return 4;
        case Format::R32U: return 4;
        case Format::D32F: return 4;
        case Format::RGBA16: return 8;
    }
    return 4;
}

Texture::~Texture() { g_device.DestroyTexture(*this); }

Buffer::~Buffer() {
    if (!buffer) return;
    VkBuffer b = buffer;
    VkDeviceMemory m = memory;
    g_device.Defer([b, m] {
        vkDestroyBuffer(g_device.device, b, nullptr);
        vkFreeMemory(g_device.device, m, nullptr);
    });
}

GraphicsPipeline::~GraphicsPipeline() {
    if (!pipeline) return;
    VkPipeline p = pipeline;
    g_device.Defer([p] { vkDestroyPipeline(g_device.device, p, nullptr); });
}

ComputePipeline::~ComputePipeline() {
    if (!pipeline) return;
    VkPipeline p = pipeline;
    g_device.Defer([p] { vkDestroyPipeline(g_device.device, p, nullptr); });
}

// ------------------------------------------------------------------ init

u32 Device::FindMemory(u32 typeBits, VkMemoryPropertyFlags props) const {
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i)
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) return i;
    return ~0u;
}

bool Device::Init(HWND window) {
    hwnd = window;
    if (volkInitialize() != VK_SUCCESS) { Log("Vulkan loader (vulkan-1.dll) not found."); return false; }
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Shaderixel";
    app.apiVersion = VK_API_VERSION_1_3;
    std::vector<const char*> exts = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    std::vector<const char*> layers;
    if (GetEnvironmentVariableA("SX_VK_VALIDATION", nullptr, 0) > 0) {
        u32 n = 0;
        vkEnumerateInstanceLayerProperties(&n, nullptr);
        std::vector<VkLayerProperties> props(n);
        vkEnumerateInstanceLayerProperties(&n, props.data());
        for (auto& p : props)
            if (!strcmp(p.layerName, "VK_LAYER_KHRONOS_validation")) layers.push_back("VK_LAYER_KHRONOS_validation");
        Log(layers.empty() ? "Vulkan validation layer not installed" : "Vulkan validation layer enabled");
    }
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = (u32)exts.size();
    ici.ppEnabledExtensionNames = exts.data();
    ici.enabledLayerCount = (u32)layers.size();
    ici.ppEnabledLayerNames = layers.data();
    if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS) { Log("vkCreateInstance failed (Vulkan 1.3 driver?)"); return false; }
    volkLoadInstance(instance);

    VkWin32SurfaceCreateInfoKHR sci = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    sci.hinstance = GetModuleHandleW(nullptr);
    sci.hwnd = hwnd;
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &sci, nullptr, &surface));

    // Physical device: prefer a discrete GPU with a graphics+compute queue that can present.
    u32 count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> gpus(count);
    vkEnumeratePhysicalDevices(instance, &count, gpus.data());
    int bestScore = -1;
    for (VkPhysicalDevice pd : gpus) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(pd, &props);
        if (props.apiVersion < VK_API_VERSION_1_3) continue;
        u32 qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qf.data());
        for (u32 i = 0; i < qn; ++i) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present);
            if (!present || !(qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !(qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) continue;
            int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
            if (score > bestScore) { bestScore = score; physical = pd; queueFamily = i; adapterName = props.deviceName; uboAlign = props.limits.minUniformBufferOffsetAlignment; }
            break;
        }
    }
    if (!physical) { Log("No Vulkan 1.3 GPU with a graphics/compute/present queue."); return false; }
    Log("GPU (Vulkan): %s", adapterName.c_str());
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);

    // Device with the features the universal layout relies on.
    VkPhysicalDeviceRobustness2FeaturesEXT robust = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
    robust.nullDescriptor = VK_TRUE;
    VkPhysicalDeviceVulkan13Features f13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    f13.dynamicRendering = VK_TRUE;
    f13.synchronization2 = VK_TRUE;
    f13.pNext = &robust;
    VkPhysicalDeviceVulkan12Features f12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.scalarBlockLayout = VK_TRUE;
    f12.uniformBufferStandardLayout = VK_TRUE;
    f12.timelineSemaphore = VK_TRUE;
    f12.pNext = &f13;
    VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.features.shaderStorageImageReadWithoutFormat = VK_TRUE;
    f2.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    f2.features.shaderStorageImageExtendedFormats = VK_TRUE;
    f2.features.fragmentStoresAndAtomics = VK_TRUE;
    f2.pNext = &f12;
    {
        // Sparse residency images whose unbound areas read as zero, on our queue.
        VkPhysicalDeviceFeatures have;
        vkGetPhysicalDeviceFeatures(physical, &have);
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physical, &props);
        u32 qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &qn, qf.data());
        sparseSupported = have.sparseBinding && have.sparseResidencyImage2D && props.sparseProperties.residencyNonResidentStrict &&
                          props.sparseProperties.residencyStandard2DBlockShape && (qf[queueFamily].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT);
        f2.features.sparseBinding = f2.features.sparseResidencyImage2D = sparseSupported ? VK_TRUE : VK_FALSE;
        Log("Sparse textures: %s", sparseSupported ? "yes" : "no");
    }
    std::vector<const char*> devExts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
                                        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};
    {
        u32 n = 0;
        vkEnumerateDeviceExtensionProperties(physical, nullptr, &n, nullptr);
        std::vector<VkExtensionProperties> avail(n);
        vkEnumerateDeviceExtensionProperties(physical, nullptr, &n, avail.data());
        for (auto& e : avail)
            if (!strcmp(e.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) { devExts.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME); memoryBudget = true; }
    }
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &f2;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = (u32)devExts.size();
    dci.ppEnabledExtensionNames = devExts.data();
    if (vkCreateDevice(physical, &dci, nullptr, &device) != VK_SUCCESS) { Log("vkCreateDevice failed (missing push descriptors / robustness2?)"); return false; }
    volkLoadDevice(device);
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    // Static samplers: point clamp, linear clamp, linear wrap.
    for (int i = 0; i < 3; ++i) {
        VkSamplerCreateInfo s = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        s.magFilter = s.minFilter = i == 0 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        s.mipmapMode = i == 0 ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
        s.addressModeU = s.addressModeV = s.addressModeW = i == 2 ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        s.maxLod = VK_LOD_CLAMP_NONE;
        VK_CHECK(vkCreateSampler(device, &s, nullptr, &samplers[i]));
    }
    // Universal push-descriptor layout + 64 bytes of push constants.
    std::vector<VkDescriptorSetLayoutBinding> b;
    for (u32 i = 0; i < kMaxSrv; ++i) b.push_back({kBindSrv + i, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr});
    for (u32 i = 0; i < kMaxUav; ++i) b.push_back({kBindUav + i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr});
    b.push_back({kBindCb, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    for (u32 i = 0; i < 3; ++i) b.push_back({kBindSampler + i, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_ALL, &samplers[i]});
    VkDescriptorSetLayoutCreateInfo lci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    lci.bindingCount = (u32)b.size();
    lci.pBindings = b.data();
    VK_CHECK(vkCreateDescriptorSetLayout(device, &lci, nullptr, &setLayout));
    VkPushConstantRange pcr = {VK_SHADER_STAGE_ALL, 0, kRootConstants * 4};
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &setLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout));

    for (auto& f : frames) {
        VkCommandPoolCreateInfo pci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.queueFamilyIndex = queueFamily;
        VK_CHECK(vkCreateCommandPool(device, &pci, nullptr, &f.pool));
        VkCommandBufferAllocateInfo cai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cai.commandPool = f.pool;
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(device, &cai, &f.cmd));
        VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(device, &fci, nullptr, &f.fence));
        VkSemaphoreCreateInfo semi = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &semi, nullptr, &f.acquired));
        VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = kUploadPerFrame;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &f.upload));
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, f.upload, &req);
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = FindMemory(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &f.uploadMem));
        VK_CHECK(vkBindBufferMemory(device, f.upload, f.uploadMem, 0));
        VK_CHECK(vkMapMemory(device, f.uploadMem, 0, VK_WHOLE_SIZE, 0, (void**)&f.uploadPtr));
    }
    for (u32 i = 0; i < kFramesInFlight; ++i) {
        VkCommandPoolCreateInfo pci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.queueFamilyIndex = queueFamily;
        VK_CHECK(vkCreateCommandPool(device, &pci, nullptr, &prePool[i]));
        VkCommandBufferAllocateInfo cai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cai.commandPool = prePool[i];
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(device, &cai, &preCmd[i]));
    }
    {
        VkSemaphoreTypeCreateInfo tci = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        tci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo semi = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        semi.pNext = &tci;
        VK_CHECK(vkCreateSemaphore(device, &semi, nullptr, &timeline));
        VK_CHECK(vkCreateSemaphore(device, &semi, nullptr, &sparseSem));
    }
    if (sparseSupported) {
        VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = kTileBytes;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &zeroBuf));
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, zeroBuf, &req);
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = FindMemory(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &zeroMem));
        VK_CHECK(vkBindBufferMemory(device, zeroBuf, zeroMem, 0));
    }
    ctx.dev = this;
    RECT rc;
    GetClientRect(hwnd, &rc);
    return CreateSwapchain((u32)std::max<LONG>(rc.right - rc.left, 1), (u32)std::max<LONG>(rc.bottom - rc.top, 1));
}

bool Device::CreateSwapchain(u32 w, u32 h) {
    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps));
    u32 n = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &n, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(n);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &n, fmts.data());
    swapFormat = fmts.empty() ? VK_FORMAT_B8G8R8A8_UNORM : fmts[0].format;
    VkColorSpaceKHR space = fmts.empty() ? VK_COLOR_SPACE_SRGB_NONLINEAR_KHR : fmts[0].colorSpace;
    for (auto& f : fmts)
        if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            swapFormat = f.format;
            space = f.colorSpace;
            break;
        }
    swapExtent = caps.currentExtent.width != ~0u ? caps.currentExtent : VkExtent2D{w, h};
    if (swapExtent.width == 0 || swapExtent.height == 0) { swapchainOk = false; return true; }
    VkSwapchainCreateInfoKHR ci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface;
    ci.minImageCount = std::max(caps.minImageCount, 2u);
    if (caps.maxImageCount) ci.minImageCount = std::min(ci.minImageCount, caps.maxImageCount);
    ci.imageFormat = swapFormat;
    ci.imageColorSpace = space;
    ci.imageExtent = swapExtent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = swapchain;
    VkSwapchainKHR sc = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &ci, nullptr, &sc) != VK_SUCCESS) { Log("vkCreateSwapchainKHR failed"); return false; }
    DestroySwapchain();
    swapchain = sc;
    vkGetSwapchainImagesKHR(device, swapchain, &n, nullptr);
    swapImages.resize(n);
    vkGetSwapchainImagesKHR(device, swapchain, &n, swapImages.data());
    for (VkImage img : swapImages) {
        VkImageViewCreateInfo vci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = img;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = swapFormat;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView v;
        VK_CHECK(vkCreateImageView(device, &vci, nullptr, &v));
        swapViews.push_back(v);
        VkSemaphoreCreateInfo semi = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore s;
        VK_CHECK(vkCreateSemaphore(device, &semi, nullptr, &s));
        renderDone.push_back(s);
    }
    swapchainOk = true;
    return true;
}

void Device::DestroySwapchain() {
    for (auto v : swapViews) vkDestroyImageView(device, v, nullptr);
    for (auto s : renderDone) vkDestroySemaphore(device, s, nullptr);
    swapViews.clear();
    renderDone.clear();
    swapImages.clear();
    if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
}

void Device::Resize(u32 w, u32 h) {
    if (!device || w == 0 || h == 0) return;
    WaitIdle();
    CreateSwapchain(w, h);
}

void Device::WaitIdle() {
    if (!device) return;
    vkDeviceWaitIdle(device);
    for (auto& f : frames) completedFrame = std::max(completedFrame, f.submitted);
    ResolveReadbacks();
}

void Device::Shutdown() {
    if (!device) return;
    vkDeviceWaitIdle(device);
    ProcessDeferred(true);
    DestroySwapchain();
    for (auto& f : frames) {
        vkDestroyBuffer(device, f.upload, nullptr);
        vkFreeMemory(device, f.uploadMem, nullptr);
        vkDestroyFence(device, f.fence, nullptr);
        vkDestroySemaphore(device, f.acquired, nullptr);
        vkDestroyCommandPool(device, f.pool, nullptr);
    }
    for (u32 i = 0; i < kFramesInFlight; ++i) {
        vkDestroyCommandPool(device, prePool[i], nullptr);
    }
    vkDestroySemaphore(device, timeline, nullptr);
    vkDestroySemaphore(device, sparseSem, nullptr);
    vkDestroyBuffer(device, zeroBuf, nullptr);
    vkFreeMemory(device, zeroMem, nullptr);
    for (VkDeviceMemory m : tileBlocks) vkFreeMemory(device, m, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
    for (auto s : samplers) vkDestroySampler(device, s, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    device = VK_NULL_HANDLE;
}

// ------------------------------------------------------------------ ImGui

bool Device::InitImGui() {
    ImGui_ImplVulkan_InitInfo init = {};
    init.ApiVersion = VK_API_VERSION_1_3;
    init.Instance = instance;
    init.PhysicalDevice = physical;
    init.Device = device;
    init.QueueFamily = queueFamily;
    init.Queue = queue;
    init.DescriptorPoolSize = 2048;
    init.MinImageCount = 2;
    init.ImageCount = (u32)std::max<size_t>(swapImages.size(), 2);
    init.UseDynamicRendering = true;
    init.PipelineInfoMain.PipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    init.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapFormat;
    // Secondary viewports (panels / windows outside the main window): Win32 surfaces.
    ImGui::GetPlatformIO().Platform_CreateVkSurface = [](ImGuiViewport* vp, ImU64 inst, const void* alloc, ImU64* out) -> int {
        VkWin32SurfaceCreateInfoKHR sci = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        sci.hinstance = GetModuleHandleW(nullptr);
        sci.hwnd = (HWND)vp->PlatformHandleRaw;
        VkSurfaceKHR s = VK_NULL_HANDLE;
        VkResult r = vkCreateWin32SurfaceKHR((VkInstance)inst, &sci, (const VkAllocationCallbacks*)alloc, &s);
        *out = (ImU64)s;
        return (int)r;
    };
    return ImGui_ImplVulkan_Init(&init);
}

void Device::ImGuiNewFrame() { ImGui_ImplVulkan_NewFrame(); }
void Device::ShutdownImGui() {
    vkDeviceWaitIdle(device);
    ProcessDeferred(true);
    ImGui_ImplVulkan_Shutdown();
}

u64 Device::UiTextureId(Texture* t) {
    if (!t || !t->view) return 0;
    if (!t->uiSet) t->uiSet = ImGui_ImplVulkan_AddTexture(samplers[1], t->view, VK_IMAGE_LAYOUT_GENERAL);
    if (std::find(uiTexturesThisFrame.begin(), uiTexturesThisFrame.end(), t) == uiTexturesThisFrame.end()) uiTexturesThisFrame.push_back(t);
    return (u64)t->uiSet;
}

// ------------------------------------------------------------------ frame

CommandContext& Device::BeginFrame() {
    Frame& f = frames[frameIndex];
    VK_CHECK(vkWaitForFences(device, 1, &f.fence, VK_TRUE, UINT64_MAX));
    completedFrame = std::max(completedFrame, f.submitted);
    ResolveReadbacks();
    ProcessDeferred(false);
    VK_CHECK(vkResetCommandPool(device, f.pool, 0));
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(f.cmd, &bi));
    f.uploadOffset = 0;
    uiTexturesThisFrame.clear();
    ctx.cmd = f.cmd;
    acquiredImage = false;
    if (swapchainOk) {
        VkResult r = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, f.acquired, VK_NULL_HANDLE, &swapIndex);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            vkDeviceWaitIdle(device);
            CreateSwapchain((u32)std::max<LONG>(rc.right, 1), (u32)std::max<LONG>(rc.bottom, 1));
            if (swapchainOk) r = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, f.acquired, VK_NULL_HANDLE, &swapIndex);
        }
        acquiredImage = r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR;
    }
    return ctx;
}

static void ImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags2 srcStage,
                         VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
                         VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
    VkImageMemoryBarrier2 b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = srcStage;
    b.srcAccessMask = srcAccess;
    b.dstStageMask = dstStage;
    b.dstAccessMask = dstAccess;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = {aspect, 0, 1, 0, 1};
    VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &di);
}

void Device::EndFrame() {
    Frame& f = frames[frameIndex];
    VkCommandBuffer cmd = f.cmd;
    for (Texture* t : uiTexturesThisFrame) ctx.Prepare(t);
    ctx.Barrier();
    ReadbackPtr capture;
    if (acquiredImage) {
        VkImage img = swapImages[swapIndex];
        ImageBarrier(cmd, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        VkRenderingAttachmentInfo ca = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        ca.imageView = swapViews[swapIndex];
        ca.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ca.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ca.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ca.clearValue.color = {{0.07f, 0.07f, 0.08f, 1.0f}};
        VkRenderingInfo ri = {VK_STRUCTURE_TYPE_RENDERING_INFO};
        ri.renderArea = {{0, 0}, swapExtent};
        ri.layerCount = 1;
        ri.colorAttachmentCount = 1;
        ri.pColorAttachments = &ca;
        vkCmdBeginRendering(cmd, &ri);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRendering(cmd);
        VkImageLayout last = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (captureRequest) {
            capture = captureRequest;
            captureRequest.reset();
            ImageBarrier(cmd, img, last, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
            last = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            capture->width = swapExtent.width;
            capture->height = swapExtent.height;
            capture->format = Format::RGBA8;
            capture->rowPitch = capture->width * 4;
            capture->swapRB = swapFormat == VK_FORMAT_B8G8R8A8_UNORM;
            VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bci.size = (VkDeviceSize)capture->rowPitch * capture->height;
            bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &capture->buffer));
            VkMemoryRequirements req;
            vkGetBufferMemoryRequirements(device, capture->buffer, &req);
            VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            mai.allocationSize = req.size;
            mai.memoryTypeIndex = FindMemory(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &capture->memory));
            VK_CHECK(vkBindBufferMemory(device, capture->buffer, capture->memory, 0));
            VkBufferImageCopy region = {};
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = {swapExtent.width, swapExtent.height, 1};
            vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, capture->buffer, 1, &region);
            pendingReadbacks.push_back(capture);
        }
        ImageBarrier(cmd, img, last, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);
    } else {
        // Nothing to present (minimized): keep ImGui's frame consistent.
        ImGui::GetDrawData();
    }
    VK_CHECK(vkEndCommandBuffer(cmd));

    const u64 id = frameNumber + 1;
    VkSemaphoreSubmitInfo waits[2] = {}, signals[2] = {};
    u32 numWaits = 0, numSignals = 0;
    if (acquiredImage) {
        waits[numWaits] = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        waits[numWaits].semaphore = f.acquired;
        waits[numWaits++].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        signals[numSignals] = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        signals[numSignals].semaphore = renderDone[swapIndex];
        signals[numSignals++].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }
    signals[numSignals] = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signals[numSignals].semaphore = timeline;
    signals[numSignals].value = id;
    signals[numSignals++].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    if (sparseValue > sparseWaited) {
        waits[numWaits] = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        waits[numWaits].semaphore = sparseSem;
        waits[numWaits].value = sparseValue;
        waits[numWaits++].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        sparseWaited = sparseValue;
    }
    VkCommandBufferSubmitInfo cbis[2] = {};
    u32 numCb = 0;
    if (preOpen) {
        VkCommandBuffer pre = preCmd[frameIndex];
        VkMemoryBarrier2 mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        mb.srcStageMask = mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        mb.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        di.memoryBarrierCount = 1;
        di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(pre, &di);
        VK_CHECK(vkEndCommandBuffer(pre));
        cbis[numCb] = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        cbis[numCb++].commandBuffer = pre;
        preOpen = false;
    }
    cbis[numCb] = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cbis[numCb++].commandBuffer = cmd;
    VkSubmitInfo2 si = {VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    si.commandBufferInfoCount = numCb;
    si.pCommandBufferInfos = cbis;
    si.waitSemaphoreInfoCount = numWaits;
    si.pWaitSemaphoreInfos = waits;
    si.signalSemaphoreInfoCount = numSignals;
    si.pSignalSemaphoreInfos = signals;
    VK_CHECK(vkResetFences(device, 1, &f.fence));
    VK_CHECK(vkQueueSubmit2(queue, 1, &si, f.fence));
    f.submitted = id;
    for (auto& r : pendingReadbacks) if (r->fence == 0) r->fence = id;
    if (acquiredImage) {
        VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &renderDone[swapIndex];
        pi.swapchainCount = 1;
        pi.pSwapchains = &swapchain;
        pi.pImageIndices = &swapIndex;
        VkResult r = vkQueuePresentKHR(queue, &pi);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            vkDeviceWaitIdle(device);
            CreateSwapchain((u32)std::max<LONG>(rc.right, 1), (u32)std::max<LONG>(rc.bottom, 1));
        }
    }
    frameIndex = (frameIndex + 1) % kFramesInFlight;
    ++frameNumber;
}

void Device::ResolveReadbacks() {
    for (size_t i = 0; i < pendingReadbacks.size();) {
        auto& r = pendingReadbacks[i];
        if (r->fence != 0 && r->fence <= completedFrame) {
            u8* p = nullptr;
            VK_CHECK(vkMapMemory(device, r->memory, 0, VK_WHOLE_SIZE, 0, (void**)&p));
            VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
            range.memory = r->memory;
            range.size = VK_WHOLE_SIZE;
            vkInvalidateMappedMemoryRanges(device, 1, &range);
            u32 rowBytes = r->width * BytesPerPixel(r->format);
            r->data.resize((size_t)rowBytes * r->height);
            for (u32 y = 0; y < r->height; ++y) memcpy(r->data.data() + (size_t)y * rowBytes, p + (size_t)y * r->rowPitch, rowBytes);
            vkUnmapMemory(device, r->memory);
            if (r->swapRB)
                for (size_t k = 0; k + 3 < r->data.size(); k += 4) std::swap(r->data[k], r->data[k + 2]);
            vkDestroyBuffer(device, r->buffer, nullptr);
            vkFreeMemory(device, r->memory, nullptr);
            r->buffer = VK_NULL_HANDLE;
            r->memory = VK_NULL_HANDLE;
            r->ready = true;
            pendingReadbacks.erase(pendingReadbacks.begin() + i);
        } else {
            ++i;
        }
    }
}

ReadbackPtr Device::CaptureNextFrame() {
    captureRequest = std::make_shared<Readback>();
    return captureRequest;
}

u64 Device::VideoMemoryUsage() const {
    if (!memoryBudget) return 0;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 p2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    p2.pNext = &budget;
    vkGetPhysicalDeviceMemoryProperties2(physical, &p2);
    u64 used = 0;
    for (u32 i = 0; i < p2.memoryProperties.memoryHeapCount; ++i)
        if (p2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) used += budget.heapUsage[i];
    return used;
}

u64 Device::VideoMemoryBudget() const {
    if (!memoryBudget) return 0;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 p2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    p2.pNext = &budget;
    vkGetPhysicalDeviceMemoryProperties2(physical, &p2);
    u64 total = 0;
    for (u32 i = 0; i < p2.memoryProperties.memoryHeapCount; ++i)
        if (p2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) total += budget.heapBudget[i];
    return total;
}

u64 Device::VideoMemoryTotal() const {
    // The largest device-local heap: the card's memory (the small BAR heap is a window into it).
    u64 best = 0;
    for (u32 i = 0; i < memProps.memoryHeapCount; ++i)
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) best = std::max<u64>(best, memProps.memoryHeaps[i].size);
    return best;
}

u64 Device::AdapterLuid() const {
    VkPhysicalDeviceIDProperties id = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
    VkPhysicalDeviceProperties2 p2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    p2.pNext = &id;
    vkGetPhysicalDeviceProperties2(physical, &p2);
    if (!id.deviceLUIDValid) return 0;
    u64 luid = 0;
    memcpy(&luid, id.deviceLUID, sizeof(luid));  // LowPart, then HighPart
    return luid;
}

// ------------------------------------------------------------------ sparse textures

int Device::AllocTile() {
    if (freeTiles.empty()) {
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = kTileBytes * kTilesPerBlock;
        mai.memoryTypeIndex = tileMemType;
        VkDeviceMemory m;
        if (vkAllocateMemory(device, &mai, nullptr, &m) != VK_SUCCESS) {
            Log("Sparse memory allocation failed (out of video memory?)");
            return -1;
        }
        int base = (int)tileBlocks.size() * kTilesPerBlock;
        tileBlocks.push_back(m);
        for (int i = kTilesPerBlock - 1; i >= 0; --i) freeTiles.push_back(base + i);
    }
    int s = freeTiles.back();
    freeTiles.pop_back();
    ++tilesInUse;
    return s;
}

VkCommandBuffer Device::PreCmd() {
    if (!preOpen) {
        // The slot may still be in flight when called between frames.
        VK_CHECK(vkWaitForFences(device, 1, &frames[frameIndex].fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetCommandPool(device, prePool[frameIndex], 0));
        VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(preCmd[frameIndex], &bi));
        preOpen = true;
        if (!zeroReady) {
            vkCmdFillBuffer(preCmd[frameIndex], zeroBuf, 0, VK_WHOLE_SIZE, 0);
            VkMemoryBarrier2 mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
            mb.srcStageMask = mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            mb.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            mb.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            di.memoryBarrierCount = 1;
            di.pMemoryBarriers = &mb;
            vkCmdPipelineBarrier2(preCmd[frameIndex], &di);
            zeroReady = true;
        }
    }
    return preCmd[frameIndex];
}

void Device::MapRect(Texture* t, const Rect& r0) {
    if (!t || !t->sparse || !t->image) return;
    Rect r = r0.Clipped((int)t->width, (int)t->height);
    if (r.Empty()) return;
    u32 tx0 = r.x0 / t->tileW, tx1 = (r.x1 - 1) / t->tileW, ty0 = r.y0 / t->tileH, ty1 = (r.y1 - 1) / t->tileH;
    std::vector<VkBufferImageCopy> clears;
    std::vector<VkSparseImageMemoryBind> binds;
    for (u32 ty = ty0; ty <= ty1; ++ty)
        for (u32 tx = tx0; tx <= tx1; ++tx) {
            int& slot = t->tiles[(size_t)ty * t->tilesX + tx];
            if (slot >= 0) continue;
            int s = AllocTile();
            if (s < 0) break;
            slot = s;
            ++t->mappedTiles;
            u32 x = tx * t->tileW, y = ty * t->tileH;
            VkExtent3D ext = {std::min(t->tileW, t->width - x), std::min(t->tileH, t->height - y), 1};
            VkSparseImageMemoryBind b = {};
            b.subresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
            b.offset = {(int)x, (int)y, 0};
            b.extent = ext;
            b.memory = tileBlocks[s / kTilesPerBlock];
            b.memoryOffset = (VkDeviceSize)(s % kTilesPerBlock) * kTileBytes;
            binds.push_back(b);
            VkBufferImageCopy c = {};
            c.bufferRowLength = t->tileW;
            c.bufferImageHeight = t->tileH;
            c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            c.imageOffset = {(int)x, (int)y, 0};
            c.imageExtent = ext;
            clears.push_back(c);
        }
    if (binds.empty()) return;
    // Wait for the previous submission (it may still read these regions as zero).
    const u64 waitValue = frameNumber, signalValue = ++sparseValue;
    VkTimelineSemaphoreSubmitInfo tsi = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    tsi.waitSemaphoreValueCount = 1;
    tsi.pWaitSemaphoreValues = &waitValue;
    tsi.signalSemaphoreValueCount = 1;
    tsi.pSignalSemaphoreValues = &signalValue;
    VkSparseImageMemoryBindInfo info = {t->image, (u32)binds.size(), binds.data()};
    VkBindSparseInfo bsi = {VK_STRUCTURE_TYPE_BIND_SPARSE_INFO};
    bsi.pNext = &tsi;
    bsi.waitSemaphoreCount = 1;
    bsi.pWaitSemaphores = &timeline;
    bsi.imageBindCount = 1;
    bsi.pImageBinds = &info;
    bsi.signalSemaphoreCount = 1;
    bsi.pSignalSemaphores = &sparseSem;
    VK_CHECK(vkQueueBindSparse(queue, 1, &bsi, VK_NULL_HANDLE));
    // Zero the new tiles (recycled memory) before this frame's main work touches them.
    vkCmdCopyBufferToImage(PreCmd(), zeroBuf, t->image, VK_IMAGE_LAYOUT_GENERAL, (u32)clears.size(), clears.data());
}

// ------------------------------------------------------------------ resources

void Device::Defer(std::function<void()> destroy) { deferred.push_back({frameNumber + 1, std::move(destroy)}); }

void Device::ProcessDeferred(bool all) {
    while (!deferred.empty() && (all || deferred.front().first <= completedFrame)) {
        deferred.front().second();
        deferred.pop_front();
    }
}

void Device::DestroyTexture(Texture& t) {
    if (!t.image) return;
    VkImage img = t.image;
    VkImageView view = t.view;
    VkDeviceMemory mem = t.memory;
    VkDescriptorSet ui = t.uiSet;
    t.image = VK_NULL_HANDLE;
    t.view = VK_NULL_HANDLE;
    t.memory = VK_NULL_HANDLE;
    t.uiSet = VK_NULL_HANDLE;
    std::vector<int> tiles;
    for (int s : t.tiles) if (s >= 0) tiles.push_back(s);
    t.tiles.clear();
    t.mappedTiles = 0;
    std::erase(uiTexturesThisFrame, &t);
    Defer([img, view, mem, ui, tiles = std::move(tiles)] {
        if (ui) ImGui_ImplVulkan_RemoveTexture(ui);
        vkDestroyImageView(g_device.device, view, nullptr);
        vkDestroyImage(g_device.device, img, nullptr);
        vkFreeMemory(g_device.device, mem, nullptr);
        for (int s : tiles) g_device.freeTiles.push_back(s);
        g_device.tilesInUse -= (u32)tiles.size();
    });
}

TexturePtr Device::CreateTexture(u32 w, u32 h, Format fmt, const char* debugName, u32 usage) {
    auto t = std::make_shared<Texture>();
    t->width = w;
    t->height = h;
    t->format = fmt;
    t->usage = usage;
    bool depth = fmt == Format::D32F;
    VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = ToVk(fmt);
    ici.extent = {w, h, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (depth) ici.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    else ici.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (usage & UsageRenderTarget) ici.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (sparseSupported && !usage && !depth && (u64)w * h >= sparseThreshold) {
        VkImageCreateInfo sci = ici;
        sci.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
        VkImage img = VK_NULL_HANDLE;
        if (vkCreateImage(device, &sci, nullptr, &img) == VK_SUCCESS) {
            VkMemoryRequirements req;
            vkGetImageMemoryRequirements(device, img, &req);
            u32 n = 0;
            vkGetImageSparseMemoryRequirements(device, img, &n, nullptr);
            std::vector<VkSparseImageMemoryRequirements> sreq(n);
            vkGetImageSparseMemoryRequirements(device, img, &n, sreq.data());
            u32 type = FindMemory(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (tileMemType == ~0u) tileMemType = type;
            // One 64 KB block per tile, no mip tail at level 0, shared memory type.
            bool ok = n == 1 && req.alignment == kTileBytes && sreq[0].imageMipTailFirstLod > 0 && type == tileMemType &&
                      (sreq[0].formatProperties.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT);
            if (ok) {
                VkExtent3D g = sreq[0].formatProperties.imageGranularity;
                ok = (VkDeviceSize)g.width * g.height * BytesPerPixel(fmt) == kTileBytes;
                if (ok) {
                    t->image = img;
                    t->sparse = true;
                    t->tileW = g.width;
                    t->tileH = g.height;
                    t->tilesX = (w + g.width - 1) / g.width;
                    t->tilesY = (h + g.height - 1) / g.height;
                    t->tiles.assign((size_t)t->tilesX * t->tilesY, -1);
                }
            }
            if (!ok) vkDestroyImage(device, img, nullptr);  // e.g. smaller than one tile: committed texture
        }
    }
    if (t->sparse) {
        // Unbound areas read as zero; move to GENERAL ahead of the frame's work.
        t->initialized = true;
        ImageBarrier(PreCmd(), t->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0,
                     VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
        VkImageViewCreateInfo vci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = t->image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = ici.format;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_CHECK(vkCreateImageView(device, &vci, nullptr, &t->view));
        return t;
    }
    if (vkCreateImage(device, &ici, nullptr, &t->image) != VK_SUCCESS) {
        Log("CreateTexture %ux%u failed", w, h);
        return nullptr;
    }
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, t->image, &req);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemory(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &mai, nullptr, &t->memory) != VK_SUCCESS) {
        Log("CreateTexture %ux%u: out of video memory", w, h);
        vkDestroyImage(device, t->image, nullptr);
        t->image = VK_NULL_HANDLE;
        return nullptr;
    }
    VK_CHECK(vkBindImageMemory(device, t->image, t->memory, 0));
    VkImageViewCreateInfo vci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = t->image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = ici.format;
    vci.subresourceRange = {Aspect(fmt), 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(device, &vci, nullptr, &t->view));
    (void)debugName;
    return t;
}

VkBuffer Device::AllocUpload(size_t size, size_t align, void** cpu, VkDeviceSize* offset) {
    Frame& f = frames[frameIndex];
    size_t off = (f.uploadOffset + align - 1) & ~(align - 1);
    if (off + size <= kUploadPerFrame) {
        f.uploadOffset = off + size;
        *cpu = f.uploadPtr + off;
        *offset = off;
        return f.upload;
    }
    // Large one-off upload: dedicated host buffer released once the frame completes.
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VkBuffer buf;
    VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &buf));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buf, &req);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemory(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory mem;
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &mem));
    VK_CHECK(vkBindBufferMemory(device, buf, mem, 0));
    VK_CHECK(vkMapMemory(device, mem, 0, VK_WHOLE_SIZE, 0, cpu));
    *offset = 0;
    Defer([buf, mem] {
        vkDestroyBuffer(g_device.device, buf, nullptr);
        vkFreeMemory(g_device.device, mem, nullptr);
    });
    return buf;
}

BufferPtr Device::CreateBuffer(CommandContext& c, const void* data, size_t size, const char* debugName) {
    auto b = std::make_shared<Buffer>();
    b->size = size;
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (vkCreateBuffer(device, &bci, nullptr, &b->buffer) != VK_SUCCESS) return nullptr;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, b->buffer, &req);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemory(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &b->memory));
    VK_CHECK(vkBindBufferMemory(device, b->buffer, b->memory, 0));
    void* mem = nullptr;
    VkDeviceSize off = 0;
    VkBuffer src = AllocUpload(size, 16, &mem, &off);
    memcpy(mem, data, size);
    c.Barrier();
    VkBufferCopy region = {off, 0, size};
    vkCmdCopyBuffer(c.cmd, src, b->buffer, 1, &region);
    (void)debugName;
    return b;
}

static VkShaderModule MakeModule(VkDevice device, const void* code, size_t size) {
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = size;
    ci.pCode = (const u32*)code;
    VkShaderModule m = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &m) != VK_SUCCESS) Log("vkCreateShaderModule failed");
    return m;
}

PipelinePtr Device::CreatePipeline(const void* code, size_t size, u32 groupX, u32 groupY) {
    VkShaderModule mod = MakeModule(device, code, size);
    if (!mod) return nullptr;
    VkComputePipelineCreateInfo ci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ci.stage.module = mod;
    ci.stage.pName = "main";
    ci.layout = pipelineLayout;
    auto p = std::make_shared<ComputePipeline>();
    VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &p->pipeline);
    vkDestroyShaderModule(device, mod, nullptr);
    if (r != VK_SUCCESS) { Log("vkCreateComputePipelines failed (%d)", (int)r); return nullptr; }
    p->groupX = groupX;
    p->groupY = groupY;
    return p;
}

GraphicsPipelinePtr Device::CreateGraphicsPipeline(const void* vs, size_t vsSize, const void* ps, size_t psSize, const GraphicsDesc& g) {
    VkShaderModule vm = MakeModule(device, vs, vsSize), fm = MakeModule(device, ps, psSize);
    if (!vm || !fm) return nullptr;
    VkPipelineShaderStageCreateInfo stages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vm;
    stages[0].pName = "vsMain";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fm;
    stages[1].pName = "psMain";
    VkVertexInputBindingDescription vb = {0, 32, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription va[3] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12}, {2, 0, VK_FORMAT_R32G32_SFLOAT, 24}};
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if (g.meshLayout) {
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &vb;
        vi.vertexAttributeDescriptionCount = 3;
        vi.pVertexAttributeDescriptions = va;
    }
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = g.cullBack ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // with the flipped viewport this matches D3D's FrontCounterClockwise
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = g.useDepth;
    ds.depthWriteEnable = g.useDepth && g.depthWrite;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    std::vector<VkPipelineColorBlendAttachmentState> blends(g.colorFormats.size());
    for (auto& b : blends) b.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = (u32)blends.size();
    cb.pAttachments = blends.data();
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;
    std::vector<VkFormat> colors;
    for (Format f : g.colorFormats) colors.push_back(ToVk(f));
    VkPipelineRenderingCreateInfo rci = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rci.colorAttachmentCount = (u32)colors.size();
    rci.pColorAttachmentFormats = colors.data();
    rci.depthAttachmentFormat = g.useDepth ? ToVk(g.depthFormat) : VK_FORMAT_UNDEFINED;
    VkGraphicsPipelineCreateInfo ci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pNext = &rci;
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vi;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vp;
    ci.pRasterizationState = &rs;
    ci.pMultisampleState = &ms;
    ci.pDepthStencilState = &ds;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &dy;
    ci.layout = pipelineLayout;
    auto p = std::make_shared<GraphicsPipeline>();
    VkResult r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &p->pipeline);
    vkDestroyShaderModule(device, vm, nullptr);
    vkDestroyShaderModule(device, fm, nullptr);
    if (r != VK_SUCCESS) { Log("vkCreateGraphicsPipelines failed (%d)", (int)r); return nullptr; }
    return p;
}

// ------------------------------------------------------------------ command context

void CommandContext::Prepare(Texture* t) {
    if (!t || !t->image || t->initialized) return;
    t->initialized = true;
    // New images start zeroed (like D3D12 committed resources) in the GENERAL layout.
    ImageBarrier(cmd, t->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0,
                 VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, Aspect(t->format));
    VkImageSubresourceRange range = {Aspect(t->format), 0, 1, 0, 1};
    if (t->format == Format::D32F) {
        VkClearDepthStencilValue v = {1.0f, 0};
        vkCmdClearDepthStencilImage(cmd, t->image, VK_IMAGE_LAYOUT_GENERAL, &v, 1, &range);
    } else {
        VkClearColorValue v = {};
        vkCmdClearColorImage(cmd, t->image, VK_IMAGE_LAYOUT_GENERAL, &v, 1, &range);
    }
    Barrier();  // the clear lands before the first use (an upload, a copy or a shader write right after)
}

void CommandContext::Barrier() {
    VkMemoryBarrier2 b = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.memoryBarrierCount = 1;
    di.pMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &di);
}

namespace {
struct Bindings {
    VkDescriptorImageInfo images[kMaxSrv + kMaxUav] = {};
    VkDescriptorBufferInfo cb = {};
    VkWriteDescriptorSet writes[kMaxSrv + kMaxUav + 1] = {};
    u32 count = 0;
};
}  // namespace

// views: kMaxSrv sampled then kMaxUav storage (VK_NULL_HANDLE = null descriptor).
static void FillBindings(Bindings& b, const VkImageView* views, VkBuffer cbBuf, VkDeviceSize cbOff, VkDeviceSize cbSize) {
    for (u32 i = 0; i < kMaxSrv + kMaxUav; ++i) {
        bool isUav = i >= kMaxSrv;
        b.images[i] = {VK_NULL_HANDLE, views[i], VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet& w = b.writes[b.count++];
        w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstBinding = isUav ? kBindUav + (i - kMaxSrv) : kBindSrv + i;
        w.descriptorCount = 1;
        w.descriptorType = isUav ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w.pImageInfo = &b.images[i];
    }
    b.cb = {cbBuf, cbOff, cbSize};
    VkWriteDescriptorSet& w = b.writes[b.count++];
    w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstBinding = kBindCb;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo = &b.cb;
}

void CommandContext::Dispatch(const DispatchDesc& d) {
    if (!d.pipeline || !d.pipeline->pipeline) return;
    for (u32 i = 0; i < kMaxUav; ++i) {
        if (!d.uav[i] || !d.uav[i]->sparse || (d.uavNoMap & (1u << i))) continue;
        const Rect& r = d.uavRect[i];
        bool unset = r.x0 == 0 && r.y0 == 0 && r.x1 == 0 && r.y1 == 0;
        dev->MapRect(d.uav[i], unset ? Rect{0, 0, (int)d.uav[i]->width, (int)d.uav[i]->height} : r);
    }
    Barrier();
    for (auto* t : d.srv) Prepare(t);
    for (auto* t : d.uav) Prepare(t);
    static const u8 zeros[256] = {};
    const void* cbSrc = d.cbData ? d.cbData : zeros;
    size_t cbSize = d.cbData ? std::max<size_t>(d.cbSize, 16) : sizeof(zeros);
    void* mem = nullptr;
    VkDeviceSize off = 0;
    VkBuffer buf = dev->AllocUpload((cbSize + 255) & ~size_t(255), (size_t)std::max<VkDeviceSize>(dev->uboAlign, 256), &mem, &off);
    memcpy(mem, cbSrc, d.cbData ? d.cbSize : sizeof(zeros));
    VkImageView views[kMaxSrv + kMaxUav] = {};
    for (u32 i = 0; i < kMaxSrv; ++i) views[i] = d.srv[i] ? d.srv[i]->view : VK_NULL_HANDLE;
    for (u32 i = 0; i < kMaxUav; ++i) views[kMaxSrv + i] = d.uav[i] ? d.uav[i]->view : VK_NULL_HANDLE;
    Bindings b;
    FillBindings(b, views, buf, off, (cbSize + 15) & ~size_t(15));
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, d.pipeline->pipeline);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dev->pipelineLayout, 0, b.count, b.writes);
    if (d.numConstants) vkCmdPushConstants(cmd, dev->pipelineLayout, VK_SHADER_STAGE_ALL, 0, d.numConstants * 4, d.constants);
    u32 gx = (d.threadsX + d.pipeline->groupX - 1) / d.pipeline->groupX;
    u32 gy = (d.threadsY + d.pipeline->groupY - 1) / d.pipeline->groupY;
    if (gx && gy) vkCmdDispatch(cmd, gx, gy, 1);
}

void CommandContext::Draw(const DrawDesc& d) {
    if (!d.pipeline || !d.pipeline->pipeline) return;
    Barrier();
    for (auto* t : d.srv) Prepare(t);
    Texture* first = nullptr;
    VkRenderingAttachmentInfo colors[4] = {};
    u32 numRt = 0;
    for (int i = 0; i < 4 && d.rt[i]; ++i) {
        Prepare(d.rt[i]);
        VkRenderingAttachmentInfo& a = colors[numRt++];
        a = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        a.imageView = d.rt[i]->view;
        a.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        a.loadOp = d.clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.clearValue.color = {{d.clearValue.x, d.clearValue.y, d.clearValue.z, d.clearValue.w}};
        if (!first) first = d.rt[i];
    }
    VkRenderingAttachmentInfo depth = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    if (d.depth) {
        Prepare(d.depth);
        depth.imageView = d.depth->view;
        depth.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        depth.loadOp = d.clearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.clearValue.depthStencil = {1.0f, 0};
        if (!first) first = d.depth;
    }
    if (!first) return;
    static const u8 zeros[256] = {};
    size_t cbSize = d.cbData ? std::max<size_t>(d.cbSize, 16) : sizeof(zeros);
    void* mem = nullptr;
    VkDeviceSize off = 0;
    VkBuffer buf = dev->AllocUpload((cbSize + 255) & ~size_t(255), (size_t)std::max<VkDeviceSize>(dev->uboAlign, 256), &mem, &off);
    memcpy(mem, d.cbData ? d.cbData : zeros, d.cbData ? d.cbSize : sizeof(zeros));
    VkImageView views[kMaxSrv + kMaxUav] = {};
    for (u32 i = 0; i < kMaxSrv; ++i) views[i] = d.srv[i] ? d.srv[i]->view : VK_NULL_HANDLE;
    Bindings b;
    FillBindings(b, views, buf, off, (cbSize + 15) & ~size_t(15));

    VkRenderingInfo ri = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea = {{0, 0}, {first->width, first->height}};
    ri.layerCount = 1;
    ri.colorAttachmentCount = numRt;
    ri.pColorAttachments = colors;
    ri.pDepthAttachment = d.depth ? &depth : nullptr;
    vkCmdBeginRendering(cmd, &ri);
    // Flipped viewport: same clip space orientation as D3D.
    VkViewport vp = {0, (float)first->height, (float)first->width, -(float)first->height, 0, 1};
    VkRect2D sc = {{0, 0}, {first->width, first->height}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, d.pipeline->pipeline);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dev->pipelineLayout, 0, b.count, b.writes);
    if (d.numConstants) vkCmdPushConstants(cmd, dev->pipelineLayout, VK_SHADER_STAGE_ALL, 0, d.numConstants * 4, d.constants);
    if (d.vertices) {
        VkDeviceSize vo = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &d.vertices->buffer, &vo);
    }
    if (d.indices) {
        vkCmdBindIndexBuffer(cmd, d.indices->buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, d.count, 1, 0, 0, 0);
    } else {
        vkCmdDraw(cmd, d.count, 1, 0, 0);
    }
    vkCmdEndRendering(cmd);
}

void CommandContext::CopyTexture(Texture* dst, Texture* src) {
    CopyRegion(dst, 0, 0, src, {0, 0, (int)std::min(src->width, dst->width), (int)std::min(src->height, dst->height)});
}

static bool AnyMapped(const Texture* t, const Rect& r) {
    if (!t->Sparse()) return true;
    if (t->MappedBytes() == 0) return false;
    for (int ty = r.y0 / (int)t->TileH(); ty <= (r.y1 - 1) / (int)t->TileH(); ++ty)
        for (int tx = r.x0 / (int)t->TileW(); tx <= (r.x1 - 1) / (int)t->TileW(); ++tx)
            if (t->TileMapped(tx, ty)) return true;
    return false;
}

void CommandContext::CopyRegion(Texture* dst, int dx, int dy, Texture* src, const Rect& r) {
    if (r.Empty() || !dst || !src) return;
    Barrier();
    Prepare(src);
    Prepare(dst);
    std::vector<VkImageCopy> copies;
    auto copy = [&](const Rect& q) {
        VkImageCopy c = {};
        c.srcSubresource = {Aspect(src->format), 0, 0, 1};
        c.dstSubresource = {Aspect(dst->format), 0, 0, 1};
        c.srcOffset = {q.x0, q.y0, 0};
        c.dstOffset = {dx + q.x0 - r.x0, dy + q.y0 - r.y0, 0};
        c.extent = {(u32)q.Width(), (u32)q.Height(), 1};
        copies.push_back(c);
    };
    auto target = [&](const Rect& q) { return Rect{dx + q.x0 - r.x0, dy + q.y0 - r.y0, dx + q.x1 - r.x0, dy + q.y1 - r.y0}; };
    if (!src->sparse) {
        dev->MapRect(dst, target(r));
        copy(r);
    } else {
        // Sparse source: copy only where the source has data (the destination gets
        // backed) or the destination holds data that must become zero.
        for (int ty = r.y0 / (int)src->tileH; ty <= (r.y1 - 1) / (int)src->tileH; ++ty) {
            int y0 = std::max(r.y0, ty * (int)src->tileH), y1 = std::min(r.y1, (ty + 1) * (int)src->tileH);
            Rect run;
            bool runMap = false;
            auto flush = [&] {
                if (run.Empty()) return;
                if (runMap) dev->MapRect(dst, target(run));
                copy(run);
                run = {};
            };
            for (int tx = r.x0 / (int)src->tileW; tx <= (r.x1 - 1) / (int)src->tileW; ++tx) {
                Rect piece{std::max(r.x0, tx * (int)src->tileW), y0, std::min(r.x1, (tx + 1) * (int)src->tileW), y1};
                bool srcHas = src->TileMapped(tx, ty);
                bool need = srcHas || AnyMapped(dst, target(piece));
                if (!need) { flush(); continue; }
                if (!run.Empty() && srcHas != runMap) flush();
                if (run.Empty()) { run = piece; runMap = srcHas; }
                else run.x1 = piece.x1;
            }
            flush();
        }
    }
    if (!copies.empty())
        vkCmdCopyImage(cmd, src->image, VK_IMAGE_LAYOUT_GENERAL, dst->image, VK_IMAGE_LAYOUT_GENERAL, (u32)copies.size(), copies.data());
}

void CommandContext::Upload(Texture* dst, const void* pixels, u32 srcRowPitch) {
    const u32 rowBytes = dst->width * BytesPerPixel(dst->format);
    void* mem = nullptr;
    VkDeviceSize off = 0;
    VkBuffer buf = dev->AllocUpload((size_t)rowBytes * dst->height, 16, &mem, &off);
    for (u32 y = 0; y < dst->height; ++y) memcpy((u8*)mem + (size_t)y * rowBytes, (const u8*)pixels + (size_t)y * srcRowPitch, rowBytes);
    Barrier();
    Prepare(dst);
    if (!dst->sparse) {
        VkBufferImageCopy c = {};
        c.bufferOffset = off;
        c.imageSubresource = {Aspect(dst->format), 0, 0, 1};
        c.imageExtent = {dst->width, dst->height, 1};
        vkCmdCopyBufferToImage(cmd, buf, dst->image, VK_IMAGE_LAYOUT_GENERAL, 1, &c);
        return;
    }
    // Sparse: only back tiles that contain non-zero pixels (or already hold data).
    const u32 bpp = BytesPerPixel(dst->format);
    std::vector<VkBufferImageCopy> copies;
    for (u32 ty = 0; ty < dst->tilesY; ++ty)
        for (u32 tx = 0; tx < dst->tilesX; ++tx) {
            u32 x0 = tx * dst->tileW, y0 = ty * dst->tileH;
            u32 x1 = std::min(dst->width, x0 + dst->tileW), y1 = std::min(dst->height, y0 + dst->tileH);
            bool nonZero = false;
            for (u32 y = y0; y < y1 && !nonZero; ++y) {
                const u8* row = (const u8*)pixels + (size_t)y * srcRowPitch + (size_t)x0 * bpp;
                size_t n = (size_t)(x1 - x0) * bpp, i = 0;
                for (; i + 8 <= n; i += 8) {
                    u64 v;
                    memcpy(&v, row + i, 8);
                    if (v) { nonZero = true; break; }
                }
                for (; i < n && !nonZero; ++i) nonZero = row[i] != 0;
            }
            if (!nonZero && !dst->TileMapped(tx, ty)) continue;
            if (nonZero) dev->MapRect(dst, {(int)x0, (int)y0, (int)x1, (int)y1});
            VkBufferImageCopy c = {};
            c.bufferOffset = off + (VkDeviceSize)y0 * rowBytes + (VkDeviceSize)x0 * bpp;
            c.bufferRowLength = dst->width;
            c.bufferImageHeight = dst->height;
            c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            c.imageOffset = {(int)x0, (int)y0, 0};
            c.imageExtent = {x1 - x0, y1 - y0, 1};
            copies.push_back(c);
        }
    if (!copies.empty()) vkCmdCopyBufferToImage(cmd, buf, dst->image, VK_IMAGE_LAYOUT_GENERAL, (u32)copies.size(), copies.data());
}

ReadbackPtr CommandContext::ReadbackTexture(Texture* src, const Rect* region) {
    Rect r = region ? region->Clipped(src->width, src->height) : Rect{0, 0, (int)src->width, (int)src->height};
    auto rb = std::make_shared<Readback>();
    if (r.Empty()) { rb->ready = true; return rb; }
    rb->width = r.Width();
    rb->height = r.Height();
    rb->format = src->format;
    rb->rowPitch = rb->width * BytesPerPixel(src->format);
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = (VkDeviceSize)rb->rowPitch * rb->height;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto failed = [&] {
        // Too large for the system memory: an empty result (callers see no pixels) instead of a device error.
        Log("Readback of %ux%u failed (out of memory)", rb->width, rb->height);
        if (rb->buffer) vkDestroyBuffer(dev->device, rb->buffer, nullptr);
        rb->buffer = VK_NULL_HANDLE;
        rb->memory = VK_NULL_HANDLE;
        rb->width = rb->height = 0;
        rb->ready = true;
        return rb;
    };
    if (vkCreateBuffer(dev->device, &bci, nullptr, &rb->buffer) != VK_SUCCESS) return failed();
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(dev->device, rb->buffer, &req);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    u32 type = dev->FindMemory(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    if (type == ~0u) type = dev->FindMemory(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    mai.memoryTypeIndex = type;
    if (type == ~0u || vkAllocateMemory(dev->device, &mai, nullptr, &rb->memory) != VK_SUCCESS) return failed();
    VK_CHECK(vkBindBufferMemory(dev->device, rb->buffer, rb->memory, 0));
    Barrier();
    Prepare(src);
    VkBufferImageCopy c = {};
    c.imageSubresource = {Aspect(src->format), 0, 0, 1};
    c.imageOffset = {r.x0, r.y0, 0};
    c.imageExtent = {(u32)r.Width(), (u32)r.Height(), 1};
    vkCmdCopyImageToBuffer(cmd, src->image, VK_IMAGE_LAYOUT_GENERAL, rb->buffer, 1, &c);
    dev->pendingReadbacks.push_back(rb);
    return rb;
}

}  // namespace sx::gpu
