// File > Automate > Crop and Straighten Photos (like Photoshop's): the photos of a scan, laid on a plain background,
// found in the merged image (background color from the border, connected shapes, smallest rotated rectangle around
// each), then each one straightened and cropped into a new document.
#include "ui/App.h"
#include "render/Dispatch.h"

#include <algorithm>
#include <cmath>

namespace sx {

std::vector<std::array<float2, 4>> App::FindPhotos(const u8* px, u32 w, u32 h) {
    std::vector<std::array<float2, 4>> out;
    if (!px || w < 16 || h < 16) return out;
    // A reduced grid (each cell the average of k x k pixels): enough to find the shapes, fast.
    const u32 k = std::max(1u, (std::max(w, h) + 799) / 800);
    const u32 sw = w / k, sh = h / k;
    std::vector<float> col((size_t)sw * sh * 3);
    for (u32 y = 0; y < sh; ++y)
        for (u32 x = 0; x < sw; ++x) {
            float acc[3] = {0, 0, 0};
            for (u32 j = 0; j < k; ++j)
                for (u32 i = 0; i < k; ++i) {
                    const u8* p = px + ((size_t)(y * k + j) * w + (x * k + i)) * 4;
                    // (transparent pixels count as white paper)
                    const float a = p[3] / 255.0f;
                    for (int c = 0; c < 3; ++c) acc[c] += p[c] * a + 255.0f * (1 - a);
                }
            for (int c = 0; c < 3; ++c) col[((size_t)y * sw + x) * 3 + c] = acc[c] / (k * k);
        }
    // The background: the median color of the border.
    float bg[3];
    for (int c = 0; c < 3; ++c) {
        std::vector<float> v;
        for (u32 x = 0; x < sw; ++x) { v.push_back(col[(size_t)x * 3 + c]); v.push_back(col[((size_t)(sh - 1) * sw + x) * 3 + c]); }
        for (u32 y = 0; y < sh; ++y) { v.push_back(col[((size_t)y * sw) * 3 + c]); v.push_back(col[((size_t)y * sw + sw - 1) * 3 + c]); }
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        bg[c] = v[v.size() / 2];
    }
    // Foreground: clearly away from the background, gaps of a cell closed (dilate then erode).
    std::vector<u8> fg((size_t)sw * sh), tmp((size_t)sw * sh);
    for (size_t i = 0; i < fg.size(); ++i) {
        float d = 0;
        for (int c = 0; c < 3; ++c) d = std::max(d, std::abs(col[i * 3 + c] - bg[c]));
        fg[i] = d > 26.0f;
    }
    auto morph = [&](bool dilate) {
        for (u32 y = 0; y < sh; ++y)
            for (u32 x = 0; x < sw; ++x) {
                bool r = !dilate;
                for (int j = -1; j <= 1; ++j)
                    for (int i = -1; i <= 1; ++i) {
                        const int xx = std::clamp((int)x + i, 0, (int)sw - 1), yy = std::clamp((int)y + j, 0, (int)sh - 1);
                        const bool f = fg[(size_t)yy * sw + xx] != 0;
                        r = dilate ? (r || f) : (r && f);
                    }
                tmp[(size_t)y * sw + x] = r;
            }
        fg.swap(tmp);
    };
    morph(true);
    morph(false);
    // Connected shapes (4-neighbours); the small ones (dust, scratches) left out.
    std::vector<int> label((size_t)sw * sh, -1);
    const size_t minArea = std::max<size_t>(64, (size_t)sw * sh / 200);
    struct Found { std::array<float2, 4> q; float top, left; };
    std::vector<Found> found;
    std::vector<u32> stack;
    for (u32 start = 0; start < sw * sh; ++start) {
        if (!fg[start] || label[start] >= 0) continue;
        std::vector<u32> cells;
        stack.assign(1, start);
        label[start] = 1;
        while (!stack.empty()) {
            const u32 c = stack.back();
            stack.pop_back();
            cells.push_back(c);
            const u32 x = c % sw, y = c / sw;
            const u32 nb[4] = {x > 0 ? c - 1 : c, x + 1 < sw ? c + 1 : c, y > 0 ? c - sw : c, y + 1 < sh ? c + sw : c};
            for (u32 n : nb)
                if (fg[n] && label[n] < 0) { label[n] = 1; stack.push_back(n); }
        }
        if (cells.size() < minArea) continue;
        // The cells' corners (document pixels) on the shape's edge, then their convex hull.
        std::vector<float2> pts;
        for (u32 c : cells) {
            const u32 x = c % sw, y = c / sw;
            const bool edge = x == 0 || y == 0 || x + 1 == sw || y + 1 == sh || !fg[c - 1] || !fg[c + 1] || !fg[c - sw] || !fg[c + sw];
            if (!edge) continue;
            const float x0 = (float)(x * k), y0 = (float)(y * k), x1 = (float)((x + 1) * k), y1 = (float)((y + 1) * k);
            pts.push_back({x0, y0}); pts.push_back({x1, y0}); pts.push_back({x1, y1}); pts.push_back({x0, y1});
        }
        std::sort(pts.begin(), pts.end(), [](float2 a, float2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
        pts.erase(std::unique(pts.begin(), pts.end(), [](float2 a, float2 b) { return a.x == b.x && a.y == b.y; }), pts.end());
        if (pts.size() < 3) continue;
        auto cross = [](float2 o, float2 a, float2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
        std::vector<float2> hull(pts.size() * 2);
        size_t n = 0;
        for (size_t i = 0; i < pts.size(); ++i) {
            while (n >= 2 && cross(hull[n - 2], hull[n - 1], pts[i]) <= 0) --n;
            hull[n++] = pts[i];
        }
        for (size_t i = pts.size() - 1, t = n + 1; i-- > 0;) {
            while (n >= t && cross(hull[n - 2], hull[n - 1], pts[i]) <= 0) --n;
            hull[n++] = pts[i];
        }
        hull.resize(n - 1);
        // The smallest rectangle around the hull has a side on one of its edges.
        float best = 1e30f, bu0 = 0, bu1 = 0, bv0 = 0, bv1 = 0;
        float2 bu = {1, 0};
        for (size_t i = 0; i < hull.size(); ++i) {
            float2 e = hull[(i + 1) % hull.size()] - hull[i];
            const float len = Length(e);
            if (len < 1e-3f) continue;
            const float2 u = e * (1.0f / len), vv = {-u.y, u.x};
            float u0 = 1e30f, u1 = -1e30f, v0 = 1e30f, v1 = -1e30f;
            for (float2 p : hull) {
                const float a = p.x * u.x + p.y * u.y, b = p.x * vv.x + p.y * vv.y;
                u0 = std::min(u0, a); u1 = std::max(u1, a); v0 = std::min(v0, b); v1 = std::max(v1, b);
            }
            if ((u1 - u0) * (v1 - v0) < best) { best = (u1 - u0) * (v1 - v0); bu = u; bu0 = u0; bu1 = u1; bv0 = v0; bv1 = v1; }
        }
        // Upright axes: u the one nearest +x (the photo turned by less than 45 degrees), v pointing down.
        float2 u = bu, vv = {-bu.y, bu.x};
        float u0 = bu0, u1 = bu1, v0 = bv0, v1 = bv1;
        for (int turn = 0; turn < 4 && !(u.x >= std::abs(u.y)); ++turn) {
            // (rotate the frame by 90 degrees: u <- v, v <- -u)
            const float2 nu = vv, nv = {-u.x, -u.y};
            const float nu0 = v0, nu1 = v1, nv0 = -u1, nv1 = -u0;
            u = nu; vv = nv; u0 = nu0; u1 = nu1; v0 = nv0; v1 = nv1;
        }
        if (vv.y < 0) { vv = {-vv.x, -vv.y}; const float a = -v1, b = -v0; v0 = a; v1 = b; }
        // A little inside: the cells on the edge are part background.
        const float inset = 0.75f * k + 1;
        u0 += inset; u1 -= inset; v0 += inset; v1 -= inset;
        if (u1 - u0 < 16 || v1 - v0 < 16) continue;
        auto at = [&](float a, float b) { return u * a + vv * b; };
        Found f;
        f.q = {at(u0, v0), at(u1, v0), at(u1, v1), at(u0, v1)};
        f.top = std::min({f.q[0].y, f.q[1].y, f.q[2].y, f.q[3].y});
        f.left = std::min({f.q[0].x, f.q[1].x, f.q[2].x, f.q[3].x});
        found.push_back(f);
    }
    // Reading order: rows from the top (a photo starting within a fifth of the image's height is on the same row),
    // then from the left.
    std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.top < b.top; });
    for (size_t i = 0; i < found.size();) {
        size_t j = i + 1;
        while (j < found.size() && found[j].top - found[i].top < h / 5.0f) ++j;  // (the same row)
        std::sort(found.begin() + i, found.begin() + j, [](const Found& a, const Found& b) { return a.left < b.left; });
        i = j;
    }
    for (auto& f : found) out.push_back(f.q);
    return out;
}

void App::StartCropPhotos(DocView& v) {
    if (cropPhotos.rb) return;
    Document& d = *v.doc;
    Prepare(v);
    gpu::Texture* img = v.renderer.EvaluateComplete(d, *ctx);
    if (!img) return;
    cropPhotos.image = gpu::Dev().CreateTexture(img->Width(), img->Height(), img->GetFormat(), "CropPhotos");
    if (!cropPhotos.image) { Status("Recadrer et redresser : mémoire graphique insuffisante"); return; }
    ctx->CopyTexture(cropPhotos.image.get(), img);
    cropPhotos.view = &v;
    cropPhotos.precision = d.GetPrecision();
    cropPhotos.rb = v.renderer.ExportRGBA8(*ctx, cropPhotos.image.get());
    if (!cropPhotos.rb) { cropPhotos = {}; Status("Recadrer et redresser : mémoire insuffisante"); return; }
    Status("Recadrer et redresser les photos : recherche...");
}

void App::CropPhotosTick() {
    auto& J = cropPhotos;
    if (!J.rb || !J.rb->ready) return;
    auto rb = std::move(J.rb);
    auto image = std::move(J.image);
    const Precision precision = J.precision;
    J = {};
    const auto frames = FindPhotos(rb->data.data(), rb->width, rb->height);
    if (frames.empty()) { Status("Recadrer et redresser : aucune photo trouvée sur un fond uni"); return; }
    const u32 W = image->Width(), H = image->Height();
    auto* k = Kernel("resample_persp.slang");
    if (!k) return;
    struct PerspRoot {  // (resample_persp.slang)
        u32 dw, dh, sw, sh;
        float h0[3];
        u32 interp;
        float h1[3];
        u32 pad0;
        float h2[3];
        u32 pad1;
    };
    int made = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
        u32 w = 0, h = 0;
        float m[9];
        if (!QuadToRectMatrix(frames[i].data(), w, h, m) || w > 16384 || h > 16384) continue;
        // A document of the photo's size, its layer sampled straight from the scan (straightened, bicubic).
        NewDocument(w, h, precision, DocMode::Layers, 3, "Photo " + std::to_string(made + 1));
        DocView& nv = *docs.back();
        Document& nd = *nv.doc;
        const Id surf = nd.AddPixelLayer("Photo").surface;
        nd.SyncGraph();
        Prepare(nv);
        Surface* s = nd.GetSurface(surf);
        if (!s || !s->tex) {  // (no memory: the empty document goes away)
            docs.pop_back();
            activeDoc = std::min(activeDoc, (int)docs.size() - 1);
            continue;
        }
        gpu::DispatchDesc dd;
        dd.pipeline = k;
        dd.srv[0] = image.get();
        dd.uav[0] = s->tex.get();
        SetConstants(dd, PerspRoot{w, h, W, H, {m[0], m[1], m[2]}, 2, {m[3], m[4], m[5]}, 0, {m[6], m[7], m[8]}, 0});
        Dispatch2D(*ctx, dd, w, h);
        nd.TouchSurface(surf);
        nv.history.Clear();  // (a fresh document: nothing to undo)
        nd.dirty = true;
        nv.fitPending = true;
        ++made;
    }
    if (!made) { Status("Recadrer et redresser : mémoire insuffisante pour les documents"); return; }
    Status(std::to_string(made) + (made > 1 ? " photos redressées, chacune dans son document" : " photo redressée dans un document"));
}

}  // namespace sx
