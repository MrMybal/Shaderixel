// Magnetic lasso: the outline snaps to the strongest edges of the image
// (live-wire: shortest path on an edge cost map between the last anchor and
// the cursor). Anchors are fixed automatically every few pixels and on click;
// double-click / Enter / click on the start closes, Backspace removes the last
// anchor, Escape cancels.
#include "ui/App.h"

#include <cmath>
#include <queue>

namespace sx {

void App::MagneticBuildCost() {
    auto& M = magLasso;
    if (!M.rb || !M.rb->ready) return;
    const int W = (int)M.rb->width, H = (int)M.rb->height;
    // Cost grid at most ~16 Mpx (large documents use a reduced grid).
    int scale = 1;
    while ((long long)(W / scale) * (H / scale) > (16ll << 20)) ++scale;
    M.scale = scale;
    M.w = W / scale;
    M.h = H / scale;
    std::vector<float> luma((size_t)M.w * M.h);
    const u8* px = M.rb->data.data();
    for (int y = 0; y < M.h; ++y)
        for (int x = 0; x < M.w; ++x) {
            const u8* p = px + ((size_t)(y * scale) * W + x * scale) * 4;
            luma[(size_t)y * M.w + x] = (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / 255.0f * (p[3] / 255.0f);
        }
    // Sobel magnitude -> cost (low on edges).
    M.cost.assign((size_t)M.w * M.h, 1.0f);
    float gmax = 1e-4f;
    std::vector<float> g((size_t)M.w * M.h, 0.0f);
    auto L = [&](int x, int y) { return luma[(size_t)std::clamp(y, 0, M.h - 1) * M.w + std::clamp(x, 0, M.w - 1)]; };
    for (int y = 0; y < M.h; ++y)
        for (int x = 0; x < M.w; ++x) {
            float gx = (L(x + 1, y - 1) + 2 * L(x + 1, y) + L(x + 1, y + 1)) - (L(x - 1, y - 1) + 2 * L(x - 1, y) + L(x - 1, y + 1));
            float gy = (L(x - 1, y + 1) + 2 * L(x, y + 1) + L(x + 1, y + 1)) - (L(x - 1, y - 1) + 2 * L(x, y - 1) + L(x + 1, y - 1));
            float m = std::sqrt(gx * gx + gy * gy);
            g[(size_t)y * M.w + x] = m;
            gmax = std::max(gmax, m);
        }
    for (size_t i = 0; i < g.size(); ++i) M.cost[i] = 0.02f + std::pow(1.0f - std::min(g[i] / (gmax * 0.6f), 1.0f), 2.0f);
    M.rb.reset();
}

std::vector<float2> App::MagneticPath(float2 from, float2 to) {
    // Shortest path on the cost grid inside the box around both points (document coordinates).
    auto& M = magLasso;
    std::vector<float2> out;
    if (M.cost.empty()) return {from, to};
    const float s = (float)M.scale;
    int ax = std::clamp((int)(from.x / s), 0, M.w - 1), ay = std::clamp((int)(from.y / s), 0, M.h - 1);
    int bx = std::clamp((int)(to.x / s), 0, M.w - 1), by = std::clamp((int)(to.y / s), 0, M.h - 1);
    const int margin = 24;
    int x0 = std::max(0, std::min(ax, bx) - margin), y0 = std::max(0, std::min(ay, by) - margin);
    int x1 = std::min(M.w - 1, std::max(ax, bx) + margin), y1 = std::min(M.h - 1, std::max(ay, by) + margin);
    int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
    if ((long long)bw * bh > 400000) return {from, to};  // too far: straight segment (anchors keep it short)
    std::vector<float> dist((size_t)bw * bh, 1e30f);
    std::vector<int> prev((size_t)bw * bh, -1);
    using Item = std::pair<float, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> q;
    int start = (ay - y0) * bw + (ax - x0), goal = (by - y0) * bw + (bx - x0);
    dist[start] = 0;
    q.push({0, start});
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static const float dl[8] = {1, 1, 1, 1, 1.4142f, 1.4142f, 1.4142f, 1.4142f};
    while (!q.empty()) {
        auto [d, i] = q.top();
        q.pop();
        if (d > dist[i]) continue;
        if (i == goal) break;
        int x = i % bw, y = i / bw;
        for (int k = 0; k < 8; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || ny < 0 || nx >= bw || ny >= bh) continue;
            int j = ny * bw + nx;
            float nd = d + M.cost[(size_t)(ny + y0) * M.w + (nx + x0)] * dl[k];
            if (nd < dist[j]) {
                dist[j] = nd;
                prev[j] = i;
                q.push({nd, j});
            }
        }
    }
    for (int i = goal; i >= 0; i = prev[i]) {
        out.push_back({((i % bw) + x0 + 0.5f) * s, ((i / bw) + y0 + 0.5f) * s});
        if (i == start) break;
    }
    std::reverse(out.begin(), out.end());
    if (out.empty()) out = {from, to};
    return out;
}

void App::MagneticLassoInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    auto& M = magLasso;
    ImGuiIO& io = ImGui::GetIO();
    float2 dp = ScreenToDoc(v, canvasMin, io.MousePos);
    Document& doc = *v.doc;
    dp = {std::clamp(dp.x, 0.0f, (float)doc.Width() - 1), std::clamp(dp.y, 0.0f, (float)doc.Height() - 1)};
    bool click = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    if (M.rb) MagneticBuildCost();
    if (click && !polyActive) {
        polyActive = true;
        selDragMode = ModeFromModifiers();
        lassoPts = {dp};
        M.anchor = dp;
        M.live.clear();
        M.cost.clear();
        if (v.lastImage) M.rb = v.renderer.ExportRGBA8(*ctx, v.lastImage, true);
        return;
    }
    if (!polyActive) return;
    // Live path from the last anchor to the cursor.
    if (Length(dp - M.lastCursor) >= 1.0f || M.live.empty()) {
        M.lastCursor = dp;
        M.live = MagneticPath(M.anchor, dp);
        // Automatic anchors: freeze the beginning of a long path.
        float len = 0;
        for (size_t i = 1; i < M.live.size(); ++i) len += Length(M.live[i] - M.live[i - 1]);
        if (len > M.anchorEvery && M.live.size() > 8) {
            size_t keep = M.live.size() * 2 / 3;
            for (size_t i = 1; i <= keep; ++i) lassoPts.push_back(M.live[i]);
            M.anchor = M.live[keep];
            M.anchors.push_back(lassoPts.size());
            M.live = MagneticPath(M.anchor, dp);
        }
    }
    if (click) {
        float closeDist = Length(dp - lassoPts.front()) * v.zoom;
        bool dbl = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        for (size_t i = 1; i < M.live.size(); ++i) lassoPts.push_back(M.live[i]);
        M.anchor = dp;
        M.anchors.push_back(lassoPts.size());
        M.live.clear();
        if ((lassoPts.size() >= 3 && closeDist < 10.0f) || dbl) {
            // Close along the edges back to the start.
            auto back = MagneticPath(lassoPts.back(), lassoPts.front());
            for (size_t i = 1; i + 1 < back.size(); ++i) lassoPts.push_back(back[i]);
            M.anchors.clear();
            FinishPolygon(v);
        }
    }
}

void App::MagneticLassoUndoAnchor() {
    auto& M = magLasso;
    if (M.anchors.empty()) {
        polyActive = false;
        lassoPts.clear();
        M.live.clear();
        return;
    }
    M.anchors.pop_back();
    size_t keep = M.anchors.empty() ? 1 : M.anchors.back();
    if (keep < lassoPts.size()) lassoPts.resize(keep);
    M.anchor = lassoPts.back();
    M.live.clear();
}

}  // namespace sx
