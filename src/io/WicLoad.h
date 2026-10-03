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

}  // namespace sx
