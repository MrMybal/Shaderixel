// Seam carving (Avidan & Shamir): dynamic programming over a gradient energy, one seam at a time, with the
// "forward" cost (the energy a removal creates) for cleaner results. Enlarging finds the seams that would be
// removed first and duplicates them (in that order), so the new pixels spread over the flat areas.
#include "render/SeamCarve.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sx {

namespace {

constexpr float kProtect = 1e5f;
constexpr float kSaliency = 0.6f;

// One direction (width): the luminance and the mask of the current image, column indices of the original.
struct Work {
    int w = 0, h = 0;
    std::vector<float> lum;     // current, row-major w x h (stride = initial width)
    std::vector<float> bias;    // mask energy (+ protect, - remove)
    std::vector<int> index;     // original x of each current pixel
    int stride = 0;
};

// Lowest-cost vertical seam (forward energy). Returns x per row.
std::vector<int> FindSeam(const Work& W) {
    const int w = W.w, h = W.h, S = W.stride;
    std::vector<float> M((size_t)w * h);
    std::vector<int8_t> from((size_t)w * h);
    auto L = [&](int x, int y) { return W.lum[(size_t)y * S + std::clamp(x, 0, w - 1)]; };
    for (int x = 0; x < w; ++x) M[x] = W.bias[x] + std::abs(L(x + 1, 0) - L(x - 1, 0));
    for (int y = 1; y < h; ++y) {
        const float* prev = &M[(size_t)(y - 1) * w];
        float* cur = &M[(size_t)y * w];
        int8_t* f = &from[(size_t)y * w];
        for (int x = 0; x < w; ++x) {
            // Forward costs: the new neighbors created by removing (x, y) after a step from up-left / up / up-right.
            const float cu = std::abs(L(x + 1, y) - L(x - 1, y));
            const float cl = cu + std::abs(L(x, y - 1) - L(x - 1, y));
            const float cr = cu + std::abs(L(x, y - 1) - L(x + 1, y));
            float best = prev[x] + cu;
            int8_t d = 0;
            if (x > 0 && prev[x - 1] + cl < best) { best = prev[x - 1] + cl; d = -1; }
            if (x + 1 < w && prev[x + 1] + cr < best) { best = prev[x + 1] + cr; d = 1; }
            cur[x] = best + W.bias[(size_t)y * S + x];
            f[x] = d;
        }
    }
    std::vector<int> seam(h);
    const float* last = &M[(size_t)(h - 1) * w];
    seam[h - 1] = (int)(std::min_element(last, last + w) - last);
    for (int y = h - 1; y > 0; --y) seam[y - 1] = std::clamp(seam[y] + from[(size_t)y * w + seam[y]], 0, w - 1);
    return seam;
}

void RemoveSeam(Work& W, const std::vector<int>& seam) {
    const int S = W.stride;
    for (int y = 0; y < W.h; ++y) {
        const int x = seam[y];
        const size_t row = (size_t)y * S;
        std::copy(W.lum.begin() + row + x + 1, W.lum.begin() + row + W.w, W.lum.begin() + row + x);
        std::copy(W.bias.begin() + row + x + 1, W.bias.begin() + row + W.w, W.bias.begin() + row + x);
        std::copy(W.index.begin() + row + x + 1, W.index.begin() + row + W.w, W.index.begin() + row + x);
    }
    --W.w;
}

// Width w -> newW on an image of luminance lum (w x h) and bias; source x of every output pixel (newW x h).
bool CarveWidth(const std::vector<float>& lum, const std::vector<float>& bias, int w, int h, int newW, std::vector<float>& map,
                std::atomic<float>* progress, float p0, float p1, const std::atomic<bool>* cancel) {
    map.assign((size_t)newW * h, 0.0f);
    if (newW == w) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) map[(size_t)y * newW + x] = (float)x;
        return true;
    }
    Work W;
    W.w = w;
    W.h = h;
    W.stride = w;
    W.lum = lum;
    W.bias = bias;
    W.index.resize((size_t)w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) W.index[(size_t)y * w + x] = x;
    const int k = std::abs(newW - w);
    // Shrinking: k seams removed. Enlarging: k seams found the same way, then duplicated.
    std::vector<std::vector<int>> removed;  // original x of each seam, per row
    for (int s = 0; s < k; ++s) {
        if (cancel && *cancel) return false;
        const std::vector<int> seam = FindSeam(W);
        if (newW > w) {
            std::vector<int> orig(h);
            for (int y = 0; y < h; ++y) orig[y] = W.index[(size_t)y * w + seam[y]];
            removed.push_back(std::move(orig));
        }
        RemoveSeam(W, seam);
        if (progress) *progress = p0 + (p1 - p0) * (float)(s + 1) / k;
    }
    if (newW < w) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < newW; ++x) map[(size_t)y * newW + x] = (float)W.index[(size_t)y * w + x];
        return true;
    }
    // Enlarging: every original column, followed by a copy (between it and the next) where a seam passed.
    for (int y = 0; y < h; ++y) {
        std::vector<int> count(w, 0);
        for (auto& sm : removed) ++count[sm[y]];
        int o = 0;
        for (int x = 0; x < w && o < newW; ++x) {
            map[(size_t)y * newW + o++] = (float)x;
            for (int c = 0; c < count[x] && o < newW; ++c) map[(size_t)y * newW + o++] = std::min((float)x + 0.5f, (float)(w - 1));
        }
        while (o < newW) { map[(size_t)y * newW + o] = (float)(w - 1); ++o; }
    }
    return true;
}

// Bilinear read along x of an RGBA8 image.
void SampleRowX(const std::vector<u8>& src, int w, int y, float x, u8 out[4]) {
    const int x0 = std::clamp((int)std::floor(x), 0, w - 1), x1 = std::min(x0 + 1, w - 1);
    const float t = std::clamp(x - (float)x0, 0.0f, 1.0f);
    const u8* a = &src[((size_t)y * w + x0) * 4];
    const u8* b = &src[((size_t)y * w + x1) * 4];
    for (int c = 0; c < 4; ++c) out[c] = (u8)std::lround(a[c] + (b[c] - a[c]) * t);
}

}  // namespace

SeamCarveResult SeamCarve(const SeamCarveInput& in, std::atomic<float>* progress, const std::atomic<bool>* cancel) {
    SeamCarveResult r;
    r.w = in.w;
    r.h = in.h;
    r.newW = in.newW;
    r.newH = in.newH;
    const int w = (int)in.w, h = (int)in.h, nw = (int)in.newW, nh = (int)in.newH;
    if (w < 4 || h < 4 || nw < 2 || nh < 2 || in.rgba.size() < (size_t)w * h * 4) { r.error = "image trop petite"; return r; }
    if (nw > w * 2 || nh > h * 2) { r.error = "agrandissement limité au double"; return r; }
    // Luminance and mask energy of the original.
    auto lumOf = [](const u8* p) { return (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / 255.0f; };
    std::vector<float> lum((size_t)w * h), bias((size_t)w * h, 0.0f);
    for (size_t i = 0; i < lum.size(); ++i) lum[i] = lumOf(&in.rgba[i * 4]);
    // Saliency: how far each pixel's color is from its wide surroundings (box blur over ~1/12 of the image).
    // Flat subjects (a plain ball, a wall) get energy over their whole area instead of their outline only, so
    // the seams go around them through the background.
    {
        const int R = std::max(2, std::max(w, h) / 24);
        std::vector<double> integ((size_t)(w + 1) * (h + 1) * 3, 0.0);
        auto I = [&](int x, int y, int c) -> double& { return integ[((size_t)y * (w + 1) + x) * 3 + c]; };
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                for (int c = 0; c < 3; ++c) I(x + 1, y + 1, c) = in.rgba[((size_t)y * w + x) * 4 + c] / 255.0 + I(x, y + 1, c) + I(x + 1, y, c) - I(x, y, c);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const int x0 = std::max(0, x - R), x1 = std::min(w, x + R + 1), y0 = std::max(0, y - R), y1 = std::min(h, y + R + 1);
                const double n = (double)(x1 - x0) * (y1 - y0);
                double d2 = 0;
                for (int c = 0; c < 3; ++c) {
                    const double mean = (I(x1, y1, c) - I(x0, y1, c) - I(x1, y0, c) + I(x0, y0, c)) / n;
                    const double v = in.rgba[((size_t)y * w + x) * 4 + c] / 255.0 - mean;
                    d2 += v * v;
                }
                bias[(size_t)y * w + x] = kSaliency * (float)std::sqrt(d2);
            }
    }
    const bool hasMask = in.maskMode && in.mask.size() >= (size_t)w * h;
    if (hasMask)
        for (size_t i = 0; i < bias.size(); ++i)
            if (in.mask[i] > 127) bias[i] = in.maskMode == 1 ? kProtect : -kProtect;
    const bool both = nw != w && nh != h;
    // Width first.
    if (!CarveWidth(lum, bias, w, h, nw, r.mapX, progress, 0.0f, both ? 0.5f : 1.0f, cancel)) { r.error = "annulé"; return r; }
    // The image (and mask) after the horizontal pass, transposed for the height.
    std::vector<float> lumT((size_t)nw * h), biasT((size_t)nw * h, 0.0f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < nw; ++x) {
            u8 px[4];
            const float sx = r.mapX[(size_t)y * nw + x];
            SampleRowX(in.rgba, w, y, sx, px);
            lumT[(size_t)x * h + y] = lumOf(px);
            if (hasMask) biasT[(size_t)x * h + y] = bias[(size_t)y * w + std::clamp((int)std::lround(sx), 0, w - 1)];
        }
    std::vector<float> mapT;
    if (!CarveWidth(lumT, biasT, h, nw, nh, mapT, progress, both ? 0.5f : 0.0f, 1.0f, cancel)) { r.error = "annulé"; return r; }
    // mapT: for each transposed row (= output column x) and output row y', the source y.
    r.mapY.assign((size_t)nw * nh, 0.0f);
    for (int x = 0; x < nw; ++x)
        for (int y = 0; y < nh; ++y) r.mapY[(size_t)y * nw + x] = mapT[(size_t)x * nh + y];
    if (progress) *progress = 1.0f;
    return r;
}

}  // namespace sx
