#pragma once
// Color management through the Windows color system (ICM, mscms): ICC profile
// list, transforms between profiles (RGB, CMYK, soft proofing, gamut check),
// 3D LUTs for the display and conversions, Lab math, ICC extraction from files.
#include "core/Common.h"

#include <string>
#include <vector>

namespace sx::color {

struct ProfileInfo {
    std::string path, name;
    bool rgb = false, cmyk = false, gray = false;
};

enum Intent { Perceptual = 0, Relative = 1, Saturation = 2, Absolute = 3 };

const std::vector<ProfileInfo>& Profiles(bool rescan = false);
std::string SrgbPath();
std::string DefaultCmykPath();                  // RSWOP / first CMYK profile ("" if none)
std::string MonitorPath(void* hwnd);            // display profile ("" = sRGB)
std::string ProfileName(const std::string& path);  // "" -> "sRGB IEC61966-2.1"
std::string UserProfileDir();                   // <exe>/profiles (embedded profiles kept here)

// RGB samples (0..1, encoded) through a chain of profiles ending in RGB (e.g. doc -> CMYK -> monitor).
// gamut (optional, one byte per sample, 0 = in gamut) checks the chain's first two profiles.
bool TransformRgb(const std::vector<std::string>& chain, int intent, const float* rgb, size_t count, float* out,
                  std::vector<u8>* gamut = nullptr);
// 8-bit RGB (3 bytes / px) -> CMYK (4 bytes / px, 0 = no ink) and back.
bool RgbToCmyk(const std::string& rgbProfile, const std::string& cmykProfile, int intent, const u8* rgb, size_t count, u8* cmyk);
bool CmykToRgb(const std::string& cmykProfile, const std::string& rgbProfile, int intent, const u8* cmyk, size_t count, u8* rgb);

// n^3 grid (red fastest) through the chain, out = rgb + alpha (1 in gamut, 0 out).
bool BuildLut(const std::vector<std::string>& chain, int intent, int n, bool gamutCheck, std::vector<float>& rgba);

// Lab (D50, Bradford adapted from sRGB D65) <-> sRGB encoded 0..1.
float4 SrgbToLab(float4 srgb);  // xyz used
float4 LabToSrgb(float4 lab);

// Embedded ICC profile of a PNG (iCCP) or JPEG (APP2) file, empty if none.
std::vector<u8> ReadEmbeddedIcc(const std::string& path);
// Saves an embedded profile under UserProfileDir (by content hash) and returns its path ("" if it is sRGB).
std::string StoreEmbeddedIcc(const std::vector<u8>& icc);

// Uncompressed CMYK TIFF (8 bits, ICC profile embedded when given).
bool WriteCmykTiff(const std::string& path, const u8* cmyk, u32 w, u32 h, const std::string& iccPath);

}  // namespace sx::color
