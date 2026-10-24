// Selection tools, free transform / move, clipboard and selection-aware layer edits.
#include "ui/App.h"
#include "ui/Filter.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <cmath>
#include <cstring>

namespace sx {

namespace {
constexpr float kPi = 3.14159265f;

float2 Rotate(float2 v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {c * v.x - s * v.y, s * v.x + c * v.y};
}

// 3x3 helpers (row-major)
void Mul3(const float* a, const float* b, float* out) {
    float r[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) r[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
    memcpy(out, r, sizeof(r));
}

bool Invert3(const float* m, float* out) {
    double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
    double A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    double det = a * A + b * B + c * C;
    if (std::abs(det) < 1e-12) return false;
    double inv = 1.0 / det;
    out[0] = (float)(A * inv);
    out[1] = (float)(-(b * i - c * h) * inv);
    out[2] = (float)((b * f - c * e) * inv);
    out[3] = (float)(B * inv);
    out[4] = (float)((a * i - c * g) * inv);
    out[5] = (float)(-(a * f - c * d) * inv);
    out[6] = (float)(C * inv);
    out[7] = (float)(-(a * h - b * g) * inv);
    out[8] = (float)((a * e - b * d) * inv);
    return true;
}

// Unit square (0,0),(1,0),(1,1),(0,1) -> quad q[0..3] (Heckbert).
void SquareToQuad(const float2* q, float* m) {
    float sx = q[0].x - q[1].x + q[2].x - q[3].x;
    float sy = q[0].y - q[1].y + q[2].y - q[3].y;
    float g = 0, h = 0;
    if (std::abs(sx) > 1e-6f || std::abs(sy) > 1e-6f) {
        float dx1 = q[1].x - q[2].x, dx2 = q[3].x - q[2].x, dy1 = q[1].y - q[2].y, dy2 = q[3].y - q[2].y;
        float den = dx1 * dy2 - dx2 * dy1;
        if (std::abs(den) > 1e-9f) {
            g = (sx * dy2 - dx2 * sy) / den;
            h = (dx1 * sy - sx * dy1) / den;
        }
    }
    m[0] = q[1].x - q[0].x + g * q[1].x;
    m[1] = q[3].x - q[0].x + h * q[3].x;
    m[2] = q[0].x;
    m[3] = q[1].y - q[0].y + g * q[1].y;
    m[4] = q[3].y - q[0].y + h * q[3].y;
    m[5] = q[0].y;
    m[6] = g;
    m[7] = h;
    m[8] = 1;
}

bool PointInQuad(const float2* q, float2 p) {
    bool pos = false, neg = false;
    for (int i = 0; i < 4; ++i) {
        float2 a = q[i], b = q[(i + 1) % 4];
        float c = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        if (c > 0) pos = true;
        if (c < 0) neg = true;
    }
    return !(pos && neg);
}

// Polygon scanline rasterization with 4x vertical supersampling and exact horizontal coverage.
void RasterizePolygon(const std::vector<float2>& pts, int W, int H, bool aa, Rect& r, std::vector<u8>& cov) {
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (auto& p : pts) {
        minx = std::min(minx, p.x); miny = std::min(miny, p.y);
        maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y);
    }
    r = Rect{(int)std::floor(minx), (int)std::floor(miny), (int)std::ceil(maxx) + 1, (int)std::ceil(maxy) + 1}.Clipped(W, H);
    cov.clear();
    if (r.Empty()) return;
    const int w = r.Width(), h = r.Height();
    const int SS = aa ? 4 : 1;
    std::vector<float> acc((size_t)w);
    cov.resize((size_t)w * h);
    std::vector<float> xs;
    for (int y = 0; y < h; ++y) {
        std::fill(acc.begin(), acc.end(), 0.0f);
        for (int s = 0; s < SS; ++s) {
            float sy = r.y0 + y + (s + 0.5f) / SS;
            xs.clear();
            for (size_t i = 0; i < pts.size(); ++i) {
                float2 a = pts[i], b = pts[(i + 1) % pts.size()];
                if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy))
                    xs.push_back(a.x + (sy - a.y) * (b.x - a.x) / (b.y - a.y));
            }
            std::sort(xs.begin(), xs.end());
            for (size_t k = 0; k + 1 < xs.size(); k += 2) {
                float xa = std::max(xs[k] - r.x0, 0.0f), xb = std::min(xs[k + 1] - r.x0, (float)w);
                if (xb <= xa) continue;
                if (!aa) { xa = std::round(xa); xb = std::round(xb); }
                for (int px = (int)std::floor(xa); px < (int)std::ceil(xb) && px < w; ++px)
                    acc[px] += (std::min(xb, px + 1.0f) - std::max(xa, (float)px)) / SS;
            }
        }
        for (int x = 0; x < w; ++x) cov[(size_t)y * w + x] = (u8)std::clamp((int)std::lround(acc[x] * 255.0f), 0, 255);
    }
}

// Magic wand on straight sRGB RGBA8 data.
void WandMask(const std::vector<u8>& img, int W, int H, int sx, int sy, int tol, bool contiguous, std::vector<u8>& out) {
    out.assign((size_t)W * H, 0);
    const u8* seed = &img[((size_t)sy * W + sx) * 4];
    auto match = [&](size_t i) {
        const u8* p = &img[i * 4];
        int d = std::max({std::abs(p[0] - seed[0]), std::abs(p[1] - seed[1]), std::abs(p[2] - seed[2]), std::abs(p[3] - seed[3])});
        return d <= tol;
    };
    if (!contiguous) {
        for (size_t i = 0; i < (size_t)W * H; ++i) out[i] = match(i) ? 255 : 0;
        return;
    }
    std::vector<std::pair<int, int>> stack{{sx, sy}};
    while (!stack.empty()) {
        auto [x, y] = stack.back();
        stack.pop_back();
        size_t row = (size_t)y * W;
        if (out[row + x] || !match(row + x)) continue;
        int l = x, rr = x;
        while (l > 0 && !out[row + l - 1] && match(row + l - 1)) --l;
        while (rr < W - 1 && !out[row + rr + 1] && match(row + rr + 1)) ++rr;
        for (int i = l; i <= rr; ++i) out[row + i] = 255;
        for (int ny : {y - 1, y + 1}) {
            if (ny < 0 || ny >= H) continue;
            size_t nrow = (size_t)ny * W;
            bool inSpan = false;
            for (int i = l; i <= rr; ++i) {
                bool ok = !out[nrow + i] && match(nrow + i);
                if (ok && !inSpan) stack.push_back({i, ny});
                inSpan = ok;
            }
        }
    }
}
}  // namespace

SelMode App::ModeFromModifiers() const {
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyShift && io.KeyAlt) return SelMode::Intersect;
    if (io.KeyShift) return SelMode::Add;
    if (io.KeyAlt) return SelMode::Subtract;
    return (SelMode)selModeOption;
}

// ------------------------------------------------------------------ selection tools

void App::SelectionToolInput(DocView& v, ImVec2 canvasMin, bool hovered, const std::vector<PointerSample>& samples) {
    ImGuiIO& io = ImGui::GetIO();
    Document& doc = *v.doc;
    float2 dp = ScreenToDoc(v, canvasMin, io.MousePos);
    if (tool == Tool::RectSelect || tool == Tool::EllipseSelect) dp = Snap(v, dp);
    bool click = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    static bool shiftAtStart = false;

    // Dragging inside the selection (no modifier) moves the outline, like Photoshop.
    bool canMove = tool == Tool::RectSelect || tool == Tool::EllipseSelect || tool == Tool::Lasso || (tool == Tool::PolyLasso && !polyActive);
    if (click && canMove && doc.Selection() && ModeFromModifiers() == SelMode::Replace && !selProbe) {
        int x = (int)std::floor(dp.x), y = (int)std::floor(dp.y);
        if (x >= 0 && y >= 0 && x < (int)doc.Width() && y < (int)doc.Height()) {
            selProbe = v.renderer.ReadPixel(*ctx, doc.Selection()->tex.get(), x, y);
            selProbeStart = dp;
            return;
        }
    }
    if (selProbe) {
        if (!selProbe->ready) return;  // decided in a frame or two
        bool inside = !selProbe->data.empty() && selProbe->data[0] >= 128;
        selProbe.reset();
        bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (inside) {
            selMoving = down;
            selMoveOffset = {0, 0};
        } else if (!down) {
            v.renderer.Deselect(doc, v.history);  // simple click outside
            return;
        } else {
            // Start the selection gesture where the click happened.
            selDragMode = SelMode::Replace;
            if (tool == Tool::RectSelect || tool == Tool::EllipseSelect) {
                selDragging = true;
                selStart = selEnd = selProbeStart;
                shiftAtStart = io.KeyShift;
            } else if (tool == Tool::Lasso) {
                selDragging = true;
                lassoPts = {selProbeStart};
            } else if (tool == Tool::PolyLasso) {
                polyActive = true;
                lassoPts = {selProbeStart};
            }
        }
    }
    if (selMoving) {
        float2 d = dp - selProbeStart;
        selMoveOffset = {(int)std::lround(d.x), (int)std::lround(d.y)};
        if (io.KeyShift) {  // constrain to an axis
            if (std::abs(selMoveOffset.x) > std::abs(selMoveOffset.y)) selMoveOffset.y = 0;
            else selMoveOffset.x = 0;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            selMoving = false;
            v.renderer.OffsetSelection(doc, *ctx, v.history, selMoveOffset, "Déplacer la sélection");
        }
        return;
    }

    if (tool == Tool::RectSelect || tool == Tool::EllipseSelect) {
        if (click) {
            selDragging = true;
            selStart = selEnd = dp;
            selDragMode = ModeFromModifiers();
            shiftAtStart = io.KeyShift;
        }
        if (!selDragging) return;
        selEnd = dp;
        if (io.KeyShift && !shiftAtStart) {  // square / circle
            float2 d = selEnd - selStart;
            float m = std::max(std::abs(d.x), std::abs(d.y));
            selEnd = selStart + float2{d.x < 0 ? -m : m, d.y < 0 ? -m : m};
        }
        if (io.KeyAlt && !(selDragMode == SelMode::Subtract || selDragMode == SelMode::Intersect)) {  // from center
            selStart = selStart;  // (center mode handled at release)
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            selDragging = false;
            float2 a = selStart, b = selEnd;
            if (Length(b - a) < 1.5f) {
                if (selDragMode == SelMode::Replace) v.renderer.Deselect(doc, v.history);
                return;
            }
            float4 bounds = {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
            if (tool == Tool::RectSelect) {  // snap rectangles to whole pixels
                bounds = {std::round(bounds.x), std::round(bounds.y), std::round(bounds.z), std::round(bounds.w)};
            }
            v.renderer.SelectShape(doc, *ctx, v.history, tool == Tool::RectSelect ? SelShape::Rect : SelShape::Ellipse, bounds,
                                   selDragMode, selAntialias, selFeather,
                                   tool == Tool::RectSelect ? "Sélection rectangulaire" : "Sélection elliptique");
        }
        return;
    }

    if (tool == Tool::Lasso) {
        if (click) {
            selDragging = true;
            selDragMode = ModeFromModifiers();
            lassoPts = {dp};
        }
        if (!selDragging) return;
        for (auto& s : samples) {
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

    if (tool == Tool::PolyLasso) {
        if (click) {
            if (!polyActive) {
                polyActive = true;
                selDragMode = ModeFromModifiers();
                lassoPts = {dp};
            } else {
                float closeDist = Length(dp - lassoPts.front()) * v.zoom;
                if ((lassoPts.size() >= 3 && closeDist < 10.0f) || ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    FinishPolygon(v);
                } else {
                    lassoPts.push_back(dp);
                }
            }
        }
        return;
    }

    if (tool == Tool::Wand && click && !wand.rb) {
        int x = (int)std::floor(dp.x), y = (int)std::floor(dp.y);
        if (x < 0 || y < 0 || x >= (int)doc.Width() || y >= (int)doc.Height()) return;
        gpu::Texture* src = nullptr;
        if (!wandSampleMerged) {
            if (Surface* s = doc.GetSurface(doc.PaintTarget())) src = s->tex.get();
        }
        if (!src) src = v.renderer.EvaluateComplete(doc, *ctx);
        if (!src) return;
        wand.rb = v.renderer.ExportRGBA8(*ctx, src);
        wand.x = x;
        wand.y = y;
        wand.mode = ModeFromModifiers();
        wand.view = &v;
        wand.fill = false;
    }
}

void App::FinishPolygon(DocView& v) {
    Document& doc = *v.doc;
    if (tool == Tool::MagneticLasso) {
        // The live path to the cursor is part of the outline.
        for (size_t i = 1; i < magLasso.live.size(); ++i) lassoPts.push_back(magLasso.live[i]);
        magLasso.live.clear();
        magLasso.anchors.clear();
    }
    polyActive = false;
    selDragging = false;
    if (lassoPts.size() < 3) {
        if (selDragMode == SelMode::Replace) v.renderer.Deselect(doc, v.history);
        lassoPts.clear();
        return;
    }
    Rect r;
    std::vector<u8> cov;
    RasterizePolygon(lassoPts, (int)doc.Width(), (int)doc.Height(), selAntialias, r, cov);
    lassoPts.clear();
    if (r.Empty()) return;
    v.renderer.SelectCoverage(doc, *ctx, v.history, cov, r, selDragMode, selFeather, tool == Tool::PolyLasso ? "Lasso polygonal" : "Lasso");
}

void App::UpdateWand(DocView& v) {
    if (!wand.rb || !wand.rb->ready) return;
    if (wand.view != &v) return;
    auto rb = wand.rb;
    wand.rb.reset();
    Document& doc = *v.doc;
    int W = (int)rb->width, H = (int)rb->height;
    if (wand.x >= W || wand.y >= H) return;
    std::vector<u8> mask;
    if (wand.fill) {
        WandMask(rb->data, W, H, wand.x, wand.y, (int)bucketTolerance, bucketContiguous, mask);
        auto cov = gpu::Dev().CreateTexture((u32)W, (u32)H, gpu::Format::R8, "BucketCoverage");
        if (!cov || !doc.GetSurface(wand.surface)) return;
        ctx->Upload(cov.get(), mask.data(), (u32)W);
        if (wand.erase) v.renderer.FillSelection(doc, *ctx, &v.history, wand.surface, 1, {}, 1.0f, "Gomme magique", nullptr, cov.get(), false);
        else v.renderer.FillSelection(doc, *ctx, &v.history, wand.surface, 0, LinearColor(fg), 1.0f, "Pot de peinture", nullptr, cov.get());
        return;
    }
    WandMask(rb->data, W, H, wand.x, wand.y, (int)wandTolerance, wandContiguous, mask);
    v.renderer.SelectCoverage(doc, *ctx, v.history, mask, {0, 0, W, H}, wand.mode, selFeather + (selAntialias ? 0.6f : 0.0f), "Baguette magique");
}

void App::DrawSelectionOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    auto dashed = [&](ImVec2 a, ImVec2 b) {
        dl->AddLine(a, b, IM_COL32(0, 0, 0, 200), 2.0f);
        dl->AddLine(a, b, IM_COL32(255, 255, 255, 230), 1.0f);
    };
    if (selDragging && (tool == Tool::RectSelect || tool == Tool::EllipseSelect)) {
        ImVec2 a = toScreen(selStart), b = toScreen(selEnd);
        ImVec2 mn(std::min(a.x, b.x), std::min(a.y, b.y)), mx(std::max(a.x, b.x), std::max(a.y, b.y));
        if (tool == Tool::RectSelect) {
            dl->AddRect(mn, mx, IM_COL32(0, 0, 0, 200), 0, 0, 2.0f);
            dl->AddRect(mn, mx, IM_COL32(255, 255, 255, 230));
        } else {
            ImVec2 c((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);
            dl->AddEllipse(c, ImVec2((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f), IM_COL32(0, 0, 0, 200), 0, 0, 2.0f);
            dl->AddEllipse(c, ImVec2((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f), IM_COL32(255, 255, 255, 230));
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f x %.0f", std::abs(selEnd.x - selStart.x), std::abs(selEnd.y - selStart.y));
        dl->AddText(ImVec2(mx.x + 8, mx.y + 4), IM_COL32_WHITE, buf);
    }
    if ((selDragging && tool == Tool::Lasso) || polyActive) {
        for (size_t i = 0; i + 1 < lassoPts.size(); ++i) dashed(toScreen(lassoPts[i]), toScreen(lassoPts[i + 1]));
        if (polyActive && tool == Tool::MagneticLasso) {
            // Live path snapped to the edges, anchors as small squares.
            for (size_t i = 0; i + 1 < magLasso.live.size(); ++i) dashed(toScreen(magLasso.live[i]), toScreen(magLasso.live[i + 1]));
            for (size_t a : magLasso.anchors)
                if (a > 0 && a <= lassoPts.size()) {
                    ImVec2 p = toScreen(lassoPts[a - 1]);
                    dl->AddRect(ImVec2(p.x - 3, p.y - 3), ImVec2(p.x + 3, p.y + 3), IM_COL32_WHITE);
                }
            if (magLasso.cost.empty()) dl->AddText(ImVec2(ImGui::GetIO().MousePos.x + 14, ImGui::GetIO().MousePos.y + 10), IM_COL32(255, 220, 120, 255), "analyse des contours...");
        } else if (polyActive && !lassoPts.empty()) {
            dashed(toScreen(lassoPts.back()), ImGui::GetIO().MousePos);
            dl->AddCircle(toScreen(lassoPts.front()), 6, IM_COL32_WHITE);
        }
    }
}

// ------------------------------------------------------------------ transform

void App::TransformUpdateCorners() {
    if (xf.freeCorners) return;
    const float2 local[4] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
    for (int i = 0; i < 4; ++i) xf.corners[i] = xf.center + Rotate({local[i].x * xf.w, local[i].y * xf.h}, xf.angle);
    if (!xf.pivotCustom) xf.pivot = xf.center;
}

bool App::TransformBegin(DocView& v, bool moveOnly) {
    if (xf.active) return true;
    Document& doc = *v.doc;
    Id target = doc.PaintTarget();
    Surface* s = doc.GetSurface(target);
    if (Layer* al = doc.ActiveLayer(); al && al->kind == LayerKind::Smart && doc.Mode() == DocMode::Layers && !doc.S().editMask) {
        // Smart object: the corners move, the contents are re-placed from the source (no loss).
        SmartObject* so = doc.GetSmart(al->smart);
        if (!so || !so->content) return false;
        xf = TransformState{};
        xf.active = true;
        xf.view = &v;
        xf.moveOnly = moveOnly;
        xf.surface = al->surface;
        xf.smartLayer = al->id;
        xf.before = doc.S();
        for (int i = 0; i < 4; ++i) xf.smartStart[i] = xf.corners[i] = al->smartCorners[i];
        xf.src = {0, 0, (int)so->width, (int)so->height};
        const float2* c = al->smartCorners;
        float2 ex = c[1] - c[0], ey = c[3] - c[0];
        xf.center = (c[0] + c[2]) * 0.5f;
        xf.w = Length(ex);
        xf.h = Length(ey);
        xf.angle = std::atan2(ex.y, ex.x);
        if (ex.x * ey.y - ex.y * ey.x < 0) xf.h = -xf.h;  // mirrored: kept when the corners are rebuilt
        // Not a rotated rectangle (skew / perspective): free corners.
        float2 rebuilt = c[0] + ex + ey;
        xf.freeCorners = Length(rebuilt - c[2]) > 0.5f || std::abs(ex.x * ey.x + ex.y * ey.y) > 1e-3f * xf.w * std::abs(xf.h);
        if (!xf.freeCorners) TransformUpdateCorners();
        xf.pivot = xf.center;
        return true;
    }
    if (Layer* al = doc.ActiveLayer(); al && (al->kind == LayerKind::Text || al->kind == LayerKind::Shape) && doc.Mode() == DocMode::Layers) {
        Status("Calque vectoriel : utilisez Calque > Pixelliser pour le transformer");
        return false;
    }
    if (!s) {
        Status("Aucun calque de pixels à transformer");
        return false;
    }
    xf = TransformState{};
    xf.active = true;
    xf.view = &v;
    xf.moveOnly = moveOnly;
    xf.surface = target;
    xf.selection = doc.S().selection;
    xf.original = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "TransformOriginal");
    ctx->CopyTexture(xf.original.get(), s->tex.get());
    if (moveOnly) {
        xf.src = {0, 0, (int)doc.Width(), (int)doc.Height()};
        xf.center = {doc.Width() * 0.5f, doc.Height() * 0.5f};
        xf.w = (float)doc.Width();
        xf.h = (float)doc.Height();
        TransformUpdateCorners();
    } else {
        Surface* sel = doc.Selection();
        xf.boundsRb = v.renderer.ComputeBounds(*ctx, xf.original.get(), sel ? sel->tex.get() : nullptr, doc.S().editMask);
    }
    return true;
}

bool App::TransformSelectionBegin(DocView& v) {
    if (xf.active) return false;
    Document& doc = *v.doc;
    Surface* sel = doc.Selection();
    if (!sel) {
        Status("Aucune sélection à transformer");
        return false;
    }
    xf = TransformState{};
    xf.active = true;
    xf.view = &v;
    xf.selectionOnly = true;
    xf.before = doc.S();
    xf.original = gpu::Dev().CreateTexture(sel->tex->Width(), sel->tex->Height(), gpu::Format::R8, "TransformOriginal");
    ctx->CopyTexture(xf.original.get(), sel->tex.get());
    // Work on a new selection surface so the previous one stays intact for undo.
    xf.surface = doc.CreateSurfaceRaw(gpu::Format::R8, "Selection");
    ctx->CopyTexture(doc.GetSurface(xf.surface)->tex.get(), sel->tex.get());
    doc.S().selection = xf.surface;
    xf.boundsRb = v.renderer.ComputeBounds(*ctx, xf.original.get(), nullptr, true, true);
    return true;
}

void App::TransformApply(DocView& v) {
    if (!xf.active || xf.view != &v) return;
    Document& doc = *v.doc;
    if (xf.boundsRb) {
        if (!xf.boundsRb->ready) return;
        const u32* b = (const u32*)xf.boundsRb->data.data();
        u32 x0 = b[0], y0 = b[1], x1 = b[2], y1 = b[3];
        xf.boundsRb.reset();
        if (x0 >= x1 || y0 >= y1) {
            Status("Rien à transformer (calque ou sélection vide)");
            xf = TransformState{};
            return;
        }
        xf.src = {(int)x0, (int)y0, (int)x1, (int)y1};
        xf.center = {(x0 + x1) * 0.5f, (y0 + y1) * 0.5f};
        xf.w = (float)(x1 - x0);
        xf.h = (float)(y1 - y0);
        TransformUpdateCorners();
        if (xf.warpPending) WarpFromCorners();
    }
    if (!xf.dirty) return;
    xf.dirty = false;
    if (xf.smartLayer) {
        if (Layer* l = doc.FindLayer(xf.smartLayer)) for (int i = 0; i < 4; ++i) l->smartCorners[i] = xf.corners[i];
        return;  // SyncSmartLayers re-places the contents
    }
    if (xf.warp) {
        Surface* sel = xf.selectionOnly ? nullptr : doc.GetSurface(xf.selection);
        if (xf.warpKind) {
            xf.mesh = BuildMeshWarp();
            v.renderer.WarpPreview(doc, *ctx, xf.surface, xf.original.get(), sel ? sel->tex.get() : nullptr, xf.src, xf.mesh, xf.selectionOnly);
            return;
        }
        v.renderer.WarpPreview(doc, *ctx, xf.surface, xf.original.get(), sel ? sel->tex.get() : nullptr, xf.src, xf.grid, xf.selectionOnly);
        return;
    }
    float m[9], n[9], hm[9];
    SquareToQuad(xf.corners, m);
    const float sw = (float)xf.src.Width(), sh = (float)xf.src.Height();
    float norm[9] = {1.0f / sw, 0, -xf.src.x0 / sw, 0, 1.0f / sh, -xf.src.y0 / sh, 0, 0, 1};
    Mul3(m, norm, hm);
    TransformParams t;
    if (!Invert3(hm, n)) return;
    memcpy(t.hinv, n, sizeof(n));
    t.src = xf.src;
    t.interp = xf.moveOnly ? 1 : xf.interp;
    // Pure integer translation: use nearest so pixels stay crisp.
    if (xf.moveOnly) {
        float dx = xf.center.x - doc.Width() * 0.5f, dy = xf.center.y - doc.Height() * 0.5f;
        if (std::abs(dx - std::round(dx)) < 1e-3f && std::abs(dy - std::round(dy)) < 1e-3f) t.interp = 0;
    }
    if (xf.selectionOnly) t.interp = 1;
    Surface* sel = xf.selectionOnly ? nullptr : doc.GetSurface(xf.selection);
    v.renderer.TransformPreview(doc, *ctx, xf.surface, xf.original.get(), sel ? sel->tex.get() : nullptr, t);
}

void App::TransformCommit(DocView& v) {
    if (!xf.active) return;
    if (xf.boundsRb) { xf = TransformState{}; return; }
    Document& doc = *v.doc;
    TransformApply(v);
    if (xf.smartLayer) {
        bool moved = false;
        for (int i = 0; i < 4; ++i) moved |= Length(xf.corners[i] - xf.smartStart[i]) > 1e-3f;
        if (moved) v.history.PushState(xf.moveOnly ? "Déplacement" : "Transformation (objet dynamique)", xf.before, doc.S());
        xf = TransformState{};
        return;
    }
    // Warp: compare the grid with the undeformed one.
    bool warped = false;
    if (xf.warp && xf.warpKind) warped = MeshWarpChanged();
    else if (xf.warp)
        for (int j = 0; j < 4; ++j)
            for (int i = 0; i < 4; ++i) {
                float2 rest = {xf.src.x0 + xf.src.Width() * i / 3.0f, xf.src.y0 + xf.src.Height() * j / 3.0f};
                if (Length(xf.grid[j * 4 + i] - rest) > 1e-3f) warped = true;
            }
    if (xf.selectionOnly) {
        bool changed = warped;
        const float2 local[4] = {{(float)xf.src.x0, (float)xf.src.y0}, {(float)xf.src.x1, (float)xf.src.y0},
                                 {(float)xf.src.x1, (float)xf.src.y1}, {(float)xf.src.x0, (float)xf.src.y1}};
        if (!xf.warp) for (int i = 0; i < 4; ++i) if (Length(xf.corners[i] - local[i]) > 1e-3f) changed = true;
        if (changed) {
            doc.S().lastSelection = doc.S().selection;
            v.history.PushState("Transformer la sélection", xf.before, doc.S());
        } else {
            doc.S().selection = xf.before.selection;
        }
        xf = TransformState{};
        return;
    }
    Surface* s = doc.GetSurface(xf.surface);
    bool moved = warped;
    for (int i = 0; i < 4 && !xf.warp; ++i) {
        const float2 local[4] = {{(float)xf.src.x0, (float)xf.src.y0}, {(float)xf.src.x1, (float)xf.src.y0},
                                 {(float)xf.src.x1, (float)xf.src.y1}, {(float)xf.src.x0, (float)xf.src.y1}};
        if (Length(xf.corners[i] - local[i]) > 1e-3f) moved = true;
    }
    if (s && moved) {
        v.history.BeginGroup();
        auto after = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "UndoAfter");
        ctx->CopyTexture(after.get(), s->tex.get());
        v.history.PushPaint(xf.warp ? (xf.warpKind == 1 ? "Marionnette" : xf.warpKind == 2 ? "Déformation de la perspective" : "Déformation") : xf.moveOnly ? "Déplacement" : "Transformation", xf.surface,
                            {0, 0, (int)s->tex->Width(), (int)s->tex->Height()}, xf.original, after);
        // The selection follows the pixels.
        Surface* sel = doc.GetSurface(xf.selection);
        if (sel && xf.warp) {
            DocState before = doc.S();
            Id nid = doc.CreateSurfaceRaw(gpu::Format::R8, "Selection");
            if (xf.warpKind) v.renderer.WarpPreview(doc, *ctx, nid, sel->tex.get(), nullptr, xf.src, xf.mesh, true);
            else v.renderer.WarpPreview(doc, *ctx, nid, sel->tex.get(), nullptr, xf.src, xf.grid, true);
            doc.S().selection = nid;
            doc.S().lastSelection = nid;
            v.history.PushState("Transformer la sélection", before, doc.S());
        } else if (sel) {
            DocState before = doc.S();
            Id nid = doc.CreateSurfaceRaw(gpu::Format::R8, "Selection");
            float m[9], n[9], hm[9];
            SquareToQuad(xf.corners, m);
            const float sw = (float)xf.src.Width(), sh = (float)xf.src.Height();
            float norm[9] = {1.0f / sw, 0, -xf.src.x0 / sw, 0, 1.0f / sh, -xf.src.y0 / sh, 0, 0, 1};
            Mul3(m, norm, hm);
            if (Invert3(hm, n)) {
                TransformParams t;
                memcpy(t.hinv, n, sizeof(n));
                t.src = xf.src;
                t.interp = 1;
                v.renderer.TransformPreview(doc, *ctx, nid, sel->tex.get(), nullptr, t);
                doc.S().selection = nid;
                doc.S().lastSelection = nid;
                v.history.PushState("Transformer la sélection", before, doc.S());
            }
        }
        v.history.EndGroup();
    } else if (s && !moved) {
        ctx->CopyTexture(s->tex.get(), xf.original.get());
        doc.TouchSurface(xf.surface);
    }
    xf = TransformState{};
}

void App::TransformCancel(DocView& v) {
    if (!xf.active) return;
    Document& doc = *v.doc;
    if (xf.selectionOnly) {
        doc.S().selection = xf.before.selection;
        xf = TransformState{};
        return;
    }
    if (xf.smartLayer) {
        if (Layer* l = doc.FindLayer(xf.smartLayer)) for (int i = 0; i < 4; ++i) l->smartCorners[i] = xf.smartStart[i];
        xf = TransformState{};
        return;
    }
    if (Surface* s = doc.GetSurface(xf.surface)) {
        ctx->CopyTexture(s->tex.get(), xf.original.get());
        doc.TouchSurface(xf.surface);
    }
    xf = TransformState{};
}

void App::Nudge(DocView& v, float dx, float dy) {
    if (!xf.active && !TransformBegin(v, true)) return;
    if (xf.boundsRb) return;
    xf.center = xf.center + float2{dx, dy};
    if (xf.freeCorners) for (auto& c : xf.corners) c = c + float2{dx, dy};
    TransformUpdateCorners();
    xf.dirty = true;
    if (xf.moveOnly) TransformCommit(v);
}

void App::TransformInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    if (!xf.active || xf.boundsRb) return;
    if (xf.warp && xf.warpKind) { MeshWarpInput(v, canvasMin, hovered); return; }
    if (xf.warp) { WarpInput(v, canvasMin, hovered); return; }
    ImGuiIO& io = ImGui::GetIO();
    float2 m = ScreenToDoc(v, canvasMin, io.MousePos);
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    auto hitNear = [&](float2 p, float r) { ImVec2 s = toScreen(p); float dx = s.x - io.MousePos.x, dy = s.y - io.MousePos.y; return dx * dx + dy * dy <= r * r; };

    // Hit test
    int hit = -1;
    if (!xf.moveOnly) {
        if (hitNear(xf.pivot, 8)) hit = 10;
        for (int i = 0; i < 4 && hit < 0; ++i) if (hitNear(xf.corners[i], 9)) hit = i;
        for (int i = 0; i < 4 && hit < 0; ++i) if (hitNear((xf.corners[i] + xf.corners[(i + 1) % 4]) * 0.5f, 8)) hit = 4 + i;
        if (hit < 0) hit = PointInQuad(xf.corners, m) ? 8 : 9;
    } else {
        hit = 8;
    }
    if (hovered && xf.drag < 0) {
        if (hit < 8) ImGui::SetMouseCursor(hit < 4 ? ImGuiMouseCursor_ResizeNWSE : (hit % 2 ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS));
        else if (hit == 8) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        else ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        xf.drag = hit;
        xf.dragStart = m;
        xf.startCenter = xf.center;
        xf.startPivot = xf.pivot;
        xf.startW = xf.w;
        xf.startH = xf.h;
        xf.startAngle = xf.angle;
        memcpy(xf.startCorners, xf.corners, sizeof(xf.corners));
        if (hit < 4 && io.KeyCtrl) xf.freeCorners = true;
    }
    if (xf.drag >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        float2 delta = m - xf.dragStart;
        int d = xf.drag;
        if (d == 8) {  // move
            if (io.KeyShift) {  // constrain to axis
                if (std::abs(delta.x) > std::abs(delta.y)) delta.y = 0; else delta.x = 0;
            }
            xf.center = xf.startCenter + delta;
            if (xf.freeCorners) for (int i = 0; i < 4; ++i) xf.corners[i] = xf.startCorners[i] + delta;
            if (xf.pivotCustom) xf.pivot = xf.startPivot + delta;
        } else if (d == 10) {
            xf.pivot = m;
            xf.pivotCustom = true;
        } else if (d == 9) {  // rotate around pivot
            float2 a = xf.dragStart - xf.startPivot, b = m - xf.startPivot;
            float da = std::atan2(b.y, b.x) - std::atan2(a.y, a.x);
            if (io.KeyShift) {
                float total = xf.startAngle + da;
                float snapped = std::round(total / (kPi / 12)) * (kPi / 12);
                da = snapped - xf.startAngle;
            }
            if (xf.freeCorners) {
                for (int i = 0; i < 4; ++i) xf.corners[i] = xf.startPivot + Rotate(xf.startCorners[i] - xf.startPivot, da);
            } else {
                xf.angle = xf.startAngle + da;
                xf.center = xf.startPivot + Rotate(xf.startCenter - xf.startPivot, da);
                if (!xf.pivotCustom) xf.pivot = xf.center;
            }
        } else if (xf.freeCorners) {  // distort / perspective
            if (d < 4) xf.corners[d] = xf.startCorners[d] + delta;
            else {
                int e = d - 4;
                xf.corners[e] = xf.startCorners[e] + delta;
                xf.corners[(e + 1) % 4] = xf.startCorners[(e + 1) % 4] + delta;
            }
        } else {  // scale
            bool fromCenter = io.KeyAlt;
            float sx = 0, sy = 0;
            if (d < 4) { sx = (d == 1 || d == 2) ? 1.0f : -1.0f; sy = d >= 2 ? 1.0f : -1.0f; }
            else {
                int e = d - 4;  // 0 top, 1 right, 2 bottom, 3 left
                if (e == 0) sy = -1; else if (e == 2) sy = 1; else if (e == 1) sx = 1; else sx = -1;
            }
            // Anchor: opposite corner/edge (or center with Alt), in the rotated frame.
            float2 anchorLocal = {fromCenter ? 0.0f : -sx * xf.startW * 0.5f, fromCenter ? 0.0f : -sy * xf.startH * 0.5f};
            float2 anchor = xf.startCenter + Rotate(anchorLocal, xf.startAngle);
            float2 local = Rotate(m - anchor, -xf.startAngle);
            float k = fromCenter ? 2.0f : 1.0f;
            float nw = sx != 0 ? local.x * sx * k : xf.startW;
            float nh = sy != 0 ? local.y * sy * k : xf.startH;
            if (io.KeyShift) {  // proportional
                float rx = sx != 0 ? nw / xf.startW : 0, ry = sy != 0 ? nh / xf.startH : 0;
                float r = (sx != 0 && sy != 0) ? (std::abs(rx) > std::abs(ry) ? rx : ry) : (sx != 0 ? rx : ry);
                nw = xf.startW * r;
                nh = xf.startH * r;
            }
            if (std::abs(nw) < 1) nw = nw < 0 ? -1.0f : 1.0f;
            if (std::abs(nh) < 1) nh = nh < 0 ? -1.0f : 1.0f;
            xf.w = nw;
            xf.h = nh;
            if (fromCenter) xf.center = xf.startCenter;
            else {
                float2 c = {sx != 0 ? sx * nw * 0.5f : 0.0f, sy != 0 ? sy * nh * 0.5f : 0.0f};
                // Keep the anchor fixed; for edge drags the perpendicular axis stays centered.
                float2 base = {sx != 0 ? anchorLocal.x : 0.0f, sy != 0 ? anchorLocal.y : 0.0f};
                xf.center = xf.startCenter + Rotate(base + c, xf.startAngle);
            }
        }
        TransformUpdateCorners();
        xf.dirty = true;
    }
    if (xf.drag >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        xf.drag = -1;
        if (xf.moveOnly) TransformCommit(v);
    }
}

void App::TransformDrawOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    if (!xf.active || xf.view != &v || xf.moveOnly || xf.boundsRb) return;
    if (xf.warp && xf.warpKind) { MeshWarpDrawOverlay(v, canvasMin, dl); return; }
    if (xf.warp) { WarpDrawOverlay(v, canvasMin, dl); return; }
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    ImVec2 q[4];
    for (int i = 0; i < 4; ++i) q[i] = toScreen(xf.corners[i]);
    dl->AddPolyline(q, 4, IM_COL32(0, 0, 0, 160), ImDrawFlags_Closed, 3.0f);
    dl->AddPolyline(q, 4, ui::col::Accent2, ImDrawFlags_Closed, 1.5f);
    auto handle = [&](ImVec2 p) {
        dl->AddRectFilled(ImVec2(p.x - 5, p.y - 5), ImVec2(p.x + 5, p.y + 5), IM_COL32_WHITE);
        dl->AddRect(ImVec2(p.x - 5, p.y - 5), ImVec2(p.x + 5, p.y + 5), IM_COL32(20, 20, 24, 255));
    };
    for (int i = 0; i < 4; ++i) {
        handle(q[i]);
        handle(ImVec2((q[i].x + q[(i + 1) % 4].x) * 0.5f, (q[i].y + q[(i + 1) % 4].y) * 0.5f));
    }
    ImVec2 pv = toScreen(xf.pivot);
    dl->AddCircle(pv, 6, IM_COL32(20, 20, 24, 255), 0, 3);
    dl->AddCircle(pv, 6, IM_COL32_WHITE, 0, 1.5f);
    dl->AddLine(ImVec2(pv.x - 9, pv.y), ImVec2(pv.x + 9, pv.y), IM_COL32_WHITE);
    dl->AddLine(ImVec2(pv.x, pv.y - 9), ImVec2(pv.x, pv.y + 9), IM_COL32_WHITE);
    if (xf.drag >= 0) {
        char buf[96];
        if (xf.drag == 9) snprintf(buf, sizeof(buf), "%.1f°", xf.angle * 180.0f / kPi);
        else snprintf(buf, sizeof(buf), "L %.1f %%  H %.1f %%", xf.w / xf.src.Width() * 100.0f, xf.h / xf.src.Height() * 100.0f);
        ImVec2 mp = ImGui::GetIO().MousePos;
        dl->AddRectFilled(ImVec2(mp.x + 14, mp.y + 14), ImVec2(mp.x + 24 + ImGui::CalcTextSize(buf).x, mp.y + 20 + ImGui::GetFontSize()), IM_COL32(20, 20, 24, 220), 4);
        dl->AddText(ImVec2(mp.x + 19, mp.y + 17), IM_COL32_WHITE, buf);
    }
}

// ------------------------------------------------------------------ warp

bool App::WarpBegin(DocView& v) {
    if (!xf.active && !TransformBegin(v, false)) return false;
    if (xf.moveOnly) return false;
    if (xf.boundsRb) xf.warpPending = true;  // grid set up once the bounds are known
    else WarpFromCorners();
    return true;
}

void App::WarpFromCorners() {
    // Start from the current quad (bilinear placement of the 16 points).
    xf.warpPending = false;
    xf.warp = true;
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i) {
            float u = i / 3.0f, vv = j / 3.0f;
            float2 top = xf.corners[0] * (1 - u) + xf.corners[1] * u, bottom = xf.corners[3] * (1 - u) + xf.corners[2] * u;
            xf.grid[j * 4 + i] = top * (1 - vv) + bottom * vv;
        }
    xf.dirty = true;
}

void App::WarpInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    float2 m = ScreenToDoc(v, canvasMin, io.MousePos);
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    auto hitNear = [&](float2 p, float r) { ImVec2 s = toScreen(p); float dx = s.x - io.MousePos.x, dy = s.y - io.MousePos.y; return dx * dx + dy * dy <= r * r; };
    int hit = -1;
    // Corners first, then tangent handles, then interior points.
    static const int order[16] = {0, 3, 15, 12, 1, 2, 4, 8, 7, 11, 13, 14, 5, 6, 9, 10};
    for (int k = 0; k < 16 && hit < 0; ++k) if (hitNear(xf.grid[order[k]], 9)) hit = order[k];
    // Otherwise: a point of the surface (closest sample of the patch).
    float2 uvHit;
    if (hit < 0) {
        float best = 1e30f;
        const int S = 32;
        for (int y = 0; y <= S; ++y)
            for (int x = 0; x <= S; ++x) {
                float2 p = BezierPatch(xf.grid, (float)x / S, (float)y / S);
                float d = Length(p - m);
                if (d < best) { best = d; uvHit = {(float)x / S, (float)y / S}; }
            }
        if (best * v.zoom < 24.0f) hit = 16;
    }
    if (hovered && xf.warpDrag < 0) ImGui::SetMouseCursor(hit >= 0 ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Arrow);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hit >= 0) {
        xf.warpDrag = hit;
        xf.warpUV = uvHit;
        xf.dragStart = m;
        memcpy(xf.startGrid, xf.grid, sizeof(xf.grid));
    }
    if (xf.warpDrag >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        float2 delta = m - xf.dragStart;
        memcpy(xf.grid, xf.startGrid, sizeof(xf.grid));
        int d = xf.warpDrag;
        if (d == 16) {
            // Surface drag: move the control points by their influence so the grabbed point follows the mouse.
            float u = xf.warpUV.x, w = xf.warpUV.y;
            float bu[4] = {(1 - u) * (1 - u) * (1 - u), 3 * u * (1 - u) * (1 - u), 3 * u * u * (1 - u), u * u * u};
            float bv[4] = {(1 - w) * (1 - w) * (1 - w), 3 * w * (1 - w) * (1 - w), 3 * w * w * (1 - w), w * w * w};
            float sum2 = 0;
            for (int j = 0; j < 4; ++j) for (int i = 0; i < 4; ++i) sum2 += (bu[i] * bv[j]) * (bu[i] * bv[j]);
            for (int j = 0; j < 4; ++j)
                for (int i = 0; i < 4; ++i) xf.grid[j * 4 + i] = xf.startGrid[j * 4 + i] + delta * (bu[i] * bv[j] / std::max(sum2, 1e-6f));
        } else {
            xf.grid[d] = xf.startGrid[d] + delta;
            // Corners carry their two tangent handles (and the adjacent interior point).
            int ci = d % 4, cj = d / 4;
            if ((ci == 0 || ci == 3) && (cj == 0 || cj == 3)) {
                int ni = ci == 0 ? 1 : 2, nj = cj == 0 ? 1 : 2;
                for (int idx : {cj * 4 + ni, nj * 4 + ci, nj * 4 + ni}) xf.grid[idx] = xf.startGrid[idx] + delta;
            }
        }
        xf.dirty = true;
    }
    if (xf.warpDrag >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) xf.warpDrag = -1;
}

void App::WarpDrawOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    // Grid lines at thirds, outline thicker.
    const int S = 32;
    for (int k = 0; k <= 3; ++k)
        for (int dir = 0; dir < 2; ++dir) {
            ImVec2 pts[S + 1];
            for (int t = 0; t <= S; ++t) {
                float a = (float)t / S, b = k / 3.0f;
                pts[t] = toScreen(dir == 0 ? BezierPatch(xf.grid, a, b) : BezierPatch(xf.grid, b, a));
            }
            bool edge = k == 0 || k == 3;
            dl->AddPolyline(pts, S + 1, IM_COL32(0, 0, 0, 140), 0, edge ? 3.0f : 2.0f);
            dl->AddPolyline(pts, S + 1, edge ? ui::col::Accent2 : IM_COL32(255, 255, 255, 150), 0, edge ? 1.5f : 1.0f);
        }
    // Tangent handles of the corners.
    const int corners[4] = {0, 3, 15, 12};
    for (int c : corners) {
        int ci = c % 4, cj = c / 4;
        int hx = cj * 4 + (ci == 0 ? 1 : 2), hy = (cj == 0 ? 1 : 2) * 4 + ci;
        for (int h : {hx, hy}) {
            dl->AddLine(toScreen(xf.grid[c]), toScreen(xf.grid[h]), IM_COL32(255, 255, 255, 170), 1.0f);
            ImVec2 p = toScreen(xf.grid[h]);
            dl->AddCircleFilled(p, 4.5f, IM_COL32_WHITE);
            dl->AddCircle(p, 4.5f, IM_COL32(20, 20, 24, 255));
        }
    }
    for (int idx : {5, 6, 9, 10}) {
        ImVec2 p = toScreen(xf.grid[idx]);
        dl->AddCircle(p, 4.0f, IM_COL32(0, 0, 0, 180), 0, 3.0f);
        dl->AddCircle(p, 4.0f, IM_COL32_WHITE, 0, 1.5f);
    }
    for (int c : corners) {
        ImVec2 p = toScreen(xf.grid[c]);
        dl->AddRectFilled(ImVec2(p.x - 5, p.y - 5), ImVec2(p.x + 5, p.y + 5), IM_COL32_WHITE);
        dl->AddRect(ImVec2(p.x - 5, p.y - 5), ImVec2(p.x + 5, p.y + 5), IM_COL32(20, 20, 24, 255));
    }
}

// ------------------------------------------------------------------ edits using the selection

void App::FillOrErase(DocView& v, bool erase) {
    Document& doc = *v.doc;
    Id target = doc.PaintTarget();
    if (!target) { Status("Aucun calque de pixels actif"); return; }
    v.renderer.FillSelection(doc, *ctx, &v.history, target, erase ? 1 : 0, LinearColor(fg), 1.0f, erase ? "Effacer" : "Remplir");
    if (erase) RecordAction("erase");
    else {
        Json p = Json::Object(), c = Json::Array();
        c.Push(fg.x); c.Push(fg.y); c.Push(fg.z); c.Push(fg.w);
        p["color"] = c;
        RecordAction("fill", p);
    }
}

void App::LayerViaCopy(DocView& v, bool cut) {
    Document& doc = *v.doc;
    if (doc.Mode() != DocMode::Layers) return;
    Layer* l = doc.ActiveLayer();
    Id src = doc.PaintTarget();
    Surface* s = doc.GetSurface(src);
    if (!l || !s || doc.S().editMask) { Status("Sélectionnez un calque de pixels"); return; }
    std::string name = l->name + (cut ? " (coupé)" : " (copie)");
    v.history.BeginGroup();
    Edit e(doc, v.history, cut ? "Calque par couper" : "Calque par copier");
    Layer& nl = doc.AddPixelLayer(name);
    Id dst = nl.surface;
    doc.CancelPendingClear(dst);
    v.renderer.CopySelected(doc, *ctx, doc.GetSurface(src)->tex.get(), doc.GetSurface(dst)->tex.get());
    doc.TouchSurface(dst);
    Id selBefore = doc.S().selection;
    doc.S().selection = 0;  // Photoshop drops the selection after Ctrl+J
    e.Commit();
    if (cut) {
        doc.S().selection = selBefore;
        v.renderer.FillSelection(doc, *ctx, &v.history, src, 1, {}, 1.0f, "Couper");
        doc.S().selection = 0;
    }
    v.history.EndGroup();
}

void App::CopyToClipboard(DocView& v, bool cut) {
    Document& doc = *v.doc;
    Id src = doc.PaintTarget();
    Surface* s = doc.GetSurface(src);
    if (!s) { Status("Rien à copier"); return; }
    clipboard = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "Clipboard");
    v.renderer.CopySelected(doc, *ctx, s->tex.get(), clipboard.get());
    if (cut) v.renderer.FillSelection(doc, *ctx, &v.history, src, 1, {}, 1.0f, "Couper");
    Status(cut ? "Coupé" : "Copié");
}

void App::PasteClipboard(DocView& v) {
    if (!clipboard) return;
    Document& doc = *v.doc;
    if (clipboard->GetFormat() != SurfaceFormat(doc.GetPrecision())) {
        Status("Coller entre documents 16 et 32 bits n'est pas encore supporté");
        return;
    }
    Edit e(doc, v.history, "Coller");
    Id dst = 0;
    if (doc.Mode() == DocMode::Layers) dst = doc.AddPixelLayer("Collé").surface;
    else {
        Node& n = doc.AddNode(NodeKind::Surface, nodeEd.contextPos);
        n.name = "Collé";
        dst = n.surface;
    }
    Surface* s = doc.GetSurface(dst);
    Rect r{0, 0, (int)std::min(clipboard->Width(), doc.Width()), (int)std::min(clipboard->Height(), doc.Height())};
    if (clipboard->Width() != doc.Width() || clipboard->Height() != doc.Height()) {
        // Different size: clear first, then copy the overlapping part.
        v.renderer.Prepare(doc, *ctx);
    } else {
        doc.CancelPendingClear(dst);
    }
    ctx->CopyRegion(s->tex.get(), 0, 0, clipboard.get(), r);
    doc.TouchSurface(dst);
    doc.S().selection = 0;
    e.Commit();
}

void App::SelectFromLayer(DocView& v, const Layer& l, SelMode mode) {
    Document& doc = *v.doc;
    gpu::Texture* img = nullptr;
    if (l.kind == LayerKind::Pixel) {
        if (Surface* s = doc.GetSurface(l.surface)) img = s->tex.get();
    } else {
        for (auto& n : doc.S().graph.nodes)
            if (n.layer == l.id && n.kind == NodeKind::Effect) { img = v.renderer.NodeOutput(n.id); break; }
    }
    if (img) v.renderer.SelectFrom(doc, *ctx, v.history, img, SelSource::Alpha, mode, {}, 0, "Sélection depuis le calque");
}

void App::MaskFromSelection(DocView& v, Id layerId, bool always) {
    Document& doc = *v.doc;
    bool hasSel = doc.S().selection != 0;
    if (!always && !hasSel) return;
    Layer* l = doc.FindLayer(layerId);
    if (!l) return;
    if (!l->mask) doc.AddMask(layerId);
    if (hasSel) {
        ApplySelectionAsMask(v, layerId);
        doc.S().selection = 0;  // Photoshop consumes the selection
    }
    doc.S().editMask = !hasSel;
}

void App::ApplySelectionAsMask(DocView& v, Id layerId) {
    Document& doc = *v.doc;
    Layer* l = doc.FindLayer(layerId);
    if (!l || !l->mask || !doc.S().selection) return;
    v.renderer.SelectionToMask(doc, *ctx, l->mask, false);
}

// ------------------------------------------------------------------ menu & dialogs

void App::DrawSelectionMenu() {
    DocView* v = Active();
    if (!ImGui::BeginMenu("Sélection", v != nullptr)) return;
    ui::MenuFilter();
    Document& d = *v->doc;
    bool has = d.S().selection != 0;
    if (ImGui::MenuItem("Tout sélectionner", "Ctrl+A")) {
        v->renderer.SelectShape(d, *ctx, v->history, SelShape::All, {}, SelMode::Replace, false, 0, "Tout sélectionner");
        RecordSelect("all");
    }
    if (ImGui::MenuItem("Désélectionner", "Ctrl+D", false, has)) {
        v->renderer.Deselect(d, v->history);
        RecordSelect("none");
    }
    if (ImGui::MenuItem("Resélectionner", "Ctrl+Maj+D", false, d.S().lastSelection != 0 && !has)) v->renderer.Reselect(d, v->history);
    if (ImGui::MenuItem("Intervertir", "Ctrl+Maj+I")) {
        v->renderer.SelectionFilter(d, *ctx, v->history, 3, 0, "Intervertir");
        RecordSelect("invert");
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Sujet", nullptr, false, !subjectJob.active)) StartSelectSubject(*v, false);
    ui::HelpTooltip("Modèle de segmentation (rembg, via Python) : le sujet principal, contour affiné ensuite.");
    if (ImGui::MenuItem("Ciel", nullptr, false, !subjectJob.active)) StartSelectSubject(*v, true);
    if (subjectInstallPrompt || ImGui::GetIO().KeyShift)
        if (ImGui::MenuItem("Installer le modèle de sujet (rembg)...", nullptr, false, !subjectInstall.valid())) {
            InstallSubjectModel();
            subjectInstallPrompt = false;
        }
    if (ImGui::MenuItem("Plage de couleurs...")) {
        colorRange.open = true;
        colorRange.hash = 0;
        if (colorRange.samples.empty()) colorRange.samples.push_back({SrgbToLinear(fg.x), SrgbToLinear(fg.y), SrgbToLinear(fg.z), 1});
    }
    if (ImGui::MenuItem("Zone de netteté...", nullptr, false, !focusArea.open)) {
        focusArea = {};
        focusArea.open = true;
        focusArea.view = v;
        focusArea.live = true;  // first result right away
    }
    ui::HelpTooltip("Sélectionne les parties nettes de la photo (le sujet mis au point), d'après le détail local.");
    if (ImGui::MenuItem("Sélectionner et masquer...", "Alt+Ctrl+R", false, has)) {
        selectMask.open = true;
        selectMask.hash = 0;
    }
    if (ImGui::MenuItem("Pixels du calque actif", nullptr, false, d.Mode() == DocMode::Layers && d.ActiveLayer()))
        SelectFromLayer(*v, *d.ActiveLayer(), SelMode::Replace);
    if (ImGui::BeginMenu("Modifier", has)) {
        if (ImGui::MenuItem("Contour progressif...", "Maj+F6")) { selDialog = 1; selDialogValue = 8; }
        if (ImGui::MenuItem("Dilater...")) { selDialog = 2; selDialogValue = 4; }
        if (ImGui::MenuItem("Contracter...")) { selDialog = 3; selDialogValue = 4; }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Transformer la sélection", nullptr, false, v->doc->Selection() != nullptr)) TransformSelectionBegin(*v);
    if (ImGui::MenuItem("Créer un tracé depuis la sélection", nullptr, false, v->doc->Selection() != nullptr)) MakePathFromSelection(*v);
    if (ImGui::MenuItem("Transformation manuelle", "Ctrl+T")) TransformBegin(*v, false);
    ImGui::MenuItem("Mode Masque", "Q", &quickMask);
    ImGui::EndMenu();
}

void App::FocusAreaApply() {
    FocusArea& F = focusArea;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == F.view;
    if (!alive) { F = {}; return; }
    DocView& v = *F.view;
    Document& d = *v.doc;
    EffectInstance fx;
    fx.effectId = "focus_area";
    auto def = Effects().Find("focus_area");
    if (!def) { F.live = false; Status("Zone de netteté : effet introuvable"); return; }
    fx.params = def->DefaultParams();
    for (size_t i = 0; i < def->params.size(); ++i) {
        const std::string& id = def->params[i].id;
        if (id == "radius") fx.params[i].x = F.radius;
        else if (id == "threshold") fx.params[i].x = F.threshold;
        else if (id == "softness") fx.params[i].x = F.softness;
        else if (id == "invert") fx.params[i].x = F.invert ? 1.0f : 0.0f;
        else if (id == "fill") fx.params[i].x = F.fill;
    }
    gpu::Texture* img = v.renderer.EvaluateComplete(d, *ctx);
    if (!img) { F.live = false; return; }
    gpu::TexturePtr mask = v.renderer.ApplyEffect(d, *ctx, fx, img);
    if (!mask) return;  // (still compiling: tried again next frame)
    // One selection step for the dialog: the previous result is taken back when it is still the last step
    // (an edit or an undo meanwhile keeps it, the new result comes on top).
    if (F.applied && v.history.Cursor() == F.appliedCursor && v.history.Count() == F.appliedCursor) {
        if (xf.active && xf.view == &v) TransformCancel(v);
        v.history.Undo(d, *ctx);
        v.history.TruncateTo(v.history.Cursor());
    }
    v.renderer.SelectFrom(d, *ctx, v.history, mask.get(), SelSource::Mask, SelMode::Replace, {}, 0, "Zone de netteté");
    F.applied = true;
    F.appliedCursor = v.history.Cursor();
    F.live = false;
}

void App::DrawFocusAreaWindow() {
    FocusArea& F = focusArea;
    if (!F.open) return;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == F.view;
    if (!alive) { F = {}; return; }
    ImGui::SetNextWindowSize(ImVec2(340 * dpi, 0), ImGuiCond_Always);
    bool open = true;
    if (ImGui::Begin("Zone de netteté", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("Les parties nettes de l'image sont sélectionnées (le résultat s'affiche en direct).");
        F.live |= ImGui::SliderFloat("Rayon d'analyse (px)", &F.radius, 1, 48, "%.0f");
        F.live |= ImGui::SliderFloat("Seuil de netteté", &F.threshold, 0.0f, 0.3f, "%.3f");
        ui::HelpTooltip("Plus haut : seules les zones très détaillées restent ; plus bas : le grain et le bruit entrent aussi.");
        F.live |= ImGui::SliderFloat("Transition", &F.softness, 0.001f, 0.1f, "%.3f");
        F.live |= ImGui::SliderFloat("Combler les zones unies (px)", &F.fill, 0, 96, "%.0f");
        ui::HelpTooltip("Remplit les surfaces sans détail entourées de contours nets (un mur uni du sujet).");
        F.live |= ImGui::Checkbox("Inverser (sélectionner le flou)", &F.invert);
        if (F.closeWhenDone) ImGui::TextDisabled("Calcul en cours...");
        if (ImGui::Button("OK", ImVec2(100 * dpi, 0))) F.closeWhenDone = true;  // (closed once the last change is applied)
        ImGui::SameLine();
        if (ImGui::Button("Annuler", ImVec2(100 * dpi, 0))) open = false;
    }
    ImGui::End();
    if (!open) {
        // Cancel: the dialog's selection step taken back if it is still the last one.
        DocView& v = *F.view;
        if (F.applied && v.history.Cursor() == F.appliedCursor && v.history.Count() == F.appliedCursor) {
            if (xf.active && xf.view == &v) TransformCancel(v);
            v.history.Undo(*v.doc, *ctx);
            v.history.TruncateTo(v.history.Cursor());
        }
        F = {};
        return;
    }
    if (F.live && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) FocusAreaApply();  // (on release while dragging a slider)
    if (F.closeWhenDone && !F.live) F = {};
}

void App::DrawSelectionDialog() {
    static const char* titles[] = {"", "Contour progressif", "Dilater la sélection", "Contracter la sélection", "Plage de couleurs", "Contour"};
    if (selDialog) {
        ImGui::OpenPopup("##seldialog");
    }
    if (!ImGui::BeginPopupModal("##seldialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) return;
    DocView* v = Active();
    ImGui::TextUnformatted(titles[std::clamp(selDialog, 0, 5)]);
    ImGui::Separator();
    if (selDialog == 5) {
        ImGui::SliderFloat("Épaisseur (px)", &strokeWidth, 1, 250, "%.0f", ImGuiSliderFlags_Logarithmic);
        ImGui::Combo("Position", &strokePosition, "Intérieur\0Centre\0Extérieur\0");
        ImGui::SliderFloat("Opacité", &strokeOpacity, 0, 1, "%.2f");
        ImGui::TextDisabled("Couleur : premier plan");
    } else if (selDialog == 4) ImGui::SliderFloat("Tolérance", &selDialogValue, 0, 255, "%.0f");
    else ImGui::SliderFloat("Rayon (px)", &selDialogValue, 0.5f, 250, "%.1f", ImGuiSliderFlags_Logarithmic);
    if (ImGui::Button("OK", ImVec2(100 * dpi, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (v) {
            Document& d = *v->doc;
            if (selDialog == 1) v->renderer.SelectionFilter(d, *ctx, v->history, 0, selDialogValue, "Contour progressif");
            else if (selDialog == 2) v->renderer.SelectionFilter(d, *ctx, v->history, 1, selDialogValue, "Dilater");
            else if (selDialog == 3) v->renderer.SelectionFilter(d, *ctx, v->history, 2, selDialogValue, "Contracter");
            else if (selDialog == 5)
                v->renderer.StrokeSelection(d, *ctx, v->history, d.PaintTarget(), strokeWidth, strokePosition, LinearColor(fg), strokeOpacity);
            else if (selDialog == 4)
                v->renderer.SelectFrom(d, *ctx, v->history, v->renderer.EvaluateComplete(d, *ctx), SelSource::ColorRange, ModeFromModifiers(), fg, selDialogValue / 255.0f, "Plage de couleurs");
        }
        selDialog = 0;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(100 * dpi, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        selDialog = 0;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}  // namespace sx
