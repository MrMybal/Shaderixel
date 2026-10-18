#pragma once
// Custom shapes: Bezier outlines normalized in the unit square, drawn in the shape's frame.
// Built-in shapes are written as SVG path data; user shapes come from a path or an SVG file.

#include "doc/VectorPath.h"

#include <string>
#include <vector>

namespace sx {

struct CustomShape {
    std::string name;
    std::vector<SubPath> path;  // normalized: fits [0, 1] x [0, 1], centered, proportions kept
    bool user = false;
};

// SVG path data ("M 10 20 C ... Z": M L H V C S Q T Z, relative forms too; arcs as straight lines).
std::vector<SubPath> ParseSvgPath(const std::string& d);
// Every <path d="..."> of an SVG document, in one outline.
std::vector<SubPath> ParseSvgFile(const std::string& svg);
// Scaled into the unit square (uniform scale, centered).
void NormalizeShapePath(std::vector<SubPath>& path);
// The shapes that come with Shaderixel.
const std::vector<CustomShape>& BuiltinShapes();

// Signed distance (pixels, negative inside, even-odd fill) of the shape drawn in a box of the given size,
// sampled on a grid that covers the box plus `margin` pixels on each side, at most `maxSide` texels long.
// out: tw x th values; scale: texels per pixel.
void ShapeDistanceField(const std::vector<SubPath>& path, float boxW, float boxH, float margin, int maxSide, std::vector<float>& out, int& tw,
                        int& th, float& scale);

}  // namespace sx
