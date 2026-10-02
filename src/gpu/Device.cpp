#include "gpu/Device.h"

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

DXGI_FORMAT ToDxgi(Format f) {
    switch (f) {
        case Format::RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case Format::RGBA32F: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case Format::R8: return DXGI_FORMAT_R8_UNORM;
        case Format::R16F: return DXGI_FORMAT_R16_FLOAT;
        case Format::R32F: return DXGI_FORMAT_R32_FLOAT;
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
    }
    return 4;
}

Texture::~Texture() { if (res) g_device.DeferRelease(*this); }

// ------------------------------------------------------------------ init

bool Device::Init(HWND hwnd) {
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
#endif
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
    D3D12_ROOT_SIGNATURE_DESC rsd = {4, params, 3, samplers, D3D12_ROOT_SIGNATURE_FLAG_NONE};
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
    ctx.list->Reset(f.alloc.Get(), nullptr);
    f.uploadOffset = 0;
    f.ringOffset = 0;
    ID3D12DescriptorHeap* heaps[] = {gpuHeap.Get()};
    ctx.list->SetDescriptorHeaps(1, heaps);
    ctx.list->SetComputeRootSignature(rootSig.Get());
    uiTexturesThisFrame.clear();
    return ctx;
}

void Device::EndFrame(const std::function<void(ID3D12GraphicsCommandList*)>& drawUi) {
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
    drawUi(list);

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
    ID3D12CommandList* lists[] = {list};
    queue->ExecuteCommandLists(1, lists);

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

TexturePtr Device::CreateTexture(u32 w, u32 h, Format fmt, const char* debugName) {
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
    d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    auto hp = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&t->res)))) {
        Log("CreateTexture %ux%u failed (out of video memory?)", w, h);
        return nullptr;
    }
    if (debugName) t->res->SetName(Widen(debugName).c_str());
    t->state = D3D12_RESOURCE_STATE_COMMON;
    t->srvCpu = cpuSlots.Alloc();
    t->uavCpu = cpuSlots.Alloc();
    D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = d.Format;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(t->res.Get(), &sd, CpuHandle(t->srvCpu));
    D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {};
    ud.Format = d.Format;
    ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(t->res.Get(), nullptr, &ud, CpuHandle(t->uavCpu));
    return t;
}

void Device::DeferRelease(Texture& t) {
    deferred.push_back({frameNumber, std::move(t.res), t.srvCpu, t.uavCpu, t.uiSlot});
    t.srvCpu = t.uavCpu = t.uiSlot = -1;
    // Drop from this frame's UI list if present.
    std::erase(uiTexturesThisFrame, &t);
}

void Device::ProcessDeferred(bool all) {
    while (!deferred.empty() && (all || deferred.front().frame + kFramesInFlight + 1 <= frameNumber)) {
        auto& d = deferred.front();
        cpuSlots.Free(d.srvCpu);
        cpuSlots.Free(d.uavCpu);
        persistentSlots.Free(d.uiSlot);
        deferred.pop_front();
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

// ------------------------------------------------------------------ command context

void CommandContext::Transition(Texture* t, D3D12_RESOURCE_STATES s) {
    if (!t || !t->res) return;
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
    for (u32 i = 0; i < kMaxUav; ++i) if (d.uav[i]) Transition(d.uav[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

void CommandContext::CopyTexture(Texture* dst, Texture* src) {
    Transition(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    Transition(dst, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(dst->res.Get(), src->res.Get());
}

void CommandContext::CopyRegion(Texture* dst, int dx, int dy, Texture* src, const Rect& r) {
    if (r.Empty()) return;
    Transition(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    Transition(dst, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION d = {dst->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    D3D12_TEXTURE_COPY_LOCATION s = {src->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    D3D12_BOX box = {(UINT)r.x0, (UINT)r.y0, 0, (UINT)r.x1, (UINT)r.y1, 1};
    list->CopyTextureRegion(&d, (UINT)dx, (UINT)dy, 0, &s, &box);
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
    u32 rowBytes = dst->width * BytesPerPixel(dst->format);
    for (u32 y = 0; y < dst->height; ++y)
        memcpy((u8*)mem + (size_t)y * fp.Footprint.RowPitch, (const u8*)pixels + (size_t)y * srcRowPitch, rowBytes);
    fp.Offset = baseOffset;
    Transition(dst, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION d = {dst->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    D3D12_TEXTURE_COPY_LOCATION s = {};
    s.pResource = srcRes;
    s.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    s.PlacedFootprint = fp;
    list->CopyTextureRegion(&d, 0, 0, 0, &s, nullptr);
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
