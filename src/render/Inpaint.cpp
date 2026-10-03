#include "render/Inpaint.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <thread>

namespace sx {

namespace {
constexpr int kR = 3;  // patch radius (7x7)

struct Level {
    int w = 0, h = 0;
    std::vector<float> c;     // 4 per pixel
    std::vector<u8> hole;     // 1 = unknown
    std::vector<u8> source;   // 1 = valid patch center for sources (patch fully known and inside)
    std::vector<int> nnf;     // per pixel: source index (y * w + x), -1 = none
    std::vector<float> dist;
};

template <class F>
void ParallelRows(int h, F&& f) {
    int n = std::max(1u, std::thread::hardware_concurrency());
    n = std::min(n, std::max(1, h / 8));
    std::vector<std::thread> t;
    std::atomic<int> next{0};
    for (int i = 0; i < n; ++i)
        t.emplace_back([&] {
            for (int y; (y = next.fetch_add(1)) < h;) f(y);
        });
    for (auto& th : t) th.join();
}

void Downsample(const Level& a, Level& b) {
    b.w = std::max(1, a.w / 2);
    b.h = std::max(1, a.h / 2);
    b.c.assign((size_t)b.w * b.h * 4, 0);
    b.hole.assign((size_t)b.w * b.h, 0);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            float acc[4] = {0, 0, 0, 0};
            int n = 0;
            bool anyHole = false;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    int sx = std::min(x * 2 + dx, a.w - 1), sy = std::min(y * 2 + dy, a.h - 1);
                    size_t i = (size_t)sy * a.w + sx;
                    if (a.hole[i]) { anyHole = true; continue; }
                    for (int k = 0; k < 4; ++k) acc[k] += a.c[i * 4 + k];
                    ++n;
                }
            size_t j = (size_t)y * b.w + x;
            b.hole[j] = anyHole ? 1 : 0;
            if (n) for (int k = 0; k < 4; ++k) b.c[j * 4 + k] = acc[k] / n;
        }
}

void ComputeSources(Level& L) {
    L.source.assign((size_t)L.w * L.h, 0);
    // Valid center: the whole patch is inside the image and known.
    for (int y = kR; y < L.h - kR; ++y)
        for (int x = kR; x < L.w - kR; ++x) {
            bool ok = true;
            for (int dy = -kR; dy <= kR && ok; ++dy)
                for (int dx = -kR; dx <= kR && ok; ++dx)
                    if (L.hole[(size_t)(y + dy) * L.w + x + dx]) ok = false;
            L.source[(size_t)y * L.w + x] = ok ? 1 : 0;
        }
}

void InitHoleByDiffusion(Level& L) {
    // Rough initial guess: repeated averaging of known / guessed neighbors.
    std::vector<u8> known(L.hole.size());
    for (size_t i = 0; i < known.size(); ++i) known[i] = !L.hole[i];
    for (int iter = 0; iter < L.w + L.h; ++iter) {
        bool changed = false;
        std::vector<u8> nextKnown = known;
        for (int y = 0; y < L.h; ++y)
            for (int x = 0; x < L.w; ++x) {
                size_t i = (size_t)y * L.w + x;
                if (known[i]) continue;
                float acc[4] = {0, 0, 0, 0};
                int n = 0;
                const int nx[4] = {x - 1, x + 1, x, x}, ny[4] = {y, y, y - 1, y + 1};
                for (int k = 0; k < 4; ++k) {
                    if (nx[k] < 0 || ny[k] < 0 || nx[k] >= L.w || ny[k] >= L.h) continue;
                    size_t j = (size_t)ny[k] * L.w + nx[k];
                    if (!known[j]) continue;
                    for (int c = 0; c < 4; ++c) acc[c] += L.c[j * 4 + c];
                    ++n;
                }
                if (n) {
                    for (int c = 0; c < 4; ++c) L.c[i * 4 + c] = acc[c] / n;
                    nextKnown[i] = 1;
                    changed = true;
                }
            }
        known.swap(nextKnown);
        if (!changed) break;
    }
}

float PatchDist(const Level& L, int tx, int ty, int sx, int sy, float best) {
    float d = 0;
    for (int dy = -kR; dy <= kR; ++dy) {
        int y1 = std::clamp(ty + dy, 0, L.h - 1), y2 = sy + dy;
        for (int dx = -kR; dx <= kR; ++dx) {
            int x1 = std::clamp(tx + dx, 0, L.w - 1), x2 = sx + dx;
            const float* a = &L.c[((size_t)y1 * L.w + x1) * 4];
            const float* b = &L.c[((size_t)y2 * L.w + x2) * 4];
            float e0 = a[0] - b[0], e1 = a[1] - b[1], e2 = a[2] - b[2], e3 = a[3] - b[3];
            d += e0 * e0 + e1 * e1 + e2 * e2 + e3 * e3;
        }
        if (d >= best) return d;
    }
    return d;
}

void PatchMatch(Level& L, const std::vector<int>& targets, const std::vector<int>& srcList, int pass, std::mt19937& rng) {
    // Propagation (scan order alternates) + random search around the best match.
    std::uniform_int_distribution<int> any(0, (int)srcList.size() - 1);
    int maxR = std::max(L.w, L.h);
    auto valid = [&](int x, int y) { return x >= 0 && y >= 0 && x < L.w && y < L.h && L.source[(size_t)y * L.w + x]; };
    auto tryCand = [&](int t, int tx, int ty, int sx, int sy) {
        if (!valid(sx, sy)) return;
        int s = sy * L.w + sx;
        if (s == L.nnf[t]) return;
        float d = PatchDist(L, tx, ty, sx, sy, L.dist[t]);
        if (d < L.dist[t]) {
            L.dist[t] = d;
            L.nnf[t] = s;
        }
    };
    int n = (int)targets.size();
    for (int k = 0; k < n; ++k) {
        int t = targets[pass % 2 == 0 ? k : n - 1 - k];
        int tx = t % L.w, ty = t / L.w;
        int dir = pass % 2 == 0 ? 1 : -1;
        // Neighbors' matches shifted by one pixel.
        for (int q = 0; q < 2; ++q) {
            int nx = tx - (q == 0 ? dir : 0), ny = ty - (q == 1 ? dir : 0);
            if (nx < 0 || ny < 0 || nx >= L.w || ny >= L.h) continue;
            int ns = L.nnf[(size_t)ny * L.w + nx];
            if (ns < 0) continue;
            tryCand(t, tx, ty, ns % L.w + (tx - nx), ns / L.w + (ty - ny));
        }
        // Random search.
        int bx = L.nnf[t] % L.w, by = L.nnf[t] / L.w;
        for (int r = maxR; r >= 1; r /= 2) {
            std::uniform_int_distribution<int> off(-r, r);
            tryCand(t, tx, ty, bx + off(rng), by + off(rng));
            bx = L.nnf[t] % L.w;
            by = L.nnf[t] / L.w;
        }
        if (rng() % 8 == 0) {  // occasional global jump
            int s = srcList[any(rng)];
            tryCand(t, tx, ty, s % L.w, s / L.w);
        }
    }
}

void Vote(Level& L, const std::vector<u8>& affected) {
    // Each hole pixel: average of the source pixels the overlapping target patches propose.
    std::vector<float> out = L.c;
    ParallelRows(L.h, [&](int y) {
        for (int x = 0; x < L.w; ++x) {
            size_t i = (size_t)y * L.w + x;
            if (!L.hole[i]) continue;
            double acc[4] = {0, 0, 0, 0}, wsum = 0;
            for (int dy = -kR; dy <= kR; ++dy)
                for (int dx = -kR; dx <= kR; ++dx) {
                    int tx = x + dx, ty = y + dy;
                    if (tx < 0 || ty < 0 || tx >= L.w || ty >= L.h) continue;
                    size_t t = (size_t)ty * L.w + tx;
                    if (!affected[t] || L.nnf[t] < 0) continue;
                    int s = L.nnf[t];
                    int px = s % L.w - dx, py = s / L.w - dy;
                    const float* c = &L.c[((size_t)py * L.w + px) * 4];
                    double w = 1.0 / (1.0 + L.dist[t] * 0.02);
                    for (int k = 0; k < 4; ++k) acc[k] += c[k] * w;
                    wsum += w;
                }
            if (wsum > 0) for (int k = 0; k < 4; ++k) out[i * 4 + k] = (float)(acc[k] / wsum);
        }
    });
    L.c.swap(out);
}
}  // namespace

bool Inpaint(std::vector<float>& rgba, int w, int h, const std::vector<u8>& hole, const std::atomic<bool>* cancel, std::atomic<float>* progress) {
    if (w < 2 * kR + 2 || h < 2 * kR + 2) return false;
    std::vector<Level> pyr(1);
    pyr[0].w = w;
    pyr[0].h = h;
    pyr[0].c = rgba;
    pyr[0].hole.resize(hole.size());
    for (size_t i = 0; i < hole.size(); ++i) pyr[0].hole[i] = hole[i] ? 1 : 0;
    // Pyramid down to a small hole / image.
    while (pyr.back().w > 48 && pyr.back().h > 48) {
        Level next;
        Downsample(pyr.back(), next);
        bool hasHole = std::any_of(next.hole.begin(), next.hole.end(), [](u8 v) { return v != 0; });
        if (!hasHole) break;
        pyr.push_back(std::move(next));
        if (pyr.size() > 8) break;
    }
    std::mt19937 rng(1234);
    const int levels = (int)pyr.size();
    for (int li = levels - 1; li >= 0; --li) {
        Level& L = pyr[li];
        ComputeSources(L);
        std::vector<int> srcList;
        for (size_t i = 0; i < L.source.size(); ++i) if (L.source[i]) srcList.push_back((int)i);
        if (srcList.empty()) return false;
        // Targets: pixels whose patch touches the hole.
        std::vector<u8> affected((size_t)L.w * L.h, 0);
        for (int y = 0; y < L.h; ++y)
            for (int x = 0; x < L.w; ++x)
                if (L.hole[(size_t)y * L.w + x])
                    for (int dy = -kR; dy <= kR; ++dy)
                        for (int dx = -kR; dx <= kR; ++dx) {
                            int ax = x + dx, ay = y + dy;
                            if (ax >= 0 && ay >= 0 && ax < L.w && ay < L.h) affected[(size_t)ay * L.w + ax] = 1;
                        }
        std::vector<int> targets;
        for (size_t i = 0; i < affected.size(); ++i) if (affected[i]) targets.push_back((int)i);
        L.nnf.assign((size_t)L.w * L.h, -1);
        L.dist.assign((size_t)L.w * L.h, 1e30f);
        if (li == levels - 1) {
            InitHoleByDiffusion(L);
            std::uniform_int_distribution<int> any(0, (int)srcList.size() - 1);
            for (int t : targets) L.nnf[t] = srcList[any(rng)];
        } else {
            // From the coarser level: its result as the initial guess, its matches scaled up.
            const Level& C = pyr[li + 1];
            for (int y = 0; y < L.h; ++y)
                for (int x = 0; x < L.w; ++x) {
                    size_t i = (size_t)y * L.w + x;
                    int cx = std::min(x / 2, C.w - 1), cy = std::min(y / 2, C.h - 1);
                    size_t ci = (size_t)cy * C.w + cx;
                    if (L.hole[i]) for (int k = 0; k < 4; ++k) L.c[i * 4 + k] = C.c[ci * 4 + k];
                    if (affected[i] && C.nnf.size() == (size_t)C.w * C.h && C.nnf[ci] >= 0) {
                        int sx = (C.nnf[ci] % C.w) * 2 + (x & 1), sy = (C.nnf[ci] / C.w) * 2 + (y & 1);
                        if (sx >= 0 && sy >= 0 && sx < L.w && sy < L.h && L.source[(size_t)sy * L.w + sx]) L.nnf[i] = sy * L.w + sx;
                    }
                }
            std::uniform_int_distribution<int> any(0, (int)srcList.size() - 1);
            for (int t : targets) if (L.nnf[t] < 0) L.nnf[t] = srcList[any(rng)];
        }
        for (int t : targets) L.dist[t] = PatchDist(L, t % L.w, t / L.w, L.nnf[t] % L.w, L.nnf[t] / L.w, 1e30f);
        const int em = li == 0 ? 3 : 5;
        for (int it = 0; it < em; ++it) {
            if (cancel && cancel->load()) return false;
            for (int pass = 0; pass < 2; ++pass) PatchMatch(L, targets, srcList, pass, rng);
            Vote(L, affected);
            for (int t : targets) L.dist[t] = PatchDist(L, t % L.w, t / L.w, L.nnf[t] % L.w, L.nnf[t] / L.w, 1e30f);
            if (progress) progress->store(((levels - 1 - li) + (it + 1) / (float)em) / levels);
        }
    }
    for (size_t i = 0; i < hole.size(); ++i)
        if (hole[i]) for (int k = 0; k < 4; ++k) rgba[i * 4 + k] = pyr[0].c[i * 4 + k];
    return true;
}

}  // namespace sx
