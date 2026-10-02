#pragma once
// Small helpers shared by the renderer translation units.
#include "gpu/ShaderSystem.h"

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

inline void Dispatch2D(gpu::CommandContext& ctx, gpu::DispatchDesc& d, u32 w, u32 h) {
    d.threadsX = w;
    d.threadsY = h;
    ctx.Dispatch(d);
}

}  // namespace sx
