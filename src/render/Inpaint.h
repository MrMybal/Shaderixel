#pragma once
// Content-aware fill without AI: multi-scale PatchMatch completion (Wexler /
// Barnes): every pixel of the hole is rebuilt from patches of the rest of the
// image that best continue its surroundings. CPU, multithreaded.

#include "core/Common.h"

#include <atomic>
#include <vector>

namespace sx {

// rgba: w*h pixels (4 floats each, any consistent color space); hole: w*h bytes, non-zero = to fill.
// The hole pixels of rgba are replaced. `cancel` (optional) stops early; `progress` receives 0..1.
bool Inpaint(std::vector<float>& rgba, int w, int h, const std::vector<u8>& hole, const std::atomic<bool>* cancel = nullptr,
             std::atomic<float>* progress = nullptr);

}  // namespace sx
