#pragma once
#include "core/Common.h"
#include <functional>
#include <optional>

namespace sx::ui {
// size is signed document pixels; percentages use an explicit, stable reference.
std::optional<float> ParseDecimal(std::string text);
std::optional<float2> ResizeDimension(float2 size, float2 reference, bool pixels,
                                      int axis, float value, bool lockAspect);
bool DecimalField(const char* label, float& value);
bool Dimensions(const char* id, float2& size, float2 reference, bool& pixels,
                bool& lockAspect, const char* percentHint,
                const std::function<void(float2)>& apply = {},
                const std::function<void(bool)>& trackUndo = {});
}
