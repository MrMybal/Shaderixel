#pragma once
// MP4 (H.264) writer through Media Foundation: frames given as RGBA8 top-down.
#include "core/Common.h"

#include <string>

namespace sx {

class VideoWriter {
public:
    VideoWriter() = default;
    VideoWriter(const VideoWriter&) = delete;
    VideoWriter& operator=(const VideoWriter&) = delete;
    ~VideoWriter() { Close(); }
    // Width and height are rounded down to even values (H.264); frames are cropped to them.
    bool Open(const std::string& path, u32 width, u32 height, u32 fps, std::string* error = nullptr);
    bool Write(const u8* rgba, u32 width, u32 height);  // source size (cropped / padded to the video)
    bool Close();
    bool IsOpen() const { return impl != nullptr; }

private:
    struct Impl;
    Impl* impl = nullptr;
};

}  // namespace sx
