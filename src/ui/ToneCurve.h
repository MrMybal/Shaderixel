#pragma once
#include "doc/Effects.h"
#include <imgui.h>
#include <functional>
namespace sx::ui {
bool CurveEditor(const char* id, CurvePoints&, ImU32 color, float size,
                 bool& activated, bool& released, bool& cancelled, const float* histogram,
                 const std::function<float(const CurvePoints&,float)>& evaluate);
}
