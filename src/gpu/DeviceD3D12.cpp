#include "gpu/Device.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>

#include <algorithm>
#include <cassert>
#include <cstring>

namespace sx::gpu {

namespace {
Device g_device;

constexpr u32 kCpuHeapSize = 65536;
constexpr u32 kPersistentSize = 16384;
constexpr u32 kRingPerFrame = 32768;
constexpr size_t kUploadPerFrame = 64ull << 20;

#define SX_CHECK(x) do { HRESULT hr_ = (x); if (FAILED(hr_)) { Log("D3D12 failure 0x%08X at %s:%d: %s", (unsigned)hr_, __FILE__, __LINE__, #x); } } while (0)

D3D12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE t) {
    D3D12_HEAP_PROPERTIES p = {};
    p.Type = t;
    return p;
}

D3D12_RESOURCE_DESC BufferDesc(size_t size) {
    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}
}  // namespace

Device& Dev() { return g_device; }

const char* BackendName() { return "D3D12"; }

static DXGI_FORMAT ToDxgi(Format f) {
    switch (f) {
        case Format::RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case Format::RGBA32F: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case Format::R8: return DXGI_FORMAT_R8_UNORM;
        case Format::R16F: return DXGI_FORMAT_R16_FLOAT;
        case Format::R32F: return DXGI_FORMAT_R32_FLOAT;
        case Format::R32U: return DXGI_FORMAT_R32_UINT;
        case Format::D32F: return DXGI_FORMAT_R32_TYPELESS;
        case Format::RGBA16: return DXGI_FORMAT_R16G16B16A16_UNORM;
    }
    return DXGI_FORMAT_UNKNOWN;
}

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

Texture::~Texture() { if (res) g_device.DeferRelease(*this); }
Buffer::~Buffer() { if (res) g_device.deferred.push_back({g_device.frameNumber, std::move(res), -1, -1, -1}); }

// ------------------------------------------------------------------ init

bool Device::Init(HWND hwnd) {
    bool debugLayer = false;
#ifdef _DEBUG
    debugLayer = true;
#endif
    if (GetEnvironmentVariableA("SX_D3D12_DEBUG", nullptr, 0) > 0) debugLayer = true;
    if (debugLayer) {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
        else Log("D3D12 debug layer unavailable");
    }
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return false;

    // Highest-performance adapter first.
    for (u32 i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
            adapterName = Narrow(desc.Description);
            break;
        }
    }
    if (!device) { Log("No D3D12 feature level 12_0 adapter found."); return false; }
    if (debugLayer) device.As(&infoQueue);
    Log("GPU: %s", adapterName.c_str());

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    SX_CHECK(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)));

    BOOL allowTearing = FALSE;
    factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    tearing = allowTearing;

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = kFramesInFlight;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SampleDesc.Count = 1;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | (tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(factory->CreateSwapChainForHwnd(queue.Get(), hwnd, &sd, nullptr, nullptr, &sc1))) return false;
    sc1.As(&swapchain);
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    swapchain->SetMaximumFrameLatency(1);  // lowest input latency
    swapWaitable = swapchain->GetFrameLatencyWaitableObject();

    rtvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    srvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC hd = {};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.NumDescriptors = kFramesInFlight;
    SX_CHECK(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap)));

    hd.NumDescriptors = 1024;
    SX_CHECK(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&texRtvHeap)));
    rtvSlots.capacity = 1024;
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    SX_CHECK(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&dsvHeap)));
    dsvSlots.capacity = 1024;
    dsvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = kCpuHeapSize;
    SX_CHECK(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&cpuHeap)));
    cpuSlots.capacity = kCpuHeapSize;

    hd.NumDescriptors = kPersistentSize + kRingPerFrame * kFramesInFlight;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    SX_CHECK(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&gpuHeap)));
    persistentSlots.capacity = kPersistentSize;

    // Null descriptors for unbound slots.
    nullSrv = cpuSlots.Alloc();
    D3D12_SHADER_RESOURCE_VIEW_DESC nsd = {};
    nsd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    nsd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    nsd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    nsd.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(nullptr, &nsd, CpuHandle(nullSrv));
    nullUav = cpuSlots.Alloc();
    D3D12_UNORDERED_ACCESS_VIEW_DESC nud = {};
    nud.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    nud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(nullptr, nullptr, &nud, CpuHandle(nullUav));

    // Universal compute root signature.
    D3D12_DESCRIPTOR_RANGE srvRange = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, kMaxSrv, 0, 0, 0};
    D3D12_DESCRIPTOR_RANGE uavRange = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, kMaxUav, 0, 0, 0};
    D3D12_ROOT_PARAMETER params[4] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants = {0, 0, kRootConstants};
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].Descriptor = {1, 0};
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].DescriptorTable = {1, &srvRange};
    params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[3].DescriptorTable = {1, &uavRange};
    D3D12_STATIC_SAMPLER_DESC samplers[3] = {};
    D3D12_FILTER filters[3] = {D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_FILTER_MIN_MAG_MIP_LINEAR};
    D3D12_TEXTURE_ADDRESS_MODE modes[3] = {D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_WRAP};
    for (u32 i = 0; i < 3; ++i) {
        samplers[i].Filter = filters[i];
        samplers[i].AddressU = samplers[i].AddressV = samplers[i].AddressW = modes[i];
        samplers[i].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[i].ShaderRegister = i;
        samplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    D3D12_ROOT_SIGNATURE_DESC rsd = {4, params, 3, samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) {
        Log("Root signature error: %s", err ? (const char*)err->GetBufferPointer() : "?");
        return false;
    }
    SX_CHECK(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig)));

    for (u32 i = 0; i < kFramesInFlight; ++i) {
        Frame& f = frames[i];
        SX_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&f.alloc)));
        auto hp = HeapProps(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = BufferDesc(kUploadPerFrame);
        SX_CHECK(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&f.upload)));
        f.upload->Map(0, nullptr, (void**)&f.uploadPtr);
        f.ringStart = kPersistentSize + i * kRingPerFrame;
    }
    SX_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[0].alloc.Get(), nullptr, IID_PPV_ARGS(&ctx.list)));
    ctx.list->Close();
    ctx.dev = this;

    for (u32 i = 0; i < kFramesInFlight; ++i)
        SX_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&preAlloc[i])));
    SX_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, preAlloc[0].Get(), nullptr, IID_PPV_ARGS(&preList)));
    preList->Close();
    D3D12_FEATURE_DATA_D3D12_OPTIONS opts = {};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts))))
        sparseSupported = opts.TiledResourcesTier >= D3D12_TILED_RESOURCES_TIER_2;  // tier 2: unmapped reads return 0
    Log("Sparse textures: %s", sparseSupported ? "yes" : "no");

    SX_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    CreateBackBuffers();
    return true;
}

void Device::Shutdown() {
    WaitIdle();
    ProcessDeferred(true);
    ReleaseBackBuffers();
    if (fenceEvent) CloseHandle(fenceEvent);
}

void Device::CreateBackBuffers() {
    for (u32 i = 0; i < kFramesInFlight; ++i) {
        swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE h = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += i * rtvInc;
        device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, h);
    }
}

void Device::ReleaseBackBuffers() {
    for (auto& b : backBuffers) b.Reset();
}

void Device::Resize(u32 w, u32 h) {
    if (w == 0 || h == 0) return;
    WaitIdle();
    ReleaseBackBuffers();
    swapchain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN,
                             DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | (tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
    CreateBackBuffers();
}

void Device::WaitIdle() {
    if (!queue) return;
    queue->Signal(fence.Get(), ++fenceValue);
    fence->SetEventOnCompletion(fenceValue, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    ResolveReadbacks();
}

// ------------------------------------------------------------------ frame

CommandContext& Device::BeginFrame() {
    WaitForSingleObject(swapWaitable, 1000);
    Frame& f = frames[frameIndex];
    if (fence->GetCompletedValue() < f.fence) {
        fence->SetEventOnCompletion(f.fence, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    ResolveReadbacks();
    ProcessDeferred(false);
    f.alloc->Reset();
    preAlloc[frameIndex]->Reset();
    ctx.list->Reset(f.alloc.Get(), nullptr);
    f.uploadOffset = 0;
    f.ringOffset = 0;
    ID3D12DescriptorHeap* heaps[] = {gpuHeap.Get()};
    ctx.list->SetDescriptorHeaps(1, heaps);
    ctx.list->SetComputeRootSignature(rootSig.Get());
    uiTexturesThisFrame.clear();
    return ctx;
}

bool Device::InitImGui() {
    ImGui_ImplDX12_InitInfo init;
    init.Device = device.Get();
    init.CommandQueue = queue.Get();
    init.NumFramesInFlight = kFramesInFlight;
    init.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init.SrvDescriptorHeap = gpuHeap.Get();
    init.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* c, D3D12_GPU_DESCRIPTOR_HANDLE* g) { Dev().AllocPersistentSrv(c, g); };
    init.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE g) { Dev().FreePersistentSrv(g); };
    return ImGui_ImplDX12_Init(&init);
}

void Device::ImGuiNewFrame() { ImGui_ImplDX12_NewFrame(); }
void Device::ShutdownImGui() { ImGui_ImplDX12_Shutdown(); }

void Device::EndFrame() {
    auto* list = ctx.list.Get();
    for (Texture* t : uiTexturesThisFrame) ctx.Transition(t, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    u32 bb = swapchain->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = backBuffers[bb].Get();
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    list->ResourceBarrier(1, &b);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += bb * rtvInc;
    const float clear[4] = {0.07f, 0.07f, 0.08f, 1.0f};
    list->ClearRenderTargetView(rtv, clear, 0, nullptr);
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = {gpuHeap.Get()};
    list->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list);

    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    if (captureRequest) {
        auto& rb = captureRequest;
        D3D12_RESOURCE_DESC desc = backBuffers[bb]->GetDesc();
        rb->width = (u32)desc.Width;
        rb->height = desc.Height;
        rb->format = Format::RGBA8;
        rb->rowPitch = (rb->width * 4 + 255) & ~255u;
        auto hp = HeapProps(D3D12_HEAP_TYPE_READBACK);
        auto bd = BufferDesc((size_t)rb->rowPitch * rb->height);
        SX_CHECK(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb->buffer)));
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        list->ResourceBarrier(1, &b);
        D3D12_TEXTURE_COPY_LOCATION d = {};
        d.pResource = rb->buffer.Get();
        d.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        d.PlacedFootprint.Footprint = {desc.Format, rb->width, rb->height, 1, rb->rowPitch};
        D3D12_TEXTURE_COPY_LOCATION s = {backBuffers[bb].Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
        list->CopyTextureRegion(&d, 0, 0, 0, &s, nullptr);
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        pendingReadbacks.push_back(rb);
        captureRequest.reset();
    }
    list->ResourceBarrier(1, &b);
    list->Close();
    FlushDebugMessages();
    if (preOpen) {
        preList->Close();
        preOpen = false;
        ID3D12CommandList* lists[] = {preList.Get(), list};
        queue->ExecuteCommandLists(2, lists);
    } else {
        ID3D12CommandList* lists[] = {list};
        queue->ExecuteCommandLists(1, lists);
    }

    swapchain->Present(1, 0);
    queue->Signal(fence.Get(), ++fenceValue);
    frames[frameIndex].fence = fenceValue;
    for (auto& r : pendingReadbacks) if (r->fence == 0) r->fence = fenceValue;
    frameIndex = (frameIndex + 1) % kFramesInFlight;
    ++frameNumber;
}

void Device::ResolveReadbacks() {
    u64 done = fence->GetCompletedValue();
    for (size_t i = 0; i < pendingReadbacks.size();) {
        auto& r = pendingReadbacks[i];
        if (r->fence != 0 && r->fence <= done) {
            u8* p = nullptr;
            r->buffer->Map(0, nullptr, (void**)&p);
            u32 rowBytes = r->width * BytesPerPixel(r->format);
            r->data.resize((size_t)rowBytes * r->height);
            for (u32 y = 0; y < r->height; ++y) memcpy(r->data.data() + (size_t)y * rowBytes, p + (size_t)y * r->rowPitch, rowBytes);
            r->buffer->Unmap(0, nullptr);
            r->buffer.Reset();
            r->ready = true;
            pendingReadbacks.erase(pendingReadbacks.begin() + i);
        } else {
            ++i;
        }
    }
}

// ------------------------------------------------------------------ resources

D3D12_CPU_DESCRIPTOR_HANDLE Device::CpuHandle(int i) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = cpuHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += (size_t)i * srvInc;
    return h;
}
D3D12_CPU_DESCRIPTOR_HANDLE Device::GpuHeapCpu(u32 i) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = gpuHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += (size_t)i * srvInc;
    return h;
}
D3D12_GPU_DESCRIPTOR_HANDLE Device::GpuHeapGpu(u32 i) const {
    D3D12_GPU_DESCRIPTOR_HANDLE h = gpuHeap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += (u64)i * srvInc;
    return h;
}

void Device::AllocPersistentSrv(D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
    int i = persistentSlots.Alloc();
    assert(i >= 0);
    *cpu = GpuHeapCpu(i);
    *gpu = GpuHeapGpu(i);
}

void Device::FreePersistentSrv(D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    persistentSlots.Free((int)((gpu.ptr - gpuHeap->GetGPUDescriptorHandleForHeapStart().ptr) / srvInc));
}

u32 Device::AllocRing(u32 count) {
    Frame& f = frames[frameIndex];
    if (f.ringOffset + count > kRingPerFrame) {
        Log("Descriptor ring overflow");
        f.ringOffset = 0;
    }
    u32 i = f.ringStart + f.ringOffset;
    f.ringOffset += count;
    return i;
}

D3D12_GPU_VIRTUAL_ADDRESS Device::AllocUpload(size_t size, size_t align, void** cpu) {
    Frame& f = frames[frameIndex];
    size_t off = (f.uploadOffset + align - 1) & ~(align - 1);
    if (off + size > kUploadPerFrame) {
        // Large one-off upload: dedicated buffer released after the frame completes.
        ComPtr<ID3D12Resource> buf;
        auto hp = HeapProps(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = BufferDesc(size);
        SX_CHECK(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)));
        buf->Map(0, nullptr, cpu);
        D3D12_GPU_VIRTUAL_ADDRESS va = buf->GetGPUVirtualAddress();
        deferred.push_back({frameNumber, buf, -1, -1, -1});
        return va;
    }
    f.uploadOffset = off + size;
    *cpu = f.uploadPtr + off;
    return f.upload->GetGPUVirtualAddress() + off;
}

TexturePtr Device::CreateTexture(u32 w, u32 h, Format fmt, const char* debugName, u32 usage) {
    auto t = std::make_shared<Texture>();
    t->width = w;
    t->height = h;
    t->format = fmt;
    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = w;
    d.Height = h;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.Format = ToDxgi(fmt);
    d.SampleDesc.Count = 1;
    bool isDepth = fmt == Format::D32F;
    d.Flags = isDepth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if (usage & UsageRenderTarget) d.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    auto hp = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = isDepth ? DXGI_FORMAT_D32_FLOAT : d.Format;
    clear.DepthStencil.Depth = 1.0f;
    const D3D12_CLEAR_VALUE* pClear = (isDepth || (usage & UsageRenderTarget)) ? &clear : nullptr;
    if (sparseSupported && !usage && !isDepth && (u64)w * h >= sparseThreshold) {
        d.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
        if (SUCCEEDED(device->CreateReservedResource(&d, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&t->res)))) {
            UINT numTiles = 0, subCount = 1;
            D3D12_PACKED_MIP_INFO packed = {};
            D3D12_TILE_SHAPE shape = {};
            D3D12_SUBRESOURCE_TILING sub = {};
            device->GetResourceTiling(t->res.Get(), &numTiles, &packed, &shape, &subCount, 0, &sub);
            t->sparse = true;
            t->tileW = shape.WidthInTexels;
            t->tileH = shape.HeightInTexels;
            t->tilesX = sub.WidthInTiles;
            t->tilesY = sub.HeightInTiles;
            t->tiles.assign((size_t)t->tilesX * t->tilesY, -1);
        } else {
            d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        }
    }
    if (!t->sparse && FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, pClear, IID_PPV_ARGS(&t->res)))) {
        Log("CreateTexture %ux%u failed (out of video memory?)", w, h);
        return nullptr;
    }
    if (debugName) t->res->SetName(Widen(debugName).c_str());
    t->state = D3D12_RESOURCE_STATE_COMMON;
    t->srvCpu = cpuSlots.Alloc();
    t->uavCpu = cpuSlots.Alloc();
    D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = isDepth ? DXGI_FORMAT_R32_FLOAT : d.Format;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(t->res.Get(), &sd, CpuHandle(t->srvCpu));
    if (!isDepth) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {};
        ud.Format = d.Format;
        ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(t->res.Get(), nullptr, &ud, CpuHandle(t->uavCpu));
    } else {
        device->CopyDescriptorsSimple(1, CpuHandle(t->uavCpu), CpuHandle(nullUav), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        t->dsv = dsvSlots.Alloc();
        D3D12_DEPTH_STENCIL_VIEW_DESC dd = {};
        dd.Format = DXGI_FORMAT_D32_FLOAT;
        dd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        D3D12_CPU_DESCRIPTOR_HANDLE dh = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        dh.ptr += (size_t)t->dsv * dsvInc;
        device->CreateDepthStencilView(t->res.Get(), &dd, dh);
    }
    if (usage & UsageRenderTarget) {
        t->rtv = rtvSlots.Alloc();
        D3D12_CPU_DESCRIPTOR_HANDLE rh = texRtvHeap->GetCPUDescriptorHandleForHeapStart();
        rh.ptr += (size_t)t->rtv * rtvInc;
        device->CreateRenderTargetView(t->res.Get(), nullptr, rh);
    }
    return t;
}

BufferPtr Device::CreateBuffer(CommandContext& c, const void* data, size_t size, const char* debugName) {
    auto b = std::make_shared<Buffer>();
    b->size = size;
    auto hp = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    auto bd = BufferDesc(size);
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&b->res)))) return nullptr;
    if (debugName) b->res->SetName(Widen(debugName).c_str());
    void* mem = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS va = AllocUpload(size, 256, &mem);
    memcpy(mem, data, size);
    auto& f = frames[frameIndex];
    ID3D12Resource* src = nullptr;
    u64 off = 0;
    if (va >= f.upload->GetGPUVirtualAddress() && va < f.upload->GetGPUVirtualAddress() + kUploadPerFrame) {
        src = f.upload.Get();
        off = va - f.upload->GetGPUVirtualAddress();
    } else {
        src = deferred.back().res.Get();
    }
    D3D12_RESOURCE_BARRIER br = {};
    br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    br.Transition.pResource = b->res.Get();
    br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    br.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    br.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    c.list->ResourceBarrier(1, &br);
    c.list->CopyBufferRegion(b->res.Get(), 0, src, off, size);
    br.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    br.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;
    c.list->ResourceBarrier(1, &br);
    return b;
}

GraphicsPipelinePtr Device::CreateGraphicsPipeline(const void* vs, size_t vsSize, const void* ps, size_t psSize, const GraphicsDesc& g) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = rootSig.Get();
    pd.VS = {vs, vsSize};
    pd.PS = {ps, psSize};
    static const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    if (g.meshLayout) pd.InputLayout = {layout, 3};
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = g.cullBack ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
    pd.RasterizerState.FrontCounterClockwise = TRUE;
    pd.RasterizerState.DepthClipEnable = TRUE;
    for (int i = 0; i < 8; ++i) pd.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.DepthStencilState.DepthEnable = g.useDepth;
    pd.DepthStencilState.DepthWriteMask = g.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    pd.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pd.SampleMask = UINT_MAX;
    pd.NumRenderTargets = (UINT)g.colorFormats.size();
    for (size_t i = 0; i < g.colorFormats.size(); ++i) pd.RTVFormats[i] = ToDxgi(g.colorFormats[i]);
    pd.DSVFormat = g.useDepth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
    pd.SampleDesc.Count = 1;
    auto p = std::make_shared<GraphicsPipeline>();
    if (FAILED(device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&p->pso)))) {
        Log("CreateGraphicsPipelineState failed");
        return nullptr;
    }
    return p;
}

void Device::DeferRelease(Texture& t) {
    std::vector<int> tiles;
    for (int s : t.tiles) if (s >= 0) tiles.push_back(s);
    t.tiles.clear();
    t.mappedTiles = 0;
    deferred.push_back({frameNumber, std::move(t.res), t.srvCpu, t.uavCpu, t.uiSlot, t.rtv, t.dsv, std::move(tiles)});
    t.srvCpu = t.uavCpu = t.uiSlot = t.rtv = t.dsv = -1;
    // Drop from this frame's UI list if present.
    std::erase(uiTexturesThisFrame, &t);
}

void Device::ProcessDeferred(bool all) {
    while (!deferred.empty() && (all || deferred.front().frame + kFramesInFlight + 1 <= frameNumber)) {
        auto& d = deferred.front();
        cpuSlots.Free(d.srvCpu);
        cpuSlots.Free(d.uavCpu);
        persistentSlots.Free(d.uiSlot);
        rtvSlots.Free(d.rtv);
        dsvSlots.Free(d.dsv);
        for (int s : d.tiles) freeTiles.push_back(s);
        tilesInUse -= (u32)d.tiles.size();
        deferred.pop_front();
    }
}

// ------------------------------------------------------------------ sparse textures

constexpr int kTilesPerHeap = 512;  // 32 MB heaps

int Device::AllocTile() {
    if (freeTiles.empty()) {
        D3D12_HEAP_DESC hd = {};
        hd.SizeInBytes = (u64)kTilesPerHeap * 65536;
        hd.Properties = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
        hd.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        hd.Flags = D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
        ComPtr<ID3D12Heap> heap;
        if (FAILED(device->CreateHeap(&hd, IID_PPV_ARGS(&heap)))) {
            Log("Sparse heap allocation failed (out of video memory?)");
            return -1;
        }
        int base = (int)tileHeaps.size() * kTilesPerHeap;
        tileHeaps.push_back(heap);
        for (int i = kTilesPerHeap - 1; i >= 0; --i) freeTiles.push_back(base + i);
    }
    int s = freeTiles.back();
    freeTiles.pop_back();
    ++tilesInUse;
    return s;
}

void Device::FlushDebugMessages() {
    if (!infoQueue) return;
    UINT64 n = infoQueue->GetNumStoredMessages();
    for (UINT64 i = 0; i < n; ++i) {
        SIZE_T len = 0;
        infoQueue->GetMessage(i, nullptr, &len);
        std::vector<u8> buf(len);
        auto* m = (D3D12_MESSAGE*)buf.data();
        if (SUCCEEDED(infoQueue->GetMessage(i, m, &len)) && m->Severity <= D3D12_MESSAGE_SEVERITY_WARNING)
            Log("[d3d12] %s", m->pDescription);
    }
    infoQueue->ClearStoredMessages();
}

ID3D12GraphicsCommandList* Device::PreList() {
    if (!preOpen) {
        SX_CHECK(preList->Reset(preAlloc[frameIndex].Get(), nullptr));
        ID3D12DescriptorHeap* heaps[] = {gpuHeap.Get()};
        preList->SetDescriptorHeaps(1, heaps);
        preOpen = true;
    }
    return preList.Get();
}

void Device::MapRect(Texture* t, const Rect& r0) {
    if (!t || !t->sparse || !t->res) return;
    Rect r = r0.Clipped((int)t->width, (int)t->height);
    if (r.Empty()) return;
    u32 tx0 = r.x0 / t->tileW, tx1 = (r.x1 - 1) / t->tileW, ty0 = r.y0 / t->tileH, ty1 = (r.y1 - 1) / t->tileH;
    std::vector<D3D12_TILED_RESOURCE_COORDINATE> coords;
    std::vector<int> slots;
    std::vector<D3D12_RECT> rects;
    for (u32 ty = ty0; ty <= ty1; ++ty)
        for (u32 tx = tx0; tx <= tx1; ++tx) {
            int& slot = t->tiles[(size_t)ty * t->tilesX + tx];
            if (slot >= 0) continue;
            int s = AllocTile();
            if (s < 0) break;
            slot = s;
            ++t->mappedTiles;
            coords.push_back({tx, ty, 0, 0});
            slots.push_back(s);
            rects.push_back({(LONG)(tx * t->tileW), (LONG)(ty * t->tileH), (LONG)std::min(t->width, (tx + 1) * t->tileW),
                             (LONG)std::min(t->height, (ty + 1) * t->tileH)});
        }
    if (coords.empty()) return;
    // One UpdateTileMappings per heap involved. It is a queue operation, so it
    // takes effect before this frame's command lists execute.
    std::vector<size_t> order(coords.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return slots[a] < slots[b]; });
    for (size_t i = 0; i < order.size();) {
        int heap = slots[order[i]] / kTilesPerHeap;
        std::vector<D3D12_TILED_RESOURCE_COORDINATE> c;
        std::vector<D3D12_TILE_REGION_SIZE> sizes;
        std::vector<UINT> starts, counts;
        for (; i < order.size() && slots[order[i]] / kTilesPerHeap == heap; ++i) {
            c.push_back(coords[order[i]]);
            sizes.push_back({1, FALSE, 1, 1, 1});
            starts.push_back((UINT)(slots[order[i]] % kTilesPerHeap));
            counts.push_back(1);
        }
        queue->UpdateTileMappings(t->res.Get(), (UINT)c.size(), c.data(), sizes.data(), tileHeaps[heap].Get(), (UINT)starts.size(), nullptr,
                                  starts.data(), counts.data(), D3D12_TILE_MAPPING_FLAG_NONE);
    }
    // Zero the new tiles (recycled memory) before anything in this frame's main list touches them.
    auto* pl = PreList();
    D3D12_RESOURCE_STATES st = t->stateFrame == frameNumber ? t->frameStartState : t->state;
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = t->res.Get();
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = st;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    if (st != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) pl->ResourceBarrier(1, &b);
    u32 slot = AllocRing(1);
    device->CopyDescriptorsSimple(1, GpuHeapCpu(slot), CpuHandle(t->uavCpu), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const float zero[4] = {0, 0, 0, 0};
    // One rect per call: large rect arrays crash the runtime/driver (fail-fast).
    for (const D3D12_RECT& rc : rects) pl->ClearUnorderedAccessViewFloat(GpuHeapGpu(slot), CpuHandle(t->uavCpu), t->res.Get(), zero, 1, &rc);
    if (st != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
        pl->ResourceBarrier(1, &b);
    } else {
        D3D12_RESOURCE_BARRIER u = {};
        u.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        u.UAV.pResource = t->res.Get();
        pl->ResourceBarrier(1, &u);
    }
}

PipelinePtr Device::CreatePipeline(const void* dxil, size_t size, u32 groupX, u32 groupY) {
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = rootSig.Get();
    pd.CS = {dxil, size};
    auto p = std::make_shared<ComputePipeline>();
    if (FAILED(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&p->pso)))) {
        Log("CreateComputePipelineState failed");
        return nullptr;
    }
    p->groupX = groupX;
    p->groupY = groupY;
    return p;
}

u64 Device::UiTextureId(Texture* t) {
    if (!t) return 0;
    if (t->uiSlot < 0) {
        t->uiSlot = persistentSlots.Alloc();
        if (t->uiSlot < 0) return 0;
    }
    // Refresh every use: cheap, and keeps the slot valid if the view changes.
    device->CopyDescriptorsSimple(1, GpuHeapCpu(t->uiSlot), CpuHandle(t->srvCpu), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    if (std::find(uiTexturesThisFrame.begin(), uiTexturesThisFrame.end(), t) == uiTexturesThisFrame.end())
        uiTexturesThisFrame.push_back(t);
    return GpuHeapGpu(t->uiSlot).ptr;
}

ReadbackPtr Device::CaptureNextFrame() {
    captureRequest = std::make_shared<Readback>();
    return captureRequest;
}

u64 Device::VideoMemoryUsage() const {
    ComPtr<IDXGIAdapter3> a3;
    if (FAILED(adapter.As(&a3))) return 0;
    DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
    a3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
    return info.CurrentUsage;
}

u64 Device::VideoMemoryBudget() const {
    ComPtr<IDXGIAdapter3> a3;
    if (FAILED(adapter.As(&a3))) return 0;
    DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
    a3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
    return info.Budget;
}

u64 Device::VideoMemoryTotal() const {
    DXGI_ADAPTER_DESC1 desc = {};
    if (!adapter || FAILED(adapter->GetDesc1(&desc))) return 0;
    return desc.DedicatedVideoMemory;
}

u64 Device::AdapterLuid() const {
    DXGI_ADAPTER_DESC1 desc = {};
    if (!adapter || FAILED(adapter->GetDesc1(&desc))) return 0;
    return ((u64)(u32)desc.AdapterLuid.HighPart << 32) | desc.AdapterLuid.LowPart;
}

// ------------------------------------------------------------------ command context

void CommandContext::Transition(Texture* t, D3D12_RESOURCE_STATES s) {
    if (!t || !t->res) return;
    if (t->stateFrame != dev->frameNumber) {
        t->frameStartState = t->state;
        t->stateFrame = dev->frameNumber;
    }
    if (t->state == s) {
        if (s == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) UavBarrier(t);
        return;
    }
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = t->res.Get();
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = t->state;
    b.Transition.StateAfter = s;
    list->ResourceBarrier(1, &b);
    t->state = s;
}

void CommandContext::UavBarrier(Texture* t) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    b.UAV.pResource = t ? t->res.Get() : nullptr;
    list->ResourceBarrier(1, &b);
}

void CommandContext::Dispatch(const DispatchDesc& d) {
    if (!d.pipeline || !d.pipeline->pso) return;
    for (u32 i = 0; i < kMaxSrv; ++i) if (d.srv[i]) Transition(d.srv[i], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    for (u32 i = 0; i < kMaxUav; ++i) {
        if (!d.uav[i]) continue;
        if (d.uav[i]->sparse && !(d.uavNoMap & (1u << i)))
            {
            const Rect& r = d.uavRect[i];
            bool unset = r.x0 == 0 && r.y0 == 0 && r.x1 == 0 && r.y1 == 0;
            dev->MapRect(d.uav[i], unset ? Rect{0, 0, (int)d.uav[i]->width, (int)d.uav[i]->height} : r);
        }
        Transition(d.uav[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    u32 base = dev->AllocRing(kMaxSrv + kMaxUav);
    for (u32 i = 0; i < kMaxSrv; ++i) {
        int src = d.srv[i] ? d.srv[i]->srvCpu : dev->nullSrv;
        dev->device->CopyDescriptorsSimple(1, dev->GpuHeapCpu(base + i), dev->CpuHandle(src), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    for (u32 i = 0; i < kMaxUav; ++i) {
        int src = d.uav[i] ? d.uav[i]->uavCpu : dev->nullUav;
        dev->device->CopyDescriptorsSimple(1, dev->GpuHeapCpu(base + kMaxSrv + i), dev->CpuHandle(src), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    list->SetPipelineState(d.pipeline->pso.Get());
    if (d.numConstants) list->SetComputeRoot32BitConstants(0, d.numConstants, d.constants, 0);
    // b1 must always be bound with valid memory; use a small zero block if unused.
    static const u8 zeros[256] = {};
    const void* cb = d.cbData ? d.cbData : zeros;
    size_t cbSize = d.cbData ? d.cbSize : sizeof(zeros);
    void* dst = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS va = dev->AllocUpload((cbSize + 255) & ~size_t(255), 256, &dst);
    memcpy(dst, cb, cbSize);
    list->SetComputeRootConstantBufferView(1, va);
    list->SetComputeRootDescriptorTable(2, dev->GpuHeapGpu(base));
    list->SetComputeRootDescriptorTable(3, dev->GpuHeapGpu(base + kMaxSrv));
    u32 gx = (d.threadsX + d.pipeline->groupX - 1) / d.pipeline->groupX;
    u32 gy = (d.threadsY + d.pipeline->groupY - 1) / d.pipeline->groupY;
    if (gx && gy) list->Dispatch(gx, gy, 1);
}

void CommandContext::Draw(const DrawDesc& d) {
    if (!d.pipeline || !d.pipeline->pso) return;
    for (u32 i = 0; i < kMaxSrv; ++i) if (d.srv[i]) Transition(d.srv[i], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[4];
    UINT numRt = 0;
    Texture* first = nullptr;
    for (int i = 0; i < 4; ++i) {
        if (!d.rt[i]) break;
        Transition(d.rt[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
        rtvs[numRt] = dev->texRtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvs[numRt].ptr += (size_t)d.rt[i]->rtv * dev->rtvInc;
        if (d.clearColor) list->ClearRenderTargetView(rtvs[numRt], &d.clearValue.x, 0, nullptr);
        if (!first) first = d.rt[i];
        ++numRt;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
    if (d.depth) {
        Transition(d.depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        dsv = dev->dsvHeap->GetCPUDescriptorHandleForHeapStart();
        dsv.ptr += (size_t)d.depth->dsv * dev->dsvInc;
        if (d.clearDepth) list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        if (!first) first = d.depth;
    }
    if (!first) return;
    list->OMSetRenderTargets(numRt, numRt ? rtvs : nullptr, FALSE, d.depth ? &dsv : nullptr);
    D3D12_VIEWPORT vp = {0, 0, (float)first->width, (float)first->height, 0, 1};
    D3D12_RECT sc = {0, 0, (LONG)first->width, (LONG)first->height};
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);

    u32 base = dev->AllocRing(kMaxSrv + kMaxUav);
    for (u32 i = 0; i < kMaxSrv; ++i) {
        int src = d.srv[i] ? d.srv[i]->srvCpu : dev->nullSrv;
        dev->device->CopyDescriptorsSimple(1, dev->GpuHeapCpu(base + i), dev->CpuHandle(src), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    for (u32 i = 0; i < kMaxUav; ++i)
        dev->device->CopyDescriptorsSimple(1, dev->GpuHeapCpu(base + kMaxSrv + i), dev->CpuHandle(dev->nullUav), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    list->SetGraphicsRootSignature(dev->rootSig.Get());
    list->SetPipelineState(d.pipeline->pso.Get());
    if (d.numConstants) list->SetGraphicsRoot32BitConstants(0, d.numConstants, d.constants, 0);
    static const u8 zeros[256] = {};
    const void* cb = d.cbData ? d.cbData : zeros;
    size_t cbSize = d.cbData ? d.cbSize : sizeof(zeros);
    void* dst = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS va = dev->AllocUpload((cbSize + 255) & ~size_t(255), 256, &dst);
    memcpy(dst, cb, cbSize);
    list->SetGraphicsRootConstantBufferView(1, va);
    list->SetGraphicsRootDescriptorTable(2, dev->GpuHeapGpu(base));
    list->SetGraphicsRootDescriptorTable(3, dev->GpuHeapGpu(base + kMaxSrv));
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (d.vertices) {
        D3D12_VERTEX_BUFFER_VIEW vbv = {d.vertices->res->GetGPUVirtualAddress(), (UINT)d.vertices->Size(), d.vertexStride};
        list->IASetVertexBuffers(0, 1, &vbv);
    }
    if (d.indices) {
        D3D12_INDEX_BUFFER_VIEW ibv = {d.indices->res->GetGPUVirtualAddress(), (UINT)d.indices->Size(), DXGI_FORMAT_R32_UINT};
        list->IASetIndexBuffer(&ibv);
        list->DrawIndexedInstanced(d.count, 1, 0, 0, 0);
    } else {
        list->DrawInstanced(d.count, 1, 0, 0);
    }
    // Restore compute state for subsequent dispatches.
    list->SetComputeRootSignature(dev->rootSig.Get());
}

void CommandContext::CopyTexture(Texture* dst, Texture* src) {
    if (src->sparse || dst->sparse) {
        CopyRegion(dst, 0, 0, src, {0, 0, (int)std::min(src->width, dst->width), (int)std::min(src->height, dst->height)});
        return;
    }
    Transition(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    Transition(dst, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(dst->res.Get(), src->res.Get());
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
    if (r.Empty()) return;
    Transition(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    Transition(dst, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION d = {dst->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    D3D12_TEXTURE_COPY_LOCATION s = {src->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    auto copy = [&](const Rect& q) {
        D3D12_BOX box = {(UINT)q.x0, (UINT)q.y0, 0, (UINT)q.x1, (UINT)q.y1, 1};
        list->CopyTextureRegion(&d, (UINT)(dx + q.x0 - r.x0), (UINT)(dy + q.y0 - r.y0), 0, &s, &box);
    };
    auto target = [&](const Rect& q) { return Rect{dx + q.x0 - r.x0, dy + q.y0 - r.y0, dx + q.x1 - r.x0, dy + q.y1 - r.y0}; };
    if (!src->sparse) {
        dev->MapRect(dst, target(r));
        copy(r);
        return;
    }
    // Sparse source: walk its tiles. A piece is copied when the source has data
    // there (the destination gets backed) or the destination already holds data
    // that must become zero; pieces empty on both sides are skipped.
    // Horizontal runs with the same mapping need are merged into one copy.
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

void CommandContext::Upload(Texture* dst, const void* pixels, u32 srcRowPitch) {
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
    UINT64 total = 0;
    D3D12_RESOURCE_DESC desc = dst->res->GetDesc();
    dev->device->GetCopyableFootprints(&desc, 0, 1, 0, &fp, nullptr, nullptr, &total);
    void* mem = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS va = dev->AllocUpload(total, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &mem);
    // Find which resource backs `va`: either the frame ring or a dedicated buffer.
    auto& f = dev->frames[dev->frameIndex];
    ID3D12Resource* srcRes = nullptr;
    UINT64 baseOffset = 0;
    if (va >= f.upload->GetGPUVirtualAddress() && va < f.upload->GetGPUVirtualAddress() + kUploadPerFrame) {
        srcRes = f.upload.Get();
        baseOffset = va - f.upload->GetGPUVirtualAddress();
    } else {
        srcRes = dev->deferred.back().res.Get();
    }
    const u32 bpp = BytesPerPixel(dst->format);
    u32 rowBytes = dst->width * bpp;
    for (u32 y = 0; y < dst->height; ++y)
        memcpy((u8*)mem + (size_t)y * fp.Footprint.RowPitch, (const u8*)pixels + (size_t)y * srcRowPitch, rowBytes);
    fp.Offset = baseOffset;
    Transition(dst, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION d = {dst->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    D3D12_TEXTURE_COPY_LOCATION s = {};
    s.pResource = srcRes;
    s.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    s.PlacedFootprint = fp;
    if (!dst->sparse) {
        list->CopyTextureRegion(&d, 0, 0, 0, &s, nullptr);
        return;
    }
    // Sparse: only back tiles that contain non-zero pixels (or already hold data).
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
            D3D12_BOX box = {x0, y0, 0, x1, y1, 1};
            list->CopyTextureRegion(&d, x0, y0, 0, &s, &box);
        }
}


ReadbackPtr CommandContext::ReadbackTexture(Texture* src, const Rect* region) {
    Rect r = region ? region->Clipped(src->width, src->height) : Rect{0, 0, (int)src->width, (int)src->height};
    auto rb = std::make_shared<Readback>();
    if (r.Empty()) { rb->ready = true; return rb; }
    rb->width = r.Width();
    rb->height = r.Height();
    rb->format = src->format;
    rb->rowPitch = (rb->width * BytesPerPixel(src->format) + 255) & ~255u;
    auto hp = HeapProps(D3D12_HEAP_TYPE_READBACK);
    auto bd = BufferDesc((size_t)rb->rowPitch * rb->height);
    SX_CHECK(dev->device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb->buffer)));
    Transition(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION d = {};
    d.pResource = rb->buffer.Get();
    d.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    d.PlacedFootprint.Footprint = {ToDxgi(src->format), rb->width, rb->height, 1, rb->rowPitch};
    D3D12_TEXTURE_COPY_LOCATION s = {src->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    D3D12_BOX box = {(UINT)r.x0, (UINT)r.y0, 0, (UINT)r.x1, (UINT)r.y1, 1};
    list->CopyTextureRegion(&d, 0, 0, 0, &s, &box);
    dev->pendingReadbacks.push_back(rb);
    return rb;
}

}  // namespace sx::gpu
