#pragma once
// Small helpers shared by the renderer translation units.
#include "gpu/ShaderSystem.h"

#include <cmath>
#include <cstring>

namespace sx {

template <class T>
inline void SetConstants(gpu::DispatchDesc& d, const T& v) {
    static_assert(sizeof(T) % 4 == 0 && sizeof(T) <= gpu::kRootConstants * 4, "root constants too large");
    memcpy(d.constants, &v, sizeof(T));
    d.numConstants = sizeof(T) / 4;
}

// Builtin kernel, compiled synchronously the first time it is needed.
inline const gpu::ComputePipeline* Kernel(const char* name) {
    if (auto* p = gpu::Shaders().Builtin(name)) return p;
    return gpu::Shaders().CompileNow(name, gpu::Shaders().File(name))->pipeline.get();
}

// Sparse targets: back only the tiles under each dab (pad pixels around it) and
// mark the UAVs as already mapped. Brush-like kernels write zeros elsewhere in
// their dispatch rectangle, which unbacked tiles drop.
inline void MapDabs(gpu::DispatchDesc& d, u32 uavMask, const float4* dabs, size_t count, size_t stride, float pad) {
    for (u32 i = 0; i < gpu::kMaxUav; ++i) {
        if (!(uavMask & (1u << i)) || !d.uav[i] || !d.uav[i]->Sparse()) continue;
        for (size_t k = 0; k < count; ++k) {
            const float* p = (const float*)((const u8*)dabs + k * stride);  // x, y, radius
            gpu::Dev().MapRect(d.uav[i], {(int)std::floor(p[0] - p[2] - pad), (int)std::floor(p[1] - p[2] - pad),
                                          (int)std::ceil(p[0] + p[2] + pad), (int)std::ceil(p[1] + p[2] + pad)});
        }
        d.uavNoMap |= 1u << i;
    }
}

inline void Dispatch2D(gpu::CommandContext& ctx, gpu::DispatchDesc& d, u32 w, u32 h) {
    d.threadsX = w;
    d.threadsY = h;
    ctx.Dispatch(d);
}

}  // namespace sx
