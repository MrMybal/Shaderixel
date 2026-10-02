#pragma once
// Layered PSD export (8-bit RGB, RLE). Every layer is rasterized: pixel/text/
// shape layers with their filters, fill layers, and adjustment layers as the
// adjusted image (with their blend mode, opacity and mask the PSD composite
// matches ours). Groups become layer folders (pass-through kept), with masks,
// clipping, visibility and blend modes. Layers are cropped to their content.
//
// Usage: StartPsdExport() records GPU readbacks; once PsdExportReady() is true,
// WritePsd() (safe on a worker thread) encodes and writes the file.

#include "doc/Document.h"

namespace sx {

class Renderer;

struct PsdExportLayer {
    enum Kind : u8 { Pixels, GroupBegin, GroupEnd } kind = Pixels;
    std::string name;
    BlendMode blend = BlendMode::Normal;
    bool passThrough = false;
    u8 opacity = 255;
    bool visible = true, clipped = false, maskDisabled = false;
    BlendIf blendIf;
    gpu::ReadbackPtr pixels;  // RGBA8 straight sRGB, document size
    gpu::ReadbackPtr mask;    // RGBA8, R = mask value
};

struct PsdExportJob {
    std::string path;
    int depth = 8;     // 8 or 16 bits per channel
    bool psb = false;  // large document format (also chosen automatically above 30000 px)
    u32 width = 0, height = 0;
    std::vector<PsdExportLayer> layers;  // bottom to top, groups bracketed
    gpu::ReadbackPtr composite;
};

// Records readbacks for every layer of `doc` (layer mode). Returns false if nothing to export.
bool StartPsdExport(Document& doc, Renderer& r, gpu::CommandContext& ctx, PsdExportJob& job);
bool PsdExportReady(const PsdExportJob& job);
bool WritePsd(const PsdExportJob& job, std::string* error);

}  // namespace sx
