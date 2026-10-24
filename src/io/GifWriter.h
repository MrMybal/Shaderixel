#pragma once
// Animated GIF writer through WIC: RGBA8 frames, palette per frame (dithered), endless loop by default.
#include "core/Common.h"

#include <string>

namespace sx {

class GifWriter {
public:
    GifWriter() = default;
    GifWriter(const GifWriter&) = delete;
    GifWriter& operator=(const GifWriter&) = delete;
    ~GifWriter() { Close(); }
    // Frames wider than maxWidth are reduced (box filter), keeping the aspect.
    // loops: times the animation plays (0: endless).
    bool Open(const std::string& path, u32 fps, u32 maxWidth = 720, std::string* error = nullptr, u32 loops = 0);
    bool Write(const u8* rgba, u32 width, u32 height);
    // A frame shown for its own duration (seconds; frame animation).
    bool Write(const u8* rgba, u32 width, u32 height, double seconds);
    bool Close();
    bool IsOpen() const { return impl != nullptr; }

private:
    struct Impl;
    Impl* impl = nullptr;
};

}  // namespace sx
