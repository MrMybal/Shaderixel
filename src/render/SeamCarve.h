#pragma once
// Content-aware resize (seam carving): the least important paths of pixels (low gradient) are removed or
// duplicated until the image has the requested size. Computed on the CPU from a reduced copy of the image
// (RGBA8); the result is a map of source coordinates applied to every layer on the GPU.

#include "core/Common.h"

#include <atomic>
#include <vector>

namespace sx {

struct SeamCarveInput {
    std::vector<u8> rgba;   // the image (w x h, RGBA8)
    std::vector<u8> mask;   // optional (w x h): > 127 selected
    u32 w = 0, h = 0;
    u32 newW = 0, newH = 0;
    int maskMode = 0;       // 0 none, 1 protect the selection, 2 remove it (seams go through it)
};

struct SeamCarveResult {
    // Horizontal pass: for each output pixel (newW x h), the source x (fractional: duplicated seams average
    // their neighbors). Vertical pass: for each output pixel (newW x newH), the source y in the horizontal result.
    std::vector<float> mapX, mapY;
    u32 w = 0, h = 0, newW = 0, newH = 0;
    std::string error;
};

// progress: 0..1 (may be read from another thread); cancel: stops early (result.error set).
SeamCarveResult SeamCarve(const SeamCarveInput& in, std::atomic<float>* progress = nullptr, const std::atomic<bool>* cancel = nullptr);

}  // namespace sx
