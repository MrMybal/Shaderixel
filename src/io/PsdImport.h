#pragma once
// Layered PSD import (ported from CyTexture's importer, standalone).
// Supports RGB/grayscale, 8/16/32-bit, raw/RLE/ZIP(+prediction) channels,
// groups (pass-through), blend modes, opacity, visibility, clipping, layer masks,
// common adjustment layers and solid color fills. Text, smart objects and
// styles come in as their rendered pixels (styles are reported as unsupported).

#include "doc/Document.h"

#include <memory>

namespace sx {

class Renderer;

struct PsdImportReport {
    int layers = 0, groups = 0, adjustments = 0, masks = 0;
    std::vector<std::string> warnings;
};

std::unique_ptr<Document> ImportPsd(const std::string& path, Renderer& renderer, gpu::CommandContext& ctx, PsdImportReport& report, std::string* error);

}  // namespace sx
