// Quick Selection tool: paint over a region, the selection grows to its edges.
//
// Geodesic growth (Dijkstra) from the painted pixels over a region of interest
// around the stroke. Stepping to a neighbor costs a small base amount (so
// homogeneous areas fill far) plus an edge term (color gradient of a lightly
// blurred image) and a region term (distance to the painted colors beyond
// their natural spread). Pixels within distance 1 are selected; the coverage
// is softened near the threshold. Runs on a cached RGBA8 readback.
#include "ui/App.h"

#include <cmath>
#include <queue>

namespace sx {

namespace {
struct Rgb { float r, g, b; };

Rgb operator-(Rgb a, Rgb b) { return {a.r - b.r, a.g - b.g, a.b - b.b}; }
float Len(Rgb a) { return std::sqrt(a.r * a.r + a.g * a.g + a.b * a.b); }

// Returns the selected coverage over `roi` (row-major, roi.Width() * roi.Height()).
void QuickSelectMask(const std::vector<u8>& img, int W, int H, const std::vector<float2>& pts, float radius, float tolerance,
                     Rect& roi, std::vector<u8>& cov) {
    roi = {};
    for (auto& p : pts) roi.Merge({(int)std::floor(p.x - radius), (int)std::floor(p.y - radius), (int)std::ceil(p.x + radius) + 1, (int)std::ceil(p.y + radius) + 1});
    int grow = (int)std::max(radius * 8.0f, 512.0f);  // growth is bounded by the cost threshold anyway
    roi = Rect{roi.x0 - grow, roi.y0 - grow, roi.x1 + grow, roi.y1 + grow}.Clipped(W, H);
    if (roi.Empty()) return;
    const int w = roi.Width(), h = roi.Height();
    const size_t n = (size_t)w * h;
    // Colors (alpha-weighted onto black) with a 3x3 box blur against noise.
    std::vector<Rgb> raw(n), col(n);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const u8* p = &img[((size_t)(roi.y0 + y) * W + roi.x0 + x) * 4];
            float a = p[3] / 255.0f;
            raw[(size_t)y * w + x] = {p[0] * a, p[1] * a, p[2] * a};
        }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            Rgb s = {0, 0, 0};
            int c = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                    const Rgb& q = raw[(size_t)yy * w + xx];
                    s.r += q.r; s.g += q.g; s.b += q.b;
                    ++c;
                }
            col[(size_t)y * w + x] = {s.r / c, s.g / c, s.b / c};
        }
    // Seeds: discs along the stroke.
    std::vector<float> dist(n, 1e9f);
    using Item = std::pair<float, u32>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    Rgb mean = {0, 0, 0};
    size_t seeds = 0;
    auto seedDisc = [&](float2 c) {
        int x0 = std::max(0, (int)std::floor(c.x - radius) - roi.x0), x1 = std::min(w - 1, (int)std::ceil(c.x + radius) - roi.x0);
        int y0 = std::max(0, (int)std::floor(c.y - radius) - roi.y0), y1 = std::min(h - 1, (int)std::ceil(c.y + radius) - roi.y0);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                float dx = x + roi.x0 + 0.5f - c.x, dy = y + roi.y0 + 0.5f - c.y;
                if (dx * dx + dy * dy > radius * radius) continue;
                size_t i = (size_t)y * w + x;
                if (dist[i] == 0) continue;
                dist[i] = 0;
                pq.push({0.0f, (u32)i});
                mean.r += col[i].r; mean.g += col[i].g; mean.b += col[i].b;
                ++seeds;
            }
    };
    for (size_t k = 0; k < pts.size(); ++k) {
        seedDisc(pts[k]);
        if (k + 1 < pts.size()) {
            float2 d = pts[k + 1] - pts[k];
            int steps = (int)(Length(d) / std::max(radius * 0.5f, 1.0f));
            for (int s = 1; s < steps; ++s) seedDisc(pts[k] + d * ((float)s / steps));
        }
    }
    if (!seeds) return;
    mean = {mean.r / seeds, mean.g / seeds, mean.b / seeds};
    double spread = 0;
    for (size_t i = 0; i < n; ++i) if (dist[i] == 0) spread += Len(col[i] - mean) * Len(col[i] - mean);
    float allowed = (float)std::sqrt(spread / seeds) * 1.5f + tolerance * 0.35f;
    const float invTol = 1.0f / std::max(tolerance, 1.0f);
    // Dijkstra, 8-connected.
    static const int nx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, ny[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!pq.empty()) {
        auto [d, i] = pq.top();
        pq.pop();
        if (d > dist[i] || d > 1.0f) continue;
        int x = (int)(i % w), y = (int)(i / w);
        for (int k = 0; k < 8; ++k) {
            int xx = x + nx[k], yy = y + ny[k];
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            size_t j = (size_t)yy * w + xx;
            float g = Len(col[j] - col[i]) * invTol;
            float r = std::max(0.0f, Len(col[j] - mean) - allowed) * invTol;
            float step = (k < 4 ? 1.0f : 1.4142f) * 0.0015f + 2.0f * g * g + 3.0f * r * r;
            float nd = d + step;
            if (nd < dist[j] && nd <= 1.2f) {
                dist[j] = nd;
                pq.push({nd, (u32)j});
            }
        }
    }
    cov.assign(n, 0);
    for (size_t i = 0; i < n; ++i) cov[i] = (u8)std::lround(255.0f * std::clamp((1.2f - dist[i]) / 0.4f, 0.0f, 1.0f));
}
}  // namespace

void App::QuickSelectInput(DocView& v, ImVec2 canvasMin, bool hovered, const std::vector<PointerSample>& samples) {
    ImGuiIO& io = ImGui::GetIO();
    Document& doc = *v.doc;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !qsel.pending) {
        qsel.painting = true;
        qsel.pts.clear();
        qsel.mode = io.KeyAlt ? SelMode::Subtract : (doc.Selection() ? SelMode::Add : SelMode::Replace);
        // Image to analyze: merged result or the active layer (cached while unchanged).
        gpu::Texture* src = nullptr;
        Id source = 0;
        if (!qselMerged) {
            source = doc.PaintTarget();
            if (Surface* s = doc.GetSurface(source)) src = s->tex.get();
        }
        u64 version = source ? (doc.GetSurface(source) ? doc.GetSurface(source)->version : 0) : v.renderer.ImageVersion();
        if (!src) { src = v.renderer.EvaluateComplete(doc, *ctx); source = 0; }
        if (!qsel.rb || qsel.view != &v || qsel.source != source || qsel.version != version) {
            qsel.rb = src ? v.renderer.ExportRGBA8(*ctx, src) : nullptr;
            qsel.view = &v;
            qsel.source = source;
            qsel.version = version;
        }
    }
    if (!qsel.painting) return;
    for (auto& s : samples) qsel.pts.push_back(ScreenToDoc(v, canvasMin, ImVec2(s.x, s.y)));
    qsel.pts.push_back(ScreenToDoc(v, canvasMin, io.MousePos));
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        qsel.painting = false;
        qsel.pending = true;  // computed once the readback is available
    }
}

void App::UpdateQuickSelect(DocView& v) {
    if (!qsel.pending || qsel.view != &v || !qsel.rb || !qsel.rb->ready) return;
    qsel.pending = false;
    Document& doc = *v.doc;
    const auto& rb = *qsel.rb;
    if (rb.width != doc.Width() || rb.height != doc.Height() || qsel.pts.empty()) return;
    Rect roi;
    std::vector<u8> cov;
    QuickSelectMask(rb.data, (int)rb.width, (int)rb.height, qsel.pts, qselSize * 0.5f, qselTolerance, roi, cov);
    qsel.pts.clear();
    if (roi.Empty() || cov.empty()) return;
    v.renderer.SelectCoverage(doc, *ctx, v.history, cov, roi, qsel.mode, 0, "Sélection rapide");
}

void App::DrawQuickSelectOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    if (!qsel.painting || qsel.view != &v) return;
    float r = std::max(qselSize * 0.5f * v.zoom, 1.0f);
    for (auto& p : qsel.pts) {
        ImVec2 s(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom);
        dl->AddCircleFilled(s, r, IM_COL32(120, 160, 255, 40));
    }
}

}  // namespace sx
