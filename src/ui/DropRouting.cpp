// Drag and drop by target window: models and .sx3d projects onto "Rendu" go into the render
// scene; images onto the graph / canvas / layers go into the active document (layers: an
// embedded smart object at its own size, transformable without loss; nodes: an image node
// followed by a Transformation node). Anything else opens as before.
#include "ui/App.h"
#include "io/UAsset.h"

#include <stb_image.h>
#include <imgui_internal.h>

namespace sx {

namespace {

bool Over(const char* name, ImVec2 p) {
    ImGuiWindow* w = ImGui::FindWindowByName(name);
    if (!w || w->Hidden || !(w->Active || w->WasActive)) return false;
    return p.x >= w->Pos.x && p.y >= w->Pos.y && p.x < w->Pos.x + w->Size.x && p.y < w->Pos.y + w->Size.y;
}

std::string Ext(const std::string& path) {
    std::string e = Narrow(Utf8Path(path).extension().wstring());
    for (auto& c : e) c = (char)tolower((unsigned char)c);
    return e;
}

u16 ToHalf(float x) {
    u32 b;
    memcpy(&b, &x, 4);
    int e = (int)((b >> 23) & 255) - 127 + 15;
    return x <= 0 || e <= 0 ? 0 : e >= 31 ? 0x7BFF : (u16)((e << 10) | ((b >> 13) & 0x3FF));
}

}  // namespace

bool App::RouteDrop(const DropRequest& r) {
    DocView* v = Active();
    const std::string ext = Ext(r.path);
    const ImVec2 p(r.x, r.y);
    UAssetInfo ua;
    const bool uasset = ext == ".uasset" && ReadUAssetInfo(r.path, ua);
    const bool model = ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".sx3d" || (uasset && ua.mesh);
    const bool image = ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".gif" || ext == ".hdr" ||
                       ext == ".psd" || ext == ".tif" || ext == ".tiff" || ext == ".webp" || (uasset && ua.texture);
    if (model && v && v->doc->kind == 1 && v->mesh && Over("Rendu", p)) return ImportIntoRender(*v, r.path);
    // A Shaderixel document onto an image document's graph / canvas / layers: a SubGraph (linked, follows the file).
    if ((ext == ".sxl" || ext == ".sx3d") && v && v->doc->Mode() == DocMode::Layers &&
        (Over("Graphe", p) || Over("Canevas", p) || Over("Calques", p) || Over("Aperçu du calque", p))) {
        PlaceSmartFile(*v, r.path, true, true);
        return true;
    }
    if (image && v && (Over("Graphe", p) || Over("Canevas", p) || Over("Calques", p) || Over("Aperçu du calque", p))) {
        if (v->doc->Mode() == DocMode::Nodes) AddImageNode(*v, r.path);
        else PlaceSmartFile(*v, r.path, false, true);
        return true;
    }
    return false;
}

void App::AddImageNode(DocView& v, const std::string& path) {
    Document& d = *v.doc;
    int iw = 0, ih = 0;
    stbi_uc* px = LoadImagePixels(path, iw, ih);
    if (!px) { Status("Image illisible : " + path); return; }
    // Left of the existing nodes.
    float2 pos = {40, 40};
    bool any = false;
    for (auto& n : d.S().graph.nodes) {
        pos = any ? float2{std::min(pos.x, n.pos.x), std::min(pos.y, n.pos.y)} : n.pos;
        any = true;
    }
    if (any) pos.x -= 560;
    Edit e(d, v.history, "Image (nœud)");
    Node& surfaceNode = d.AddNode(NodeKind::Surface, pos);
    surfaceNode.name = Narrow(Utf8Path(path).stem().wstring());
    const Id sid = surfaceNode.id, surface = surfaceNode.surface;
    Node& xf = d.AddEffectNode("transform_2d", "", {pos.x + 260, pos.y});
    xf.name = "Transformation";
    const Id xid = xf.id;
    d.Link(sid, xid, 0);
    // Composited over the current result: a Fusion node in front of the output (visible right away).
    Id outId = 0, prev = 0;
    u8 prevCh = 0;
    float2 outPos = {pos.x + 780, pos.y};
    for (auto& n : d.S().graph.nodes)
        if (n.kind == NodeKind::Output) {
            outId = n.id;
            prev = n.inputs.empty() ? 0 : n.inputs[0];
            prevCh = InputChannel(n, 0);
            outPos = n.pos;
        }
    if (outId) {
        Node& b = d.AddNode(NodeKind::Blend, {outPos.x - 260, outPos.y + 220});
        b.name = "Fusion : " + Narrow(Utf8Path(path).stem().wstring());
        const Id bid = b.id;
        if (prev) d.Link(prev, bid, 0, prevCh);
        d.Link(xid, bid, 1);
        d.Link(bid, outId, 0);
    }
    d.S().activeNode = xid;
    e.Commit();
    // Pixels at their own size, centered (premultiplied linear).
    if (Surface* sf = d.GetSurface(surface)) {
        v.renderer.Prepare(d, *ctx);
        const int W = (int)d.Width(), H = (int)d.Height();
        const int x0 = (W - iw) / 2, y0 = (H - ih) / 2;
        std::vector<float> buf((size_t)W * H * 4, 0.0f);
        for (int y = 0; y < ih; ++y) {
            const int dy = y0 + y;
            if (dy < 0 || dy >= H) continue;
            for (int x = 0; x < iw; ++x) {
                const int dx = x0 + x;
                if (dx < 0 || dx >= W) continue;
                const stbi_uc* s = &px[((size_t)y * iw + x) * 4];
                float a = s[3] / 255.0f;
                float* o = &buf[((size_t)dy * W + dx) * 4];
                for (int c = 0; c < 3; ++c) o[c] = SrgbToLinear(s[c] / 255.0f) * a;
                o[3] = a;
            }
        }
        if (sf->tex->GetFormat() == gpu::Format::RGBA32F) {
            ctx->Upload(sf->tex.get(), buf.data(), W * 16);
        } else {
            std::vector<u16> h(buf.size());
            for (size_t i = 0; i < buf.size(); ++i) h[i] = ToHalf(buf[i]);
            ctx->Upload(sf->tex.get(), h.data(), W * 8);
        }
        d.CancelPendingClear(surface);
        d.TouchSurface(surface);
    }
    stbi_image_free(px);
    Status("Image ajoutée au graphe : " + Narrow(Utf8Path(path).filename().wstring()) + " (" + std::to_string(iw) + " x " + std::to_string(ih) +
           "), suivie d'un nœud Transformation (position, échelle, rotation)");
}

}  // namespace sx
