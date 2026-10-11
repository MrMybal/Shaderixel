#pragma once
// Camera RAW decoding with LibRaw (https://www.libraw.org, LGPL 2.1 / CDDL 1.0): every format LibRaw
// knows (Canon CR2 / CR3 / CRW, Nikon NEF / NRW, Sony ARW, Fujifilm RAF, Olympus ORF, Panasonic RW2,
// Pentax PEF, DNG, Leica, Hasselblad, Phase One, Sigma X3F...). Demosaic, camera white balance, linear
// sRGB float output (no tone curve: the Camera Raw adjustments do the development). Built only when
// external/LibRaw is present (SX_LIBRAW), the WIC loader being the fallback.
#include "core/Common.h"

#include <string>
#include <vector>

namespace sx {

// Shot metadata (EXIF / maker notes), used to find the lens profile.
struct RawInfo {
    std::string make, model;          // camera (normalized by LibRaw)
    std::string lensMake, lens;       // lens model
    float focal = 0, aperture = 0;    // mm, f-number
    float focal35 = 0;                // 35 mm equivalent (0 unknown)
    float iso = 0, shutter = 0;       // ISO, seconds
    float cropFactor = 0;             // focal35 / focal when both are known
    int width = 0, height = 0;
    std::string Summary() const;      // "Canon EOS 5D, EF 24-70mm, 35 mm f/2.8, 1/125 s, ISO 100"
};

bool HaveLibRaw();
// Linear sRGB float RGBA. halfSize: 2x2 binned (fast preview).
bool LoadRawFloat(const std::string& path, std::vector<float>& rgba, int& w, int& h, RawInfo* info, std::string* error,
                  bool halfSize = false);
// Metadata only (no decoding). Works for any file LibRaw opens; false otherwise.
bool ReadRawInfo(const std::string& path, RawInfo& info);
// Embedded preview (JPEG or bitmap written by the camera), turned upright: fast thumbnails. RGBA8.
bool LoadRawThumb(const std::string& path, std::vector<u8>& rgba, int& w, int& h, RawInfo* info = nullptr);
// Any photo file stb does not read: camera RAW through LibRaw (when built with it), else the Windows
// Imaging Component (TIFF, HEIF, WebP, JPEG XR, RAW with the Microsoft codecs). Linear float RGBA.
bool LoadPhotoFloat(const std::string& path, std::vector<float>& rgba, int& w, int& h, RawInfo* info, std::string* error);

}  // namespace sx
