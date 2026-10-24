// Retouching beyond the brushes: content-aware fill (PatchMatch, no AI), Patch
// tool, content-aware move, red eye, magic eraser, history brush snapshots.
#include "ui/App.h"
#include "render/Inpaint.h"

#include <cstring>

namespace sx {

// ------------------------------------------------------------------ content-aware fill

void App::StartContentAwareFill(DocView& v, std::function<void()> then) {
    Document& d = *v.doc;
    if (fillJob.active) { Status("Remplissage d'après le contenu déjà en cours..."); return; }
    Id target = d.PaintTarget();
    Surface* s = d.GetSurface(target);
    Surface* sel = d.Selection();
    if (!s || !sel) { Status("Remplissage d'après le contenu : sélectionnez la zone à remplacer sur un calque de pixels"); return; }
    fillJob = {};
    fillJob.active = true;
    fillJob.view = &v;
    fillJob.surface = target;
    fillJob.selection = d.S().selection;
    fillJob.pixels = ctx->ReadbackTexture(s->tex.get());
    fillJob.mask = ctx->ReadbackTexture(sel->tex.get());
    fillJob.then = std::move(then);
    Status("Remplissage d'après le contenu : analyse...");
}

void App::StartContentAwareFillMask(DocView& v, std::vector<u8> mask, const std::string& label) {
    Document& d = *v.doc;
    if (fillJob.active) { Status("Correction déjà en cours..."); return; }
    Id target = d.PaintTarget();
    Surface* s = d.GetSurface(target);
    if (!s || mask.size() != (size_t)s->tex->Width() * s->tex->Height()) { Status("Correcteur localisé : sélectionnez un calque de pixels"); return; }
    fillJob = {};
    fillJob.active = true;
    fillJob.view = &v;
    fillJob.surface = target;
    fillJob.pixels = ctx->ReadbackTexture(s->tex.get());
    fillJob.maskCpu = std::move(mask);
    fillJob.label = label;
    Status(label + " : analyse...");
}

void App::SpotHealInput(DocView& v, float2 dp, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    if (fillJob.active) {  // (one correction at a time: a stroke started meanwhile is dropped)
        spotHeal.painting = false;
        return;
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        spotHeal.dabs.clear();
        spotHeal.painting = true;
        spotHeal.view = &v;
    }
    if (!spotHeal.painting || spotHeal.view != &v) return;
    const float r = std::max(spotHeal.size * 0.5f, 1.0f);
    if (spotHeal.dabs.empty()) {
        spotHeal.dabs.push_back({dp.x, dp.y, r, 0});
    } else {
        // Dabs every quarter of the radius from the last one (a fast stroke leaves no gaps).
        const float2 last = {spotHeal.dabs.back().x, spotHeal.dabs.back().y};
        const float len = Length(dp - last), step = std::max(r * 0.25f, 0.5f);
        const int n = std::min(2048, (int)std::floor(len / step));
        for (int i = 1; i <= n; ++i) {
            const float2 q = last + (dp - last) * (i * step / len);
            spotHeal.dabs.push_back({q.x, q.y, r, 0});
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        spotHeal.painting = false;
        SpotHealApply(v);
    }
    (void)io;
}

void App::SpotHealApply(DocView& v) {
    {
        // The painted area (a little wider: the defect's edge too) becomes the hole to rebuild.
        Surface* s = v.doc->GetSurface(v.doc->PaintTarget());
        if (!s || spotHeal.dabs.empty()) return;
        const int W = (int)s->tex->Width(), H = (int)s->tex->Height();
        std::vector<u8> mask((size_t)W * H, 0);
        for (const float4& dab : spotHeal.dabs) {
            const float rr = dab.z + 2;
            const int x0 = std::max(0, (int)std::floor(dab.x - rr)), x1 = std::min(W - 1, (int)std::ceil(dab.x + rr));
            const int y0 = std::max(0, (int)std::floor(dab.y - rr)), y1 = std::min(H - 1, (int)std::ceil(dab.y + rr));
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    const float dx = x + 0.5f - dab.x, dy = y + 0.5f - dab.y;
                    if (dx * dx + dy * dy <= rr * rr) mask[(size_t)y * W + x] = 255;
                }
        }
        StartContentAwareFillMask(v, std::move(mask), "Correcteur localisé");
    }
}

void App::SpotHealOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    if (tool != Tool::SpotHeal) {
        // Another tool: a stroke in progress is dropped (never applied later).
        if (spotHeal.painting) {
            spotHeal.painting = false;
            spotHeal.dabs.clear();
        }
        return;
    }
    auto S = [&](float x, float y) { return ImVec2(canvasMin.x + v.pan.x + x * v.zoom, canvasMin.y + v.pan.y + y * v.zoom); };
    // The stroke being painted (or being rebuilt), then the brush outline.
    const bool show = spotHeal.painting || (fillJob.active && fillJob.view == &v && !fillJob.maskCpu.empty());
    if (show)
        for (const float4& d : spotHeal.dabs) dl->AddCircleFilled(S(d.x, d.y), d.z * v.zoom, IM_COL32(255, 60, 60, 70));
    const ImVec2 m = ImGui::GetIO().MousePos;
    const float r = std::max(spotHeal.size * 0.5f * v.zoom, 1.0f);
    dl->AddCircle(m, r + 1, IM_COL32(0, 0, 0, 160), 0, 1.5f);
    dl->AddCircle(m, r, IM_COL32(255, 255, 255, 220), 0, 1.0f);
}

void App::UpdateContentAwareFill() {
    auto& J = fillJob;
    if (!J.active) return;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == J.view;
    if (!alive) { J.cancel = true; if (J.work.valid()) J.work.wait(); J = {}; return; }
    if (J.pixels) {
        if (!J.pixels->ready || (J.mask && !J.mask->ready)) return;
        // Crop around the hole (with room for sources), then PatchMatch on a worker.
        const int W = (int)J.pixels->width, H = (int)J.pixels->height;
        if (!J.mask && J.maskCpu.size() != (size_t)W * H) { J = {}; return; }
        const u8* m = J.mask ? J.mask->data.data() : J.maskCpu.data();
        int x0 = W, y0 = H, x1 = -1, y1 = -1;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (m[(size_t)y * W + x] > 4) {
                    x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
                }
        if (x1 < 0) { Status(J.mask ? "Sélection vide" : "Rien à corriger"); J = {}; return; }
        int margin = std::max(48, (int)(std::max(x1 - x0, y1 - y0) * 0.9f));
        // Bounded work: at most ~3 Mpx around the hole.
        while ((long long)(x1 - x0 + 2 * margin) * (y1 - y0 + 2 * margin) > 3000000 && margin > 48) margin = margin * 3 / 4;
        J.cx = std::max(0, x0 - margin);
        J.cy = std::max(0, y0 - margin);
        J.cw = std::min(W, x1 + margin + 1) - J.cx;
        J.ch = std::min(H, y1 + margin + 1) - J.cy;
        J.rgba.assign((size_t)J.cw * J.ch * 4, 0.0f);
        J.hole.assign((size_t)J.cw * J.ch, 0);
        const bool half = J.pixels->format == gpu::Format::RGBA16F;
        for (int y = 0; y < J.ch; ++y)
            for (int x = 0; x < J.cw; ++x) {
                size_t src = (size_t)(y + J.cy) * W + (x + J.cx);
                size_t dst = (size_t)y * J.cw + x;
                if (half) {
                    const u16* h = (const u16*)J.pixels->data.data() + src * 4;
                    for (int k = 0; k < 4; ++k) J.rgba[dst * 4 + k] = HalfToFloat(h[k]);
                } else {
                    memcpy(&J.rgba[dst * 4], (const float*)J.pixels->data.data() + src * 4, 16);
                }
                J.hole[dst] = m[src] > 4 ? 1 : 0;
            }
        J.pixels.reset();
        J.mask.reset();
        J.work = std::async(std::launch::async, [&J] { return Inpaint(J.rgba, J.cw, J.ch, J.hole, &J.cancel, &J.progress); });
        return;
    }
    if (J.work.valid()) {
        if (J.work.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            char b[64];
            snprintf(b, sizeof(b), " : %.0f %%", J.progress.load() * 100.0f);
            Status(J.label + b);
            return;
        }
        bool ok = J.work.get();
        DocView& v = *J.view;
        Document& d = *v.doc;
        Surface* sel = d.GetSurface(J.selection);
        gpu::TexturePtr ownMask;
        if (!J.maskCpu.empty()) {
            // The caller's mask (spot healing brush), document-sized.
            Surface* s = d.GetSurface(J.surface);
            if (s && J.maskCpu.size() == (size_t)s->tex->Width() * s->tex->Height()) {
                ownMask = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), gpu::Format::R8, "SpotHealMask");
                ctx->Upload(ownMask.get(), J.maskCpu.data(), s->tex->Width());
            }
        }
        gpu::Texture* maskTex = ownMask ? ownMask.get() : sel ? sel->tex.get() : nullptr;
        if (ok && maskTex && d.GetSurface(J.surface)) {
            auto crop = gpu::Dev().CreateTexture((u32)J.cw, (u32)J.ch, gpu::Format::RGBA32F, "FillCrop");
            ctx->Upload(crop.get(), J.rgba.data(), (u32)J.cw * 16);
            v.renderer.PasteMasked(d, *ctx, v.history, J.surface, crop.get(), {J.cx, J.cy}, maskTex, J.label);
            Status(J.label + " : terminé");
            auto then = std::move(J.then);
            J = {};
            if (then) then();
            return;
        }
        Status("Remplissage d'après le contenu impossible (zone trop grande par rapport au reste ?)");
        J = {};
    }
}

// ------------------------------------------------------------------ patch / content-aware move

void App::PatchToolInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    // No selection: draw one (lasso). In the selection: drag it to the area that replaces it (Patch)
    // or to where its content goes (content-aware move); released: healed blend.
    ImGuiIO& io = ImGui::GetIO();
    Document& doc = *v.doc;
    float2 dp = ScreenToDoc(v, canvasMin, io.MousePos);
    bool click = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    auto& P = patchDrag;
    if (P.probe) {
        if (!P.probe->ready) return;
        bool inside = !P.probe->data.empty() && P.probe->data[0] >= 128;
        P.probe.reset();
        if (inside && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            P.dragging = true;
            selMoving = true;  // the selection outline follows (display only)
            selMoveOffset = {0, 0};
        } else {
            // Outside: a new selection drawn with the lasso.
            selDragging = true;
            selDragMode = SelMode::Replace;
            lassoPts = {P.start};
        }
    }
    if (click && !P.dragging && !selDragging) {
        P.start = dp;
        if (doc.Selection()) {
            int x = (int)std::floor(dp.x), y = (int)std::floor(dp.y);
            if (x >= 0 && y >= 0 && x < (int)doc.Width() && y < (int)doc.Height()) {
                P.probe = v.renderer.ReadPixel(*ctx, doc.Selection()->tex.get(), x, y);
                return;
            }
        }
        selDragging = true;
        selDragMode = ModeFromModifiers();
        lassoPts = {dp};
        return;
    }
    if (selDragging) {
        for (auto& s : frameSamples) {
            float2 p = ScreenToDoc(v, canvasMin, ImVec2(s.x, s.y));
            if (Length(p - lassoPts.back()) * v.zoom >= 2.0f) lassoPts.push_back(p);
        }
        if (Length(dp - lassoPts.back()) * v.zoom >= 2.0f) lassoPts.push_back(dp);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            selDragging = false;
            FinishPolygon(v);
        }
        return;
    }
    if (!P.dragging) return;
    float2 dd = dp - P.start;
    selMoveOffset = {(int)std::lround(dd.x), (int)std::lround(dd.y)};
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
    P.dragging = false;
    selMoving = false;
    int2 off = selMoveOffset;
    selMoveOffset = {0, 0};
    if (off.x == 0 && off.y == 0) return;
    Id target = doc.PaintTarget();
    Surface* sel = doc.Selection();
    if (!target || !sel) { Status("Sélectionnez un calque de pixels"); return; }
    int radius = std::max(6, (int)(patchBlend * 40.0f));
    if (tool == Tool::Patch) {
        // Source mode: the selected area is rebuilt from where it was dropped.
        v.renderer.HealRegion(doc, *ctx, v.history, target, nullptr, sel->tex.get(), off, radius, "Pièce");
        return;
    }
    // Content-aware move: fill the original area, then place the content (healed) where it was dropped.
    Surface* s = doc.GetSurface(target);
    auto original = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "MoveSource");
    ctx->CopyTexture(original.get(), s->tex.get());
    DocView* vp = &v;
    StartContentAwareFill(v, [this, vp, target, original, off, radius] {
        Document& d = *vp->doc;
        if (!d.GetSurface(target) || !d.Selection()) return;
        vp->history.BeginGroup();
        gpu::Texture* moved = vp->renderer.OffsetSelectionPreview(d, *ctx, off);
        vp->renderer.HealRegion(d, *ctx, vp->history, target, original.get(), moved, {-off.x, -off.y}, radius, "Déplacement basé sur le contenu");
        vp->renderer.OffsetSelection(d, *ctx, vp->history, off, "Déplacer la sélection");
        vp->history.EndGroup();
    });
}

// ------------------------------------------------------------------ click tools

void App::ClickToolInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    Document& doc = *v.doc;
    if (!hovered || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
    float2 dp = ScreenToDoc(v, canvasMin, io.MousePos);
    Id target = doc.PaintTarget();
    if (!target) { Status("Sélectionnez un calque de pixels"); return; }
    if (tool == Tool::RedEye) {
        // One dab over the pupil: strongly red pixels become dark gray.
        if (!v.renderer.BeginStroke(doc, *ctx, target)) return;
        RetouchParams rp;
        rp.mode = RetouchMode::RedEye;
        rp.strength = redEyeDarken;
        BrushSettings b = retouch;
        b.hardness = 0.3f;
        b.opacity = 1;
        v.renderer.RetouchDabs(doc, *ctx, {{dp.x, dp.y, redEyeSize * 0.5f, 1.0f}}, b, rp);
        v.renderer.EndStroke(doc, *ctx, v.history, "Yeux rouges");
        return;
    }
    if (tool == Tool::MagicEraser && !wand.rb) {
        // Wand on the layer pixels, then erase the result.
        int x = (int)std::floor(dp.x), y = (int)std::floor(dp.y);
        if (x < 0 || y < 0 || x >= (int)doc.Width() || y >= (int)doc.Height()) return;
        Surface* s = doc.GetSurface(target);
        if (!s) return;
        wand.rb = v.renderer.ExportRGBA8(*ctx, s->tex.get());
        wand.x = x;
        wand.y = y;
        wand.view = &v;
        wand.fill = true;
        wand.erase = true;
        wand.surface = target;
    }
}

void App::TakeHistorySnapshot(DocView& v, Id surface) {
    Surface* s = v.doc->GetSurface(surface);
    if (!s) return;
    auto t = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "HistorySnapshot");
    ctx->CopyTexture(t.get(), s->tex.get());
    v.snapshots[surface] = t;
}

}  // namespace sx
