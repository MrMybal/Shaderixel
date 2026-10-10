#pragma once
// Image decoding through the Windows Imaging Component: camera RAW files (DNG,
// and CR2 / CR3 / NEF / ARW / ... with the Microsoft Raw Image Extension), TIFF,
// HEIF / WebP when their codecs are installed. Output: linear float RGBA.
#include "core/Common.h"

#include <string>
#include <vector>

namespace sx {

bool IsRawExtension(const std::string& extLower);
bool LoadWicFloat(const std::string& path, std::vector<float>& rgba, int& w, int& h, std::string* error);
// 16-bit RGBA PNG (stb_image_write only does 8 bits).
bool SavePng16(const std::string& path, const std::vector<u16>& rgba, int w, int h, std::string* error = nullptr);

}  // namespace sx
