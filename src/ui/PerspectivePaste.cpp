// Edit > Paste in perspective (like Photoshop's Vanishing Point paste): the active layer's content is laid on a
// plane drawn on the image (4 corners dragged onto a wall, a floor, a screen...), on a new layer, live; optionally
// repeated as tiles over the whole plane. Enter applies (one undo step), Escape cancels.
#include "ui/App.h"
#include "ui/Theme.h"

#include <cmath>

namespace sx {

namespace {
// Unit square -> quad (TL, TR, BR, BL), Heckbert; false when degenerate.
bool SquareToQuad(const float2 q[4], float m[9]) {
    const float x0 = q[0].x, y0 = q[0].y, x1 = q[1].x, y1 = q[1].y, x2 = q[2].x, y2 = q[2].y, x3 = q[3].x, y3 = q[3].y;
    const float sx = x0 - x1 + x2 - x3, sy = y0 - y1 + y2 - y3;
    float g = 0, h = 0;
    if (std::abs(sx) > 1e-6f || std::abs(sy) > 1e-6f) {
        const float dx1 = x1 - x2, dx2 = x3 - x2, dy1 = y1 - y2, dy2 = y3 - y2, den = dx1 * dy2 - dx2 * dy1;
        if (std::abs(den) < 1e-9f) return false;
        g = (sx * dy2 - dx2 * sy) / den;
        h = (dx1 * sy - sx * dy1) / den;
    }
    const float r[9] = {x1 - x0 + g * x1, x3 - x0 + h * x3, x0, y1 - y0 + g * y1, y3 - y0 + h * y3, y0, g, h, 1};
    for (int i = 0; i < 9; ++i) m[i] = r[i];
    return true;
}

bool Invert3(const float m[9], float out[9]) {
    const double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
    const double A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    const double det = a * A + b * B + c * C;
    if (std::abs(det) < 1e-12) return false;
    const double r[9] = {A / det, -(b * i - c * h) / det, (b * f - c * e) / det, B / det, (a * i - c * g) / det, -(a * f - c * d) / det,
                         C / det, -(a * h - b * g) / det, (a * e - b * d) / det};
    for (int k = 0; k < 9; ++k) out[k] = (float)r[k];
    return true;
}

bool Convex(const float2 q[4]) {
    int sign = 0;
    for (int k = 0; k < 4; ++k) {
        const float2 a = q[(k + 1) % 4] - q[k], b = q[(k + 2) % 4] - q[(k + 1) % 4];
        const float c = a.x * b.y - a.y * b.x;
        const int sg = c > 1e-3f ? 1 : c < -1e-3f ? -1 : 0;
        if (!sg || (sign && sg != sign)) return false;
        sign = sg;
    }
    return true;
}

bool InsideQuad(const float2 q[4], float2 p) {
    int sign = 0;
    for (int k = 0; k < 4; ++k) {
        const float2 a = q[(k + 1) % 4] - q[k], b = p - q[k];
        const float c = a.x * b.y - a.y * b.x;
        const int sg = c >= 0 ? 1 : -1;
        if (sign && sg != sign) return false;
        sign = sg;
    }
    return true;
}
}  // namespace

void App::StartPerspectivePaste(DocView& v) {
    Document& d = *v.doc;
    Layer* l = d.ActiveLayer();
    Surface* s = l && l->kind == LayerKind::Pixel ? d.GetSurface(l->surface) : nullptr;
    if (!s || d.Mode() != DocMode::Layers) { Status("Coller en perspective : sélectionnez un calque de pixels (l'image à poser)"); return; }
    if (xf.active && xf.view) TransformCommit(*xf.view);
    persp = {};
    persp.active = true;
    persp.view = &v;
    // The source: a copy of the layer's pixels, its content's bounds read back.
    persp.src = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "PerspSource");
    ctx->CopyTexture(persp.src.get(), s->tex.get());
    persp.boundsRb = v.renderer.ComputeBounds(*ctx, persp.src.get(), nullptr, false);
    const std::string name = l->name;
    // The new layer is an ordinary step (edits and undo meanwhile stay consistent); its pixels follow the plane.
    Edit e(d, v.history, "Coller en perspective");
    Layer& nl = d.AddPixelLayer("Perspective - " + name);
    persp.layer = nl.id;
    d.S().activeLayer = nl.id;
    d.SyncGraph();
    e.Commit();
    persp.histCursor = v.history.Cursor();
    persp.srcRect = {0, 0, (float)s->tex->Width(), (float)s->tex->Height()};
    Status("Coller en perspective : glissez les coins sur le plan (mur, sol, écran), Entrée pour appliquer, Échap pour annuler");
}

void App::FinishPerspectivePaste(bool keep) {
    if (!persp.active) return;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == persp.view;
    if (alive) {
        DocView& v = *persp.view;
        Document& d = *v.doc;
        if (keep) {
            d.dirty = true;
            Status("Coller en perspective : appliqué");
        } else if (d.FindLayer(persp.layer)) {
            if (v.history.Cursor() == persp.histCursor && v.history.Count() == persp.histCursor) {
                // Nothing done since: the step that added the layer is taken back entirely.
                if (xf.active && xf.view == &v) TransformCancel(v);
                v.history.Undo(d, *ctx);
                v.history.TruncateTo(v.history.Cursor());
            } else {
                Edit e(d, v.history, "Annuler le collage en perspective");
                d.RemoveLayer(persp.layer);
                e.Commit();
            }
            Status("Coller en perspective : annulé");
        }
    }
    persp = {};
}

void App::PerspectivePasteTick() {
    if (!persp.active) return;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == persp.view;
    if (!alive) { persp = {}; return; }
    DocView& v = *persp.view;
    Document& d = *v.doc;
    if (persp.boundsRb) {
        if (!persp.boundsRb->ready) return;
        const u32* b = (const u32*)persp.boundsRb->data.data();
        if (persp.boundsRb->data.size() >= 16 && b[2] > b[0] && b[3] > b[1]) persp.srcRect = {(float)b[0], (float)b[1], (float)(b[2] - b[0]), (float)(b[3] - b[1])};
        persp.boundsRb.reset();
        // First plane: centered, half the document, the source's proportions.
        const float W = (float)d.Width(), H = (float)d.Height();
        float w = W * 0.5f, h = w * persp.srcRect.w / std::max(persp.srcRect.z, 1.0f);
        if (h > H * 0.6f) { h = H * 0.6f; w = h * persp.srcRect.z / std::max(persp.srcRect.w, 1.0f); }
        const float x0 = (W - w) * 0.5f, y0 = (H - h) * 0.5f;
        persp.quad[0] = {x0, y0};
        persp.quad[1] = {x0 + w, y0};
        persp.quad[2] = {x0 + w, y0 + h};
        persp.quad[3] = {x0, y0 + h};
        persp.dirty = true;
    }
    // Options window.
    ImGui::SetNextWindowSize(ImVec2(300 * dpi, 0), ImGuiCond_Always);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 70 * dpi), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0));
    if (ImGui::Begin("Coller en perspective", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("Glissez les coins du plan sur les bords d'un mur, d'un sol, d'un écran ; glissez l'intérieur pour le déplacer.");
        persp.dirty |= ImGui::Checkbox("Mosaïque (répéter sur le plan)", &persp.tile);
        if (persp.tile) persp.dirty |= ImGui::SliderFloat("Taille d'un motif", &persp.tileSize, 0.02f, 1.0f, "%.2f");
        persp.dirty |= ImGui::SliderFloat("Opacité", &persp.opacity, 0, 1, "%.2f");
        if (ImGui::Button("Appliquer (Entrée)")) {
            ImGui::End();
            FinishPerspectivePaste(true);
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Annuler (Échap)")) {
            ImGui::End();
            FinishPerspectivePaste(false);
            return;
        }
    }
    ImGui::End();
    Layer* l = d.FindLayer(persp.layer);
    if (!l) { persp = {}; return; }  // (undone or deleted meanwhile: the session ends)
    if (Surface* cur = d.GetSurface(l->surface); cur && persp.warpedVersion && cur->version != persp.warpedVersion) {
        // The layer was painted / changed by something else: the plane is fixed as it is (no rewarp over the edit).
        persp = {};
        Status("Coller en perspective : appliqué (le calque a été modifié)");
        return;
    }
    if (!persp.dirty) return;
    Prepare(v);  // (the new layer's surface allocated)
    Surface* s = d.GetSurface(l->surface);
    float m[9], inv[9];
    if (!s || !Convex(persp.quad) || !SquareToQuad(persp.quad, m) || !Invert3(m, inv)) return;
    float2 tiles = {0, 0};
    if (persp.tile) {
        // One motif covers tileSize of the plane's width, its height following the source's proportions.
        const float pw = 0.5f * (Length(persp.quad[1] - persp.quad[0]) + Length(persp.quad[2] - persp.quad[3]));
        const float ph = 0.5f * (Length(persp.quad[3] - persp.quad[0]) + Length(persp.quad[2] - persp.quad[1]));
        const float tw = persp.tileSize * pw, th = tw * persp.srcRect.w / std::max(persp.srcRect.z, 1.0f);
        tiles = {1.0f / persp.tileSize, ph / std::max(th, 1e-3f)};
    }
    v.renderer.WarpPerspective(*ctx, persp.src.get(), s->tex.get(), inv, persp.srcRect, tiles, persp.opacity);
    d.TouchSurface(l->surface);
    persp.warpedVersion = s->version;
    persp.dirty = false;
}

bool App::PerspectivePasteInput(DocView& v, float2 dp, bool hovered) {
    if (!persp.active || persp.view != &v) return false;
    if (persp.boundsRb) return true;  // (the plane appears once the source's bounds are known)
    if (!ImGui::GetIO().WantTextInput) {  // (Enter in a text field belongs to it)
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) { FinishPerspectivePaste(true); return true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { FinishPerspectivePaste(false); return true; }
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const float grab = 10.0f / std::max(v.zoom, 1e-3f);
        persp.drag = -1;
        for (int k = 0; k < 4; ++k)
            if (Length(persp.quad[k] - dp) < grab) persp.drag = k;
        if (persp.drag < 0 && InsideQuad(persp.quad, dp)) persp.drag = 4;
        persp.dragFrom = dp;
    }
    if (persp.drag >= 0) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            persp.drag = -1;
        } else if (persp.drag == 4) {
            const float2 dd = dp - persp.dragFrom;
            for (auto& q : persp.quad) q = q + dd;
            persp.dragFrom = dp;
            persp.dirty = true;
        } else {
            const float2 keep = persp.quad[persp.drag];
            persp.quad[persp.drag] = dp;
            if (!Convex(persp.quad)) persp.quad[persp.drag] = keep;  // (the plane never folds)
            persp.dirty = true;
        }
    }
    if (hovered) ImGui::SetMouseCursor(persp.drag == 4 ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Arrow);
    return true;
}

void App::PerspectivePasteOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    if (!persp.active || persp.view != &v || persp.boundsRb) return;
    auto S = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    float m[9];
    if (!SquareToQuad(persp.quad, m)) return;
    // The plane's grid (perspective-correct lines through the homography).
    auto P = [&](float u, float w) {
        const float d = m[6] * u + m[7] * w + 1;
        return S({(m[0] * u + m[1] * w + m[2]) / d, (m[3] * u + m[4] * w + m[5]) / d});
    };
    for (int i = 1; i < 8; ++i) {
        const float t = i / 8.0f;
        dl->AddLine(P(t, 0), P(t, 1), IM_COL32(120, 200, 255, 90));
        dl->AddLine(P(0, t), P(1, t), IM_COL32(120, 200, 255, 90));
    }
    ImVec2 q[4];
    for (int k = 0; k < 4; ++k) q[k] = S(persp.quad[k]);
    dl->AddPolyline(q, 4, IM_COL32(120, 200, 255, 255), ImDrawFlags_Closed, 2.0f);
    for (int k = 0; k < 4; ++k) {
        dl->AddRectFilled(ImVec2(q[k].x - 5, q[k].y - 5), ImVec2(q[k].x + 5, q[k].y + 5), IM_COL32_WHITE);
        dl->AddRect(ImVec2(q[k].x - 5, q[k].y - 5), ImVec2(q[k].x + 5, q[k].y + 5), IM_COL32(0, 0, 0, 200));
    }
}

}  // namespace sx
