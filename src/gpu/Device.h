#pragma once
// Thin GPU layer for Shaderixel (D3D12 or Vulkan, chosen at build time with
// SX_VULKAN). Everything image-related runs as compute dispatches through one
// universal binding layout:
//   b0 : 16 root/push constants     (small per-dispatch data)
//   b1 : constant buffer from the per-frame upload ring (params, brush dabs, up to 64 KB)
//   t0..t7 : sampled textures, u0..u3 : storage textures
//   s0 point-clamp, s1 linear-clamp, s2 linear-wrap (static samplers)
// Vulkan maps the HLSL registers to bindings t:0+, u:16+, b:32+, s:48+ (set 0,
// push descriptors) and b0 to push constants; shaders are shared.

#include "core/Common.h"

#ifdef SX_VULKAN
#include <windows.h>
#include <volk.h>
#else
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#endif
#include <functional>
#include <memory>
#include <deque>

namespace sx::gpu {

#ifndef SX_VULKAN
using Microsoft::WRL::ComPtr;
#endif

constexpr u32 kFramesInFlight = 2;
constexpr u32 kMaxSrv = 8;
constexpr u32 kMaxUav = 4;
constexpr u32 kRootConstants = 16;

enum class Format : u8 { RGBA8, RGBA16F, RGBA32F, R8, R16F, R32F, R32U, D32F, RGBA16 };  // RGBA16: unorm (exports)
enum TextureUsage : u32 { UsageRenderTarget = 1, UsageDepth = 2 };
u32 BytesPerPixel(Format f);
const char* BackendName();

class Device;

class Texture {
public:
    ~Texture();
    u32 Width() const { return width; }
    u32 Height() const { return height; }
    Format GetFormat() const { return format; }
    // Sparse (reserved) textures: 64 KB tiles are backed by memory only once written.
    // Unbacked areas read as zero.
    bool Sparse() const { return sparse; }
    u64 MappedBytes() const { return (u64)mappedTiles * 65536; }
    u32 TileW() const { return tileW; }
    u32 TileH() const { return tileH; }
    bool TileMapped(u32 tx, u32 ty) const { return tiles[(size_t)ty * tilesX + tx] >= 0; }

private:
    friend class Device;
    friend class CommandContext;
    u32 width = 0, height = 0;
    Format format = Format::RGBA16F;
    bool sparse = false;
    u32 tileW = 0, tileH = 0, tilesX = 0, tilesY = 0, mappedTiles = 0;
    std::vector<int> tiles;  // heap slot per tile, -1 = unmapped
#ifdef SX_VULKAN
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDescriptorSet uiSet = VK_NULL_HANDLE;  // ImGui texture id
    bool initialized = false;                // layout moved to GENERAL
    u32 usage = 0;
#else
    ComPtr<ID3D12Resource> res;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES frameStartState = D3D12_RESOURCE_STATE_COMMON;  // state when this frame's list begins
    u64 stateFrame = ~0ull;
    int srvCpu = -1, uavCpu = -1;  // staging heap slots
    int uiSlot = -1;               // persistent shader-visible SRV (for ImGui)
    int rtv = -1, dsv = -1;        // render target / depth views (graphics)
#endif
};

class Buffer {
public:
    ~Buffer();
    u64 Size() const { return size; }
private:
    friend class Device;
    friend class CommandContext;
    u64 size = 0;
#ifdef SX_VULKAN
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
#else
    ComPtr<ID3D12Resource> res;
#endif
};
using BufferPtr = std::shared_ptr<Buffer>;

struct GraphicsPipeline {
#ifdef SX_VULKAN
    VkPipeline pipeline = VK_NULL_HANDLE;
    ~GraphicsPipeline();
#else
    ComPtr<ID3D12PipelineState> pso;
#endif
};
using GraphicsPipelinePtr = std::shared_ptr<GraphicsPipeline>;

// Graphics pipeline options. Vertex layout is fixed for meshes:
// POSITION float3, NORMAL float3, TEXCOORD float2 (32 bytes), or none.
struct GraphicsDesc {
    std::vector<Format> colorFormats;
    Format depthFormat = Format::D32F;
    bool useDepth = true;
    bool depthWrite = true;
    bool meshLayout = true;
    bool cullBack = false;
};

struct DrawDesc {
    const GraphicsPipeline* pipeline = nullptr;
    Texture* srv[kMaxSrv] = {};
    Texture* rt[4] = {};
    Texture* depth = nullptr;
    bool clearColor = false, clearDepth = false;
    float4 clearValue;
    u32 constants[kRootConstants] = {};
    u32 numConstants = 0;
    const void* cbData = nullptr;
    size_t cbSize = 0;
    Buffer* vertices = nullptr;
    u32 vertexStride = 32;
    Buffer* indices = nullptr;  // u32 indices
    u32 count = 0;              // index (or vertex) count
};
using TexturePtr = std::shared_ptr<Texture>;

struct ComputePipeline {
#ifdef SX_VULKAN
    VkPipeline pipeline = VK_NULL_HANDLE;
    ~ComputePipeline();
#else
    ComPtr<ID3D12PipelineState> pso;
#endif
    u32 groupX = 8, groupY = 8;
};
using PipelinePtr = std::shared_ptr<ComputePipeline>;

struct DispatchDesc {
    const ComputePipeline* pipeline = nullptr;
    Texture* srv[kMaxSrv] = {};
    Texture* uav[kMaxUav] = {};
    u32 constants[kRootConstants] = {};
    u32 numConstants = 0;
    const void* cbData = nullptr;   // copied to upload ring, bound at b1
    size_t cbSize = 0;
    u32 threadsX = 1, threadsY = 1; // total threads; groups computed from pipeline
    // Sparse UAVs: area that receives non-zero writes (memory gets backed there).
    // {0,0,0,0} = whole texture. Writes outside backed tiles are dropped, so a kernel
    // that only writes zeros outside `uavRect` stays exact.
    Rect uavRect[kMaxUav];
    u32 uavNoMap = 0;               // bit i: uav i only writes zeros, back nothing new
};

// Result of an async GPU->CPU copy; `ready` flips once the GPU finished.
struct Readback {
    bool ready = false;
    u32 width = 0, height = 0, rowPitch = 0;
    Format format = Format::RGBA8;
    std::vector<u8> data;  // tightly packed rows after resolve
    u64 fence = 0;
    bool swapRB = false;   // swapchain capture in BGRA order
#ifdef SX_VULKAN
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
#else
    ComPtr<ID3D12Resource> buffer;
#endif
};
using ReadbackPtr = std::shared_ptr<Readback>;

class CommandContext {
public:
    void Dispatch(const DispatchDesc& d);
    void CopyTexture(Texture* dst, Texture* src);
    // Copy src rect into dst at (dx,dy).
    void CopyRegion(Texture* dst, int dx, int dy, Texture* src, const Rect& srcRect);
    void Upload(Texture* dst, const void* pixels, u32 srcRowPitch);
    void Draw(const DrawDesc& d);
    ReadbackPtr ReadbackTexture(Texture* src, const Rect* region = nullptr);

private:
    friend class Device;
    Device* dev = nullptr;
#ifdef SX_VULKAN
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    void Prepare(Texture* t);  // first use: layout to GENERAL
    void Barrier();            // full memory dependency between consecutive GPU operations
#else
    void Transition(Texture* t, D3D12_RESOURCE_STATES s);
    void UavBarrier(Texture* t);
    ComPtr<ID3D12GraphicsCommandList> list;
#endif
};

class Device {
public:
    bool Init(HWND hwnd);
    void Shutdown();
    void Resize(u32 w, u32 h);

    // ImGui renderer backend (the platform backend is set up by the app).
    bool InitImGui();
    void ImGuiNewFrame();
    void ShutdownImGui();

    // Frame lifecycle. BeginFrame returns the context all work is recorded into;
    // EndFrame renders ImGui's draw data on top and presents.
    CommandContext& BeginFrame();
    void EndFrame();
    void WaitIdle();

    TexturePtr CreateTexture(u32 w, u32 h, Format f, const char* debugName = nullptr, u32 usage = 0);
    BufferPtr CreateBuffer(CommandContext& ctx, const void* data, size_t size, const char* debugName = nullptr);
    // Shader bytecode: DXIL (D3D12) or SPIR-V (Vulkan).
    GraphicsPipelinePtr CreateGraphicsPipeline(const void* vs, size_t vsSize, const void* ps, size_t psSize, const GraphicsDesc& desc);
    PipelinePtr CreatePipeline(const void* code, size_t size, u32 groupX, u32 groupY);

    // ImGui::Image id for a texture (valid for this frame's UI).
    u64 UiTextureId(Texture* t);

    u64 FrameNumber() const { return frameNumber; }
    std::string AdapterName() const { return adapterName; }
    u64 VideoMemoryUsage() const;
    // Textures of at least this many pixels (no RT/depth usage) are created sparse.
    void SetSparseThreshold(u64 pixels) { sparseThreshold = pixels; }
    bool SparseSupported() const { return sparseSupported; }
    u64 SparseBytes() const { return (u64)tilesInUse * 65536; }
    // Back the tiles of `t` covering `r` with memory (new tiles are zeroed).
    void MapRect(Texture* t, const Rect& r);
    // Copy the next presented frame (UI included) to the CPU (RGBA8).
    ReadbackPtr CaptureNextFrame();

private:
    friend class Texture;
    friend class Buffer;
    friend class CommandContext;
    friend struct GraphicsPipeline;
    friend struct ComputePipeline;

    u64 frameNumber = 0;
    u32 frameIndex = 0;
    CommandContext ctx;
    std::vector<ReadbackPtr> pendingReadbacks;
    ReadbackPtr captureRequest;
    std::string adapterName;
    u32 tilesInUse = 0;
    bool sparseSupported = false;
    u64 sparseThreshold = 6000ull * 6000ull;
    void ResolveReadbacks();

#ifdef SX_VULKAN
    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE;
        VkBuffer upload = VK_NULL_HANDLE;
        VkDeviceMemory uploadMem = VK_NULL_HANDLE;
        u8* uploadPtr = nullptr;
        size_t uploadOffset = 0;
        u64 submitted = 0;  // frame number submitted with this slot
    };
    u32 FindMemory(u32 typeBits, VkMemoryPropertyFlags props) const;
    bool CreateSwapchain(u32 w, u32 h);
    void DestroySwapchain();
    // Host-visible scratch for this frame (ring, or a dedicated buffer released later).
    VkBuffer AllocUpload(size_t size, size_t align, void** cpu, VkDeviceSize* offset);
    void Defer(std::function<void()> destroy);
    void ProcessDeferred(bool all);
    void DestroyTexture(Texture& t);

    HWND hwnd = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    u32 queueFamily = 0;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapExtent = {};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapViews;
    std::vector<VkSemaphore> renderDone;  // per swapchain image
    u32 swapIndex = 0;
    bool swapchainOk = false;
    VkPhysicalDeviceMemoryProperties memProps = {};
    VkDeviceSize uboAlign = 256;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkSampler samplers[3] = {};
    VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    Frame frames[kFramesInFlight];
    std::deque<std::pair<u64, std::function<void()>>> deferred;
    std::vector<Texture*> uiTexturesThisFrame;
    u64 completedFrame = 0;      // highest submission id known finished
    bool acquiredImage = false;  // a swapchain image is being rendered this frame
    bool memoryBudget = false;   // VK_EXT_memory_budget available
    // Sparse textures: 64 KB tiles carved from shared memory blocks. Tiles are
    // bound with vkQueueBindSparse as soon as they are needed, before the
    // commands using them are recorded (writes recorded earlier are lost on
    // NVIDIA). Each bind waits for the previous frame and signals `sparseSem`;
    // the frame's submission waits for the last bind. New tiles are zeroed by a
    // command buffer that runs ahead of the main one.
    int AllocTile();
    VkCommandBuffer PreCmd();
    std::vector<VkDeviceMemory> tileBlocks;
    std::vector<int> freeTiles;
    u32 tileMemType = ~0u;
    VkBuffer zeroBuf = VK_NULL_HANDLE;  // 64 KB of zeros (tile clears)
    VkDeviceMemory zeroMem = VK_NULL_HANDLE;
    bool zeroReady = false;
    VkCommandPool prePool[kFramesInFlight] = {};
    VkCommandBuffer preCmd[kFramesInFlight] = {};
    bool preOpen = false;
    VkSemaphore sparseSem = VK_NULL_HANDLE;  // timeline: one value per bind
    u64 sparseValue = 0, sparseWaited = 0;
    VkSemaphore timeline = VK_NULL_HANDLE;       // signaled with the submission id of each frame
#else
    struct FreeList {
        std::vector<int> free;
        int next = 0, capacity = 0;
        int Alloc() {
            if (!free.empty()) { int i = free.back(); free.pop_back(); return i; }
            return next < capacity ? next++ : -1;
        }
        void Free(int i) { if (i >= 0) free.push_back(i); }
    };
    struct Frame {
        ComPtr<ID3D12CommandAllocator> alloc;
        u64 fence = 0;
        ComPtr<ID3D12Resource> upload;
        u8* uploadPtr = nullptr;
        size_t uploadOffset = 0;
        u32 ringStart = 0, ringOffset = 0;
    };
    struct Deferred { u64 frame; ComPtr<ID3D12Resource> res; int srvCpu, uavCpu, uiSlot; int rtv = -1, dsv = -1; std::vector<int> tiles; };
    int AllocTile();
    void FlushDebugMessages();  // log debug-layer warnings/errors (SX_D3D12_DEBUG=1)
    ComPtr<ID3D12InfoQueue> infoQueue;
    ID3D12GraphicsCommandList* PreList();
    void AllocPersistentSrv(D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu);
    void FreePersistentSrv(D3D12_GPU_DESCRIPTOR_HANDLE gpu);

    void CreateBackBuffers();
    void ReleaseBackBuffers();
    void ProcessDeferred(bool all);
    void DeferRelease(Texture& t);
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(int i) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GpuHeapCpu(u32 i) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHeapGpu(u32 i) const;
    u32 AllocRing(u32 count);
    D3D12_GPU_VIRTUAL_ADDRESS AllocUpload(size_t size, size_t align, void** cpu);

    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain3> swapchain;
    HANDLE swapWaitable = nullptr;
    ComPtr<ID3D12DescriptorHeap> rtvHeap, cpuHeap, gpuHeap, texRtvHeap, dsvHeap;
    FreeList rtvSlots, dsvSlots;
    u32 dsvInc = 0;
    ComPtr<ID3D12Resource> backBuffers[kFramesInFlight];
    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    u64 fenceValue = 0;
    Frame frames[kFramesInFlight];
    u32 rtvInc = 0, srvInc = 0;
    FreeList cpuSlots, persistentSlots;
    int nullSrv = -1, nullUav = -1;
    std::deque<Deferred> deferred;
    std::vector<Texture*> uiTexturesThisFrame;
    bool tearing = false;
    // Sparse texture memory: heaps of 64 KB tiles shared by all reserved textures.
    std::vector<ComPtr<ID3D12Heap>> tileHeaps;
    std::vector<int> freeTiles;
    // Executed before the frame's main list (zeroing newly backed tiles).
    ComPtr<ID3D12CommandAllocator> preAlloc[kFramesInFlight];
    ComPtr<ID3D12GraphicsCommandList> preList;
    bool preOpen = false;
#endif
};

Device& Dev();  // global device instance

}  // namespace sx::gpu
