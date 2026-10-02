#pragma once
// Thin D3D12 layer for Shaderixel. Everything image-related runs as compute
// dispatches through one universal root signature:
//   b0 : 16 root constants         (small per-dispatch data)
//   b1 : root CBV from upload ring  (params, brush dabs, up to 64 KB)
//   t0..t7 : SRV table, u0..u3 : UAV table
//   s0 point-clamp, s1 linear-clamp, s2 linear-wrap (static samplers)
// The backend is isolated here so Vulkan/Metal can be added behind the same API.

#include "core/Common.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <functional>
#include <memory>
#include <deque>

namespace sx::gpu {

using Microsoft::WRL::ComPtr;

constexpr u32 kFramesInFlight = 2;
constexpr u32 kMaxSrv = 8;
constexpr u32 kMaxUav = 4;
constexpr u32 kRootConstants = 16;

enum class Format : u8 { RGBA8, RGBA16F, RGBA32F, R8, R16F, R32F, R32U, D32F };
enum TextureUsage : u32 { UsageRenderTarget = 1, UsageDepth = 2 };
DXGI_FORMAT ToDxgi(Format f);
u32 BytesPerPixel(Format f);

class Device;

class Texture {
public:
    ~Texture();
    u32 Width() const { return width; }
    u32 Height() const { return height; }
    Format GetFormat() const { return format; }
    ID3D12Resource* Resource() const { return res.Get(); }
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
    ComPtr<ID3D12Resource> res;
    u32 width = 0, height = 0;
    Format format = Format::RGBA16F;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES frameStartState = D3D12_RESOURCE_STATE_COMMON;  // state when this frame's list begins
    u64 stateFrame = ~0ull;
    bool sparse = false;
    u32 tileW = 0, tileH = 0, tilesX = 0, tilesY = 0, mappedTiles = 0;
    std::vector<int> tiles;  // heap slot per tile, -1 = unmapped
    int srvCpu = -1, uavCpu = -1;  // staging heap slots
    int uiSlot = -1;               // persistent shader-visible SRV (for ImGui)
    int rtv = -1, dsv = -1;        // render target / depth views (graphics)
};

class Buffer {
public:
    ~Buffer();
    D3D12_GPU_VIRTUAL_ADDRESS Address() const { return res ? res->GetGPUVirtualAddress() : 0; }
    u64 Size() const { return size; }
private:
    friend class Device;
    friend class CommandContext;
    ComPtr<ID3D12Resource> res;
    u64 size = 0;
};
using BufferPtr = std::shared_ptr<Buffer>;

struct GraphicsPipeline {
    ComPtr<ID3D12PipelineState> pso;
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
    ComPtr<ID3D12PipelineState> pso;
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
    ComPtr<ID3D12Resource> buffer;
    u64 fence = 0;
};
using ReadbackPtr = std::shared_ptr<Readback>;

class CommandContext {
public:
    ID3D12GraphicsCommandList* List() const { return list.Get(); }
    void Transition(Texture* t, D3D12_RESOURCE_STATES s);
    void UavBarrier(Texture* t);
    void Dispatch(const DispatchDesc& d);
    void CopyTexture(Texture* dst, Texture* src);
    // Copy src rect into dst at (dx,dy).
    void CopyRegion(Texture* dst, int dx, int dy, Texture* src, const Rect& srcRect);
    void Upload(Texture* dst, const void* pixels, u32 srcRowPitch);
    void Draw(const DrawDesc& d);
    ReadbackPtr ReadbackTexture(Texture* src, const Rect* region = nullptr);

private:
    friend class Device;
    ComPtr<ID3D12GraphicsCommandList> list;
    Device* dev = nullptr;
};

class Device {
public:
    bool Init(HWND hwnd);
    void Shutdown();
    void Resize(u32 w, u32 h);

    // Frame lifecycle. BeginFrame returns the context all work is recorded into.
    CommandContext& BeginFrame();
    void EndFrame(const std::function<void(ID3D12GraphicsCommandList*)>& drawUi);
    void WaitIdle();

    TexturePtr CreateTexture(u32 w, u32 h, Format f, const char* debugName = nullptr, u32 usage = 0);
    BufferPtr CreateBuffer(CommandContext& ctx, const void* data, size_t size, const char* debugName = nullptr);
    GraphicsPipelinePtr CreateGraphicsPipeline(const void* vs, size_t vsSize, const void* ps, size_t psSize, const GraphicsDesc& desc);
    PipelinePtr CreatePipeline(const void* dxil, size_t size, u32 groupX, u32 groupY);

    // Persistent shader-visible SRV for ImGui::Image. Textures used this frame are
    // transitioned to shader-resource state before the UI pass.
    u64 UiTextureId(Texture* t);

    ID3D12Device* D3D() const { return device.Get(); }
    ID3D12CommandQueue* Queue() const { return queue.Get(); }
    ID3D12DescriptorHeap* GpuHeap() const { return gpuHeap.Get(); }
    void AllocPersistentSrv(D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu);
    void FreePersistentSrv(D3D12_GPU_DESCRIPTOR_HANDLE gpu);
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

    void CreateBackBuffers();
    void ReleaseBackBuffers();
    void ProcessDeferred(bool all);
    void DeferRelease(Texture& t);
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(int i) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GpuHeapCpu(u32 i) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHeapGpu(u32 i) const;
    u32 AllocRing(u32 count);
    D3D12_GPU_VIRTUAL_ADDRESS AllocUpload(size_t size, size_t align, void** cpu);
    void ResolveReadbacks();

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
    u32 frameIndex = 0;
    u64 frameNumber = 0;
    CommandContext ctx;
    u32 rtvInc = 0, srvInc = 0;
    FreeList cpuSlots, persistentSlots;
    int nullSrv = -1, nullUav = -1;
    std::deque<Deferred> deferred;
    std::vector<Texture*> uiTexturesThisFrame;
    std::vector<ReadbackPtr> pendingReadbacks;
    ReadbackPtr captureRequest;
    std::string adapterName;
    bool tearing = false;
    // Sparse texture memory: heaps of 64 KB tiles shared by all reserved textures.
    std::vector<ComPtr<ID3D12Heap>> tileHeaps;
    std::vector<int> freeTiles;
    u32 tilesInUse = 0;
    bool sparseSupported = false;
    u64 sparseThreshold = 6000ull * 6000ull;
    // Executed before the frame's main list (zeroing newly backed tiles).
    ComPtr<ID3D12CommandAllocator> preAlloc[kFramesInFlight];
    ComPtr<ID3D12GraphicsCommandList> preList;
    bool preOpen = false;
};

Device& Dev();  // global device instance

}  // namespace sx::gpu
