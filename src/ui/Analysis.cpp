// Image analysis panel: histogram, waveform (luma, RGB overlay, RGB parade), vectorscope, spectrum (2D FFT),
// false color exposure map, statistics, dominant colors and a pixel inspector. Computed on the CPU from a
// reduced copy (sRGB, as displayed) of the whole image or of the active layer / node, when it changes.
#include "ui/App.h"
#include "ui/Theme.h"
#include "ui/Lang.h"

#include <imgui_internal.h>
#include <complex>
#include <random>

namespace sx {

namespace {
constexpr int kMaxW = 512;     // analysis copy: at most this wide
constexpr int kLevels = 256;   // waveform height, histogram bins
constexpr int kVs = 256;       // vectorscope size
constexpr int kFft = 256;      // spectrum size

struct F3 {
    float x = 0, y = 0, z = 0;
};

float Luma(float r, float g, float b) { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }

// In-place radix-2 FFT.
void Fft(std::vector<std::complex<float>>& a, bool inverse) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const float ang = 6.2831853f / (float)len * (inverse ? 1.0f : -1.0f);
        const std::complex<float> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1, 0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<float> u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Exposure bands (false color): luma ranges and their colors; the rest shows the luma in gray.
struct Band {
    float lo, hi;
    ImU32 col;
    const char* label;
};
const Band kBands[] = {
    {0.0f, 0.02f, IM_COL32(120, 30, 160, 255), "Noir écrêté"},
    {0.02f, 0.10f, IM_COL32(30, 80, 230, 255), "Ombres profondes"},
    {0.10f, 0.20f, IM_COL32(40, 160, 190, 255), "Ombres"},
    {0.38f, 0.45f, IM_COL32(60, 190, 70, 255), "Gris moyen (18 %)"},
    {0.52f, 0.58f, IM_COL32(235, 140, 170, 255), "Tons chair"},
    {0.90f, 0.97f, IM_COL32(240, 220, 40, 255), "Hautes lumières"},
    {0.97f, 2.0f, IM_COL32(230, 40, 40, 255), "Blanc écrêté"},
};

ImU32 Inferno(float t) {
    // Black, purple, orange, yellow.
    t = std::clamp(t, 0.0f, 1.0f);
    const float k[5][3] = {{0, 0, 0.02f}, {0.35f, 0.05f, 0.45f}, {0.8f, 0.2f, 0.3f}, {0.98f, 0.6f, 0.1f}, {1, 1, 0.75f}};
    const float f = t * 4;
    const int i = std::min(3, (int)f);
    const float u = f - i;
    auto c = [&](int ch) { return (int)(255 * (k[i][ch] + (k[i + 1][ch] - k[i][ch]) * u)); };
    return IM_COL32(c(0), c(1), c(2), 255);
}
}  // namespace

void App::UpdateAnalysis() {
    auto& A = analysis;
    DocView* v = Active();
    if (!v) { A.valid = false; return; }
    // Result of the readback: the CPU analysis.
    if (A.rb && A.rb->ready && (!A.rbSel || A.rbSel->ready)) {
        A.sel.clear();
        if (A.rbSel && A.rbSel->width == A.rb->width && A.rbSel->height == A.rb->height) {
            const u16* sp = (const u16*)A.rbSel->data.data();
            A.sel.resize((size_t)A.rbSel->width * A.rbSel->height);
            for (size_t i = 0; i < A.sel.size(); ++i) A.sel[i] = sp[i * 4] / 65535.0f;
        }
        A.rbSel.reset();
        const u32 w = A.rb->width, h = A.rb->height;
        const u16* p = (const u16*)A.rb->data.data();
        A.w = w;
        A.h = h;
        A.px.resize((size_t)w * h * 4);
        for (size_t i = 0; i < A.px.size(); ++i) A.px[i] = p[i] / 65535.0f;
        // Selection only: the pixels outside count as transparent (left out of the scopes).
        if (!A.sel.empty())
            for (size_t i = 0; i < A.sel.size(); ++i)
                if (A.sel[i] < 0.5f) A.px[i * 4 + 3] = 0.0f;
        A.rb.reset();
        ComputeAnalysis();
        A.valid = true;
        ++A.version;
    }
    // Source: the whole image or the active layer / node.
    u64 hash = 0;
    gpu::Texture* src = nullptr;
    if (A.source == 0) { src = v->lastImage; hash = v->renderer.ImageVersion(); }
    else src = ActiveLayerContent(*v, hash);
    if (!src) return;
    hash ^= (u64)A.source * 0x9E3779B97F4A7C15ull;
    if (A.selectionOnly)
        if (Surface* s = v->doc->Selection()) hash ^= (s->version + 1) * 0xC2B2AE3D27D4EB4Full;
    hash ^= A.selectionOnly ? 0x165667B19E3779F9ull : 0;
    const u64 frame = gpu::Dev().FrameNumber();
    if (!A.rb && (hash != A.hash || A.view != v) && frame - A.frame >= 12) {
        const u32 w = std::min<u32>(kMaxW, src->Width()), h = std::max<u32>(1, (u32)((u64)w * src->Height() / std::max<u32>(1, src->Width())));
        if (auto copy = v->renderer.PreviewCopy(*ctx, src, w, h)) {
            A.rb = v->renderer.ExportRGBA16(*ctx, copy.get(), true);
            // The selection at the same size (red = coverage), read back with it.
            A.rbSel.reset();
            if (A.selectionOnly)
                if (Surface* s = v->doc->Selection())
                    if (auto sc = v->renderer.PreviewCopy(*ctx, s->tex.get(), w, h)) A.rbSel = v->renderer.ExportRGBA16(*ctx, sc.get(), false);
            A.hash = hash;
            A.view = v;
            A.frame = frame;
            A.srcW = src->Width();
            A.srcH = src->Height();
        }
    }
    // Comparison: the other layer, reduced to the same size, and the difference metrics.
    if (A.rbB && A.rbB->ready) {
        const u16* p = (const u16*)A.rbB->data.data();
        A.pxB.resize((size_t)A.rbB->width * A.rbB->height * 4);
        for (size_t i = 0; i < A.pxB.size(); ++i) A.pxB[i] = p[i] / 65535.0f;
        A.rbB.reset();
        if (A.pxB.size() == A.px.size()) {
            double sum = 0, sq = 0;
            float mx = 0;
            size_t changed = 0;
            const size_t np = A.px.size() / 4;
            for (size_t i = 0; i < np; ++i) {
                float dm = 0;
                for (int k = 0; k < 3; ++k) {
                    const float d = A.px[i * 4 + k] - A.pxB[i * 4 + k];
                    sum += std::abs(d);
                    sq += (double)d * d;
                    dm = std::max(dm, std::abs(d));
                }
                dm = std::max(dm, std::abs(A.px[i * 4 + 3] - A.pxB[i * 4 + 3]));
                mx = std::max(mx, dm);
                if (dm > 1.5f / 255.0f) ++changed;
            }
            A.diffMean = (float)(sum / std::max<size_t>(1, np * 3)) * 255.0f;
            A.diffMax = mx * 255.0f;
            const double mse = sq / std::max<size_t>(1, np * 3);
            A.psnr = mse > 0 ? (float)(10.0 * std::log10(1.0 / mse)) : 999.0f;
            A.changedShare = np ? (float)changed / np : 0.0f;
        }
        ++A.versionB;
    }
    if (A.compareWanted && A.compareLayer && A.valid && !A.rbB) {
        // Content of that layer (filters included): input B of its blend node, else its pixels.
        Document& d = *v->doc;
        gpu::Texture* t = nullptr;
        u64 h2 = 0;
        for (auto& n : d.S().graph.nodes)
            if (n.kind == NodeKind::Blend && n.layer == A.compareLayer && n.inputs.size() > 1 && n.inputs[1])
                if ((t = v->renderer.NodeOutput(n.inputs[1])) != nullptr) h2 = v->renderer.NodeHash(n.inputs[1]);
        if (!t)
            if (const Layer* l = d.FindLayer(A.compareLayer))
                if (Surface* s = d.GetSurface(l->surface)) { t = s->tex.get(); h2 = s->version; }
        h2 ^= A.compareLayer * 0x9E3779B97F4A7C15ull;
        if (t && (h2 != A.hashB || A.hashBFor != A.version) && frame - A.frameB >= 12)
            if (auto copy = v->renderer.PreviewCopy(*ctx, t, A.w, A.h)) {
                A.rbB = v->renderer.ExportRGBA16(*ctx, copy.get(), true);
                A.hashB = h2;
                A.hashBFor = A.version;
                A.frameB = frame;
            }
    }
    A.compareWanted = false;
    // Color samplers.
    for (auto& sp : A.samplers) {
        if (sp.rb && sp.rb->ready) {
            auto& d = sp.rb->data;
            if (sp.rb->format == gpu::Format::RGBA16F && d.size() >= 8) {
                const u16* hh = (const u16*)d.data();
                sp.value = {HalfToFloat(hh[0]), HalfToFloat(hh[1]), HalfToFloat(hh[2]), HalfToFloat(hh[3])};
            } else if (sp.rb->format == gpu::Format::RGBA32F && d.size() >= 16) {
                memcpy(&sp.value, d.data(), 16);
            }
            sp.valid = true;
            sp.rb.reset();
        }
        if (!sp.rb && v->lastImage && frame - sp.frame >= 10 && sp.pos.x < v->lastImage->Width() && sp.pos.y < v->lastImage->Height()) {
            sp.rb = v->renderer.ReadPixel(*ctx, v->lastImage, (int)sp.pos.x, (int)sp.pos.y);
            sp.frame = frame;
        }
    }
    // Pixel inspector: the exact value under the cursor (composited image).
    if (A.probe && A.probe->ready) {
        auto& d = A.probe->data;
        float4 c;
        if (A.probe->format == gpu::Format::RGBA16F && d.size() >= 8) {
            const u16* hh = (const u16*)d.data();
            c = {HalfToFloat(hh[0]), HalfToFloat(hh[1]), HalfToFloat(hh[2]), HalfToFloat(hh[3])};
        } else if (A.probe->format == gpu::Format::RGBA32F && d.size() >= 16) {
            memcpy(&c, d.data(), 16);
        }
        A.probeValue = c;
        A.probeValid = true;
        A.probe.reset();
    }
    if (A.wantProbe && cursorInDoc && v->lastImage && !A.probe && (cursorDoc.x != A.probePos.x || cursorDoc.y != A.probePos.y || frame - A.probeFrame > 30)) {
        const int x = (int)cursorDoc.x, y = (int)cursorDoc.y;
        if (x >= 0 && y >= 0 && x < (int)v->lastImage->Width() && y < (int)v->lastImage->Height()) {
            A.probe = v->renderer.ReadPixel(*ctx, v->lastImage, x, y);
            A.probePos = cursorDoc;
            A.probeFrame = frame;
        }
    }
    A.wantProbe = false;
}

void App::ComputeAnalysis() {
    auto& A = analysis;
    const u32 w = A.w, h = A.h;
    const size_t n = (size_t)w * h;
    const float* px = A.px.data();
    // Histograms: R, G, B, luma, alpha; statistics.
    for (auto& b : A.bins) std::fill(std::begin(b), std::end(b), 0ull);
    double sum[5] = {}, sum2[5] = {};
    float mn[5], mx[5];
    std::fill(mn, mn + 5, 1e9f);
    std::fill(mx, mx + 5, -1e9f);
    size_t opaque = 0;
    for (size_t i = 0; i < n; ++i) {
        const float* c = px + i * 4;
        const float vals[5] = {c[0], c[1], c[2], Luma(c[0], c[1], c[2]), c[3]};
        for (int k = 0; k < 5; ++k) {
            if (k < 4 && c[3] < 0.004f) continue;  // transparent pixels: alpha only
            const float x = vals[k];
            A.bins[k][std::clamp((int)(x * 255.0f + 0.5f), 0, 255)]++;
            sum[k] += x;
            sum2[k] += (double)x * x;
            mn[k] = std::min(mn[k], x);
            mx[k] = std::max(mx[k], x);
        }
        if (c[3] >= 0.004f) ++opaque;
    }
    for (int k = 0; k < 5; ++k) {
        const double cnt = k < 4 ? (double)std::max<size_t>(opaque, 1) : (double)std::max<size_t>(n, 1);
        auto& s = A.stats[k];
        s.mean = (float)(sum[k] / cnt);
        s.sd = (float)std::sqrt(std::max(0.0, sum2[k] / cnt - (double)s.mean * s.mean));
        s.min = mn[k] > mx[k] ? 0 : mn[k];
        s.max = mn[k] > mx[k] ? 0 : mx[k];
        u64 acc = 0;
        s.median = 0;
        for (int b = 0; b < 256; ++b) {
            acc += A.bins[k][b];
            if (acc * 2 >= (u64)cnt) { s.median = b / 255.0f; break; }
        }
        s.clipLow = (float)(A.bins[k][0] / cnt);
        s.clipHigh = (float)(A.bins[k][255] / cnt);
    }
    A.transparent = n ? 1.0f - (float)opaque / n : 0.0f;

    // Waveform: per column of the image, how many pixels at each level (R, G, B, luma).
    A.waveW = std::min<u32>(w, kMaxW);
    for (auto& wv : A.wave) wv.assign((size_t)A.waveW * kLevels, 0);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x) {
            const float* c = px + ((size_t)y * w + x) * 4;
            if (c[3] < 0.004f) continue;
            const u32 col = x * A.waveW / w;
            const float vals[4] = {c[0], c[1], c[2], Luma(c[0], c[1], c[2])};
            for (int k = 0; k < 4; ++k) A.wave[k][(size_t)std::clamp((int)(vals[k] * 255.0f + 0.5f), 0, 255) * A.waveW + col]++;
        }

    // Vectorscope: Cb / Cr (BT.709) of every pixel, with the mean color landing there.
    A.vs.assign((size_t)kVs * kVs, 0);
    A.vsColor.assign((size_t)kVs * kVs * 3, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        const float* c = px + i * 4;
        if (c[3] < 0.004f) continue;
        const float y = Luma(c[0], c[1], c[2]), cb = (c[2] - y) / 1.8556f, cr = (c[0] - y) / 1.5748f;
        const int vx = std::clamp((int)(kVs * 0.5f + cb * kVs * 0.9f), 0, kVs - 1), vy = std::clamp((int)(kVs * 0.5f - cr * kVs * 0.9f), 0, kVs - 1);
        const size_t k = (size_t)vy * kVs + vx;
        A.vs[k]++;
        A.vsColor[k * 3] += c[0];
        A.vsColor[k * 3 + 1] += c[1];
        A.vsColor[k * 3 + 2] += c[2];
    }

    // Spectrum: 2D FFT of the luminance (square resample, Hann window), log magnitude, centered.
    std::vector<std::complex<float>> f((size_t)kFft * kFft);
    double meanL = 0;
    for (int y = 0; y < kFft; ++y)
        for (int x = 0; x < kFft; ++x) {
            const u32 sx = std::min<u32>(w - 1, (u32)((x + 0.5f) * w / kFft)), sy = std::min<u32>(h - 1, (u32)((y + 0.5f) * h / kFft));
            const float* c = px + ((size_t)sy * w + sx) * 4;
            const float l = Luma(c[0], c[1], c[2]);
            f[(size_t)y * kFft + x] = l;
            meanL += l;
        }
    meanL /= (double)kFft * kFft;
    for (int y = 0; y < kFft; ++y)
        for (int x = 0; x < kFft; ++x) {
            const float wx = 0.5f - 0.5f * std::cos(6.2831853f * x / (kFft - 1)), wy = 0.5f - 0.5f * std::cos(6.2831853f * y / (kFft - 1));
            f[(size_t)y * kFft + x] = (f[(size_t)y * kFft + x] - (float)meanL) * wx * wy;
        }
    std::vector<std::complex<float>> line(kFft);
    for (int y = 0; y < kFft; ++y) {
        for (int x = 0; x < kFft; ++x) line[x] = f[(size_t)y * kFft + x];
        Fft(line, false);
        for (int x = 0; x < kFft; ++x) f[(size_t)y * kFft + x] = line[x];
    }
    for (int x = 0; x < kFft; ++x) {
        for (int y = 0; y < kFft; ++y) line[y] = f[(size_t)y * kFft + x];
        Fft(line, false);
        for (int y = 0; y < kFft; ++y) f[(size_t)y * kFft + x] = line[y];
    }
    A.spectrum.assign((size_t)kFft * kFft, 0.0f);
    float peak = 1e-6f;
    for (int y = 0; y < kFft; ++y)
        for (int x = 0; x < kFft; ++x) {
            const float m = std::log1p(std::abs(f[(size_t)y * kFft + x]));
            const int sx = (x + kFft / 2) % kFft, sy = (y + kFft / 2) % kFft;
            A.spectrum[(size_t)sy * kFft + sx] = m;
            peak = std::max(peak, m);
        }
    for (float& s : A.spectrum) s /= peak;

    // Dominant colors: k-means on a sample of the opaque pixels.
    std::vector<F3> sample;
    const size_t step = std::max<size_t>(1, n / 6000);
    for (size_t i = 0; i < n; i += step)
        if (px[i * 4 + 3] >= 0.5f) sample.push_back({px[i * 4], px[i * 4 + 1], px[i * 4 + 2]});
    const int K = std::min<int>(6, (int)sample.size());
    std::vector<F3> centers;
    std::mt19937 rng(7);
    if (K > 0) {
        centers.push_back(sample[sample.size() / 2]);
        std::vector<float> dist(sample.size());
        while ((int)centers.size() < K) {
            // k-means++: the next center far from the others.
            double total = 0;
            for (size_t i = 0; i < sample.size(); ++i) {
                float best = 1e9f;
                for (auto& c : centers) {
                    const F3 d{sample[i].x - c.x, sample[i].y - c.y, sample[i].z - c.z};
                    best = std::min(best, d.x * d.x + d.y * d.y + d.z * d.z);
                }
                dist[i] = best;
                total += best;
            }
            if (total <= 0) break;
            double r = std::uniform_real_distribution<double>(0, total)(rng);
            size_t pick = 0;
            for (; pick < sample.size() - 1 && (r -= dist[pick]) > 0; ++pick) {}
            centers.push_back(sample[pick]);
        }
    }
    std::vector<int> owner(sample.size(), 0);
    std::vector<size_t> counts(centers.size(), 0);
    for (int it = 0; it < 12 && !centers.empty(); ++it) {
        std::vector<F3> acc(centers.size(), F3{0, 0, 0});
        std::fill(counts.begin(), counts.end(), 0);
        for (size_t i = 0; i < sample.size(); ++i) {
            int best = 0;
            float bd = 1e9f;
            for (size_t c = 0; c < centers.size(); ++c) {
                const F3 d{sample[i].x - centers[c].x, sample[i].y - centers[c].y, sample[i].z - centers[c].z};
                const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
                if (dd < bd) { bd = dd; best = (int)c; }
            }
            owner[i] = best;
            acc[best] = {acc[best].x + sample[i].x, acc[best].y + sample[i].y, acc[best].z + sample[i].z};
            counts[best]++;
        }
        for (size_t c = 0; c < centers.size(); ++c)
            if (counts[c]) centers[c] = {acc[c].x / counts[c], acc[c].y / counts[c], acc[c].z / counts[c]};
    }
    // Sharpness: variance of the Laplacian of the luminance per 16 x 16 block; noise: Immerkaer's estimator.
    {
        std::vector<float> L(n);
        for (size_t i = 0; i < n; ++i) L[i] = Luma(px[i * 4], px[i * 4 + 1], px[i * 4 + 2]);
        const u32 bs = 16;
        A.sharpW = std::max<u32>(1, w / bs);
        A.sharpH = std::max<u32>(1, h / bs);
        std::vector<double> s1((size_t)A.sharpW * A.sharpH, 0.0), s2(s1.size(), 0.0), cnt(s1.size(), 0.0);
        double noiseSum = 0, varSum = 0;
        for (u32 y = 1; y + 1 < h; ++y)
            for (u32 x = 1; x + 1 < w; ++x) {
                auto at = [&](int dx, int dy) { return L[(size_t)(y + dy) * w + (x + dx)]; };
                const float lap = 4 * at(0, 0) - at(-1, 0) - at(1, 0) - at(0, -1) - at(0, 1);
                const float im = at(-1, -1) - 2 * at(0, -1) + at(1, -1) - 2 * at(-1, 0) + 4 * at(0, 0) - 2 * at(1, 0) + at(-1, 1) - 2 * at(0, 1) + at(1, 1);
                noiseSum += std::abs(im);
                const size_t b = (size_t)std::min(A.sharpH - 1, y / bs) * A.sharpW + std::min(A.sharpW - 1, x / bs);
                s1[b] += lap;
                s2[b] += (double)lap * lap;
                cnt[b] += 1;
            }
        A.sharpMap.assign(s1.size(), 0.0f);
        float peak = 1e-9f;
        for (size_t b = 0; b < s1.size(); ++b) {
            if (!cnt[b]) continue;
            const double m = s1[b] / cnt[b], var = std::max(0.0, s2[b] / cnt[b] - m * m);
            A.sharpMap[b] = (float)std::sqrt(var);
            peak = std::max(peak, A.sharpMap[b]);
            varSum += var;
        }
        for (float& s : A.sharpMap) s /= peak;
        A.sharpness = (float)std::sqrt(varSum / std::max<size_t>(1, s1.size())) * 255.0f;
        A.noise = w > 2 && h > 2 ? (float)(std::sqrt(3.14159265 * 0.5) / (6.0 * (w - 2) * (h - 2)) * noiseSum) * 255.0f : 0.0f;
    }
    A.palette.clear();
    for (size_t c = 0; c < centers.size(); ++c)
        if (counts[c]) A.palette.push_back({float4(centers[c].x, centers[c].y, centers[c].z, 1), (float)counts[c] / (float)sample.size()});
    std::sort(A.palette.begin(), A.palette.end(), [](auto& a, auto& b) { return a.second > b.second; });
}

// The scope images, rebuilt when the analysis or the display settings change.
gpu::Texture* App::AnalysisImage(DocView& v, int kind) {
    auto& A = analysis;
    const u64 key = A.version * 1000003ull ^ (u64)kind * 7919ull ^ (u64)(A.gain * 1000) ^ ((u64)A.waveMode << 40) ^ (A.versionB << 52);
    if (A.tex[kind] && A.texKey[kind] == key) return A.tex[kind].get();
    std::vector<u8> img;
    u32 w = 0, h = 0;
    auto put = [&](size_t i, ImU32 c) {
        img[i * 4] = (u8)(c & 0xFF);
        img[i * 4 + 1] = (u8)((c >> 8) & 0xFF);
        img[i * 4 + 2] = (u8)((c >> 16) & 0xFF);
        img[i * 4 + 3] = 255;
    };
    if (kind == 0) {
        // Waveform: brightness from the share of the column's pixels at that level.
        w = A.waveW;
        h = kLevels;
        img.assign((size_t)w * h * 4, 0);
        const float norm = A.gain * 40.0f / std::max<u32>(1, A.h);
        auto level = [&](int ch, u32 x, int lv) { return 1.0f - std::exp(-(float)A.wave[ch][(size_t)lv * A.waveW + x] * norm); };
        for (u32 y = 0; y < h; ++y)
            for (u32 x = 0; x < w; ++x) {
                const int lv = 255 - (int)y;
                float r = 0, g = 0, b = 0;
                if (A.waveMode == 0) {
                    const float t = level(3, x, lv);
                    r = t * 0.75f;
                    g = t;
                    b = t * 0.8f;
                } else if (A.waveMode == 1) {
                    r = level(0, x, lv);
                    g = level(1, x, lv);
                    b = level(2, x, lv);
                } else {
                    // Parade: R, G, B side by side.
                    const u32 third = std::max<u32>(1, w / 3), ch = std::min<u32>(2, x / third), sx = std::min<u32>(w - 1, (x - ch * third) * 3);
                    const float t = level((int)ch, sx, lv);
                    (ch == 0 ? r : ch == 1 ? g : b) = t;
                    r += ch == 0 ? 0 : t * 0.15f;
                    g += ch == 1 ? 0 : t * 0.15f;
                    b += ch == 2 ? 0 : t * 0.15f;
                }
                put((size_t)y * w + x, IM_COL32((int)(std::min(1.0f, r) * 255), (int)(std::min(1.0f, g) * 255), (int)(std::min(1.0f, b) * 255), 255));
            }
    } else if (kind == 1) {
        // Vectorscope: the mean color of the pixels landing there, brightness from their count (log).
        w = h = kVs;
        img.assign((size_t)w * h * 4, 0);
        for (size_t i = 0; i < (size_t)w * h; ++i) img[i * 4 + 3] = 255;  // opaque black
        u32 peak = 1;
        for (u32 c : A.vs) peak = std::max(peak, c);
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            if (!A.vs[i]) continue;
            const float t = std::min(1.0f, std::log1p((float)A.vs[i] * A.gain) / std::log1p((float)peak * 0.25f));
            const float inv = 1.0f / A.vs[i];
            float r = A.vsColor[i * 3] * inv, g = A.vsColor[i * 3 + 1] * inv, b = A.vsColor[i * 3 + 2] * inv;
            const float m = std::max({r, g, b, 1e-3f});
            r = r / m * (0.35f + 0.65f * t);
            g = g / m * (0.35f + 0.65f * t);
            b = b / m * (0.35f + 0.65f * t);
            put(i, IM_COL32((int)(r * 255 * t), (int)(g * 255 * t), (int)(b * 255 * t), 255));
        }
    } else if (kind == 2) {
        w = h = kFft;
        img.assign((size_t)w * h * 4, 0);
        for (size_t i = 0; i < (size_t)w * h; ++i) put(i, Inferno(std::pow(A.spectrum[i], 1.0f / std::max(0.2f, A.gain))));
    } else if (kind == 4) {
        w = A.sharpW;
        h = A.sharpH;
        img.assign((size_t)w * h * 4, 0);
        for (size_t i = 0; i < (size_t)w * h; ++i) put(i, Inferno(std::pow(A.sharpMap[i], 1.0f / std::max(0.2f, A.gain))));
    } else if (kind == 5) {
        // Difference with the compared layer, amplified.
        if (A.pxB.size() != A.px.size()) return nullptr;
        w = A.w;
        h = A.h;
        img.assign((size_t)w * h * 4, 0);
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            float d = 0;
            for (int k = 0; k < 4; ++k) d = std::max(d, std::abs(A.px[i * 4 + k] - A.pxB[i * 4 + k]));
            put(i, d < 0.5f / 255.0f ? IM_COL32(16, 16, 20, 255) : Inferno(std::min(1.0f, 0.15f + d * 4.0f * A.gain)));
        }
    } else {
        // False color.
        w = A.w;
        h = A.h;
        img.assign((size_t)w * h * 4, 0);
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            const float* c = A.px.data() + i * 4;
            const float l = Luma(c[0], c[1], c[2]);
            ImU32 col = 0;
            for (const Band& b : kBands)
                if (l >= b.lo && l < b.hi) { col = b.col; break; }
            if (!col) {
                const int g = (int)(l * 200 + 20);
                col = IM_COL32(g, g, g, 255);
            }
            if (c[3] < 0.004f) col = IM_COL32(30, 30, 34, 255);
            put(i, col);
        }
    }
    if (!w || !h) return nullptr;
    const gpu::Format fmt = v.doc->GetPrecision() == Precision::Float32 ? gpu::Format::RGBA32F : gpu::Format::RGBA16F;
    A.tex[kind] = v.renderer.ImportTexture(*ctx, img.data(), w, h, false, fmt);
    A.texKey[kind] = key;
    return A.tex[kind].get();
}

void App::DrawAnalysis() {
    if (!ImGui::Begin("Analyse d'image", PanelFlag("Analyse d'image"))) { ImGui::End(); return; }
    PopOutCorner("Analyse d'image");
    auto& A = analysis;
    DocView* v = Active();
    ImGui::SetNextItemWidth(170 * dpi);
    ImGui::Combo("##ansrc", &A.source, "Image entière\0Calque ou nœud actif\0");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::SliderFloat("##angain", &A.gain, 0.25f, 4.0f, "Intensité %.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    ImGui::Checkbox("4 vues", &A.quad);
    ImGui::SameLine();
    ImGui::BeginDisabled(!v || !v->doc->Selection());
    ImGui::Checkbox("Sélection", &A.selectionOnly);
    ImGui::EndDisabled();
    ui::HelpTooltip("Seulement les pixels de la sélection (scopes, statistiques, couleurs).");
    ui::HelpTooltip("Histogramme, waveform, vectorscope et spectre ensemble.");
    // Overlay on the canvas (full resolution, in the display shader).
    ImGui::SetNextItemWidth(170 * dpi);
    ImGui::Combo("##anov", &A.overlay, "Canevas : normal\0Canevas : zébras\0Canevas : écrêtage\0Canevas : fausses couleurs\0Canevas : focus peaking\0Canevas : luminance\0");
    ui::HelpTooltip("Superposition sur le canevas, en pleine résolution :\n"
                    "zébras au-dessus d'un seuil de luminance, écrêtage (bleu : canal à 0, rouge : canal à 1),\n"
                    "fausses couleurs (bandes d'exposition), focus peaking (zones nettes en rouge), luminance seule.");
    if (A.overlay == 1) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130 * dpi);
        float pct = A.zebra * 100.0f;
        if (ImGui::SliderFloat("##anzebra", &pct, 50.0f, 100.0f, "Seuil %.0f %%")) A.zebra = pct / 100.0f;
    } else if (A.overlay == 4) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130 * dpi);
        ImGui::SliderFloat("##anpeak", &A.peaking, 0.03f, 0.5f, "Sensibilité %.2f");
        ui::HelpTooltip("Seuil de contraste local : plus bas, plus de zones marquées.");
    }
    if (!v || !A.valid) {
        ImGui::TextDisabled(v ? "Analyse en cours..." : "Aucune image");
        ImGui::End();
        return;
    }
    ImGui::TextDisabled("%u x %u analysé (image %u x %u)", A.w, A.h, A.srcW, A.srcH);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 grid = IM_COL32(255, 255, 255, 40), gridText = IM_COL32(200, 200, 210, 140);
    ImFont* font = ImGui::GetFont();
    const float fsSmall = ImGui::GetFontSize() * 0.8f;

    // A scope image drawn in a box, returns its rect.
    auto scopeImage = [&](gpu::Texture* t, u64 tkey, float bw, float bh, bool keepAspect) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = bw, h = bh;
        if (keepAspect && t && t->Width()) {
            const float asp = (float)t->Height() / t->Width();
            if (w * asp > h) w = h / asp;
            else h = w * asp;
        }
        ImVec2 a(p.x + (bw - w) * 0.5f, p.y), b(a.x + w, a.y + h);
        dl->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + bw, p.y + h), IM_COL32(10, 10, 12, 255), 3);
        // Stretched scopes (waveform): the display copy at the texture's own size, stretched on the box.
        const u32 tw = keepAspect || !t ? (u32)std::max(1.0f, w) : t->Width(), th = keepAspect || !t ? (u32)std::max(1.0f, h) : t->Height();
        if (t)
            if (gpu::Texture* shown = v->renderer.Thumbnail(*ctx, tkey, t, A.version ^ ((u64)tw << 16) ^ (u64)th ^ (u64)(A.gain * 997) ^ ((u64)A.waveMode << 50), tw, th))
                dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(shown), a, b);
        ImGui::Dummy(ImVec2(bw, h));
        return ImRect(a, b);
    };

    auto drawHistogram = [&](float bw, float bh) {
        ImGui::SetNextItemWidth(130 * dpi);
        ImGui::Combo("##anhch", &A.histoChannel, "RVB\0Luminance\0Rouge\0Vert\0Bleu\0Alpha\0");
        ImGui::SameLine();
        ImGui::Checkbox("Log", &A.logHisto);
        ImVec2 p0 = ImGui::GetCursorScreenPos(), p1(p0.x + bw, p0.y + bh);
        ImGui::Dummy(ImVec2(bw, bh));
        dl->AddRectFilled(p0, p1, IM_COL32(14, 14, 18, 255), 3);
        auto plot = [&](int c, ImU32 col) {
            u64 peak = 1;
            for (int i = 1; i < 255; ++i) peak = std::max(peak, A.bins[c][i]);
            const float lp = std::log1p((float)peak);
            for (int i = 0; i < 256; ++i) {
                float t = A.logHisto ? std::log1p((float)A.bins[c][i]) / lp : (float)A.bins[c][i] / (float)peak;
                t = std::min(1.0f, t);
                const float x0 = p0.x + bw * i / 256.0f, x1 = p0.x + bw * (i + 1) / 256.0f;
                dl->AddRectFilled(ImVec2(x0, p1.y - t * (bh - 2)), ImVec2(std::max(x1, x0 + 1), p1.y), col);
            }
        };
        dl->PushClipRect(p0, p1, true);
        if (A.histoChannel == 0) {
            plot(0, IM_COL32(230, 60, 60, 110));
            plot(1, IM_COL32(60, 210, 80, 110));
            plot(2, IM_COL32(70, 110, 240, 110));
        } else {
            static const int map[] = {0, 3, 0, 1, 2, 4};
            static const ImU32 cols[] = {0, IM_COL32(210, 210, 220, 220), IM_COL32(230, 70, 70, 220), IM_COL32(70, 210, 90, 220), IM_COL32(80, 120, 245, 220),
                                         IM_COL32(180, 180, 180, 220)};
            plot(map[A.histoChannel], cols[A.histoChannel]);
        }
        for (int k = 1; k < 4; ++k) dl->AddLine(ImVec2(p0.x + bw * k / 4, p0.y), ImVec2(p0.x + bw * k / 4, p1.y), grid);
        dl->PopClipRect();
        static const int statOf[] = {3, 3, 0, 1, 2, 4};
        const int sc = statOf[std::clamp(A.histoChannel, 0, 5)];
        const auto& s = A.stats[sc];
        ImGui::TextDisabled("Moyenne %.0f  Médiane %.0f  Écart type %.1f  Écrêtés : %.1f %% noirs, %.1f %% blancs", s.mean * 255, s.median * 255,
                            s.sd * 255, s.clipLow * 100, s.clipHigh * 100);
    };

    auto drawWaveform = [&](float bw, float bh) {
        ImGui::SetNextItemWidth(160 * dpi);
        ImGui::Combo("##anwave", &A.waveMode, "Luminance\0RVB superposés\0Parade RVB\0");
        ImRect r = scopeImage(AnalysisImage(*v, 0), 0x7E5FA00000000000ull, bw, bh, false);
        for (int k = 0; k <= 4; ++k) {
            const float y = r.Max.y - (r.Max.y - r.Min.y) * k / 4.0f;
            dl->AddLine(ImVec2(r.Min.x, y), ImVec2(r.Max.x, y), grid);
            char b[8];
            snprintf(b, sizeof(b), "%d", k * 25);
            dl->AddText(font, fsSmall, ImVec2(r.Min.x + 3, std::min(y, r.Max.y - fsSmall) - (k == 4 ? 0 : fsSmall)), gridText, b);
        }
        if (A.waveMode == 2)
            for (int k = 1; k < 3; ++k) dl->AddLine(ImVec2(r.Min.x + r.GetWidth() * k / 3, r.Min.y), ImVec2(r.Min.x + r.GetWidth() * k / 3, r.Max.y), grid);
    };

    auto drawVectorscope = [&](float bw, float bh) {
        const float side = std::min(bw, bh);
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + (bw - side) * 0.5f, ImGui::GetCursorScreenPos().y));
        ImRect r = scopeImage(AnalysisImage(*v, 1), 0x7E5FA00000000001ull, side, side, true);
        const ImVec2 c = r.GetCenter();
        const float rad = r.GetWidth() * 0.5f;
        dl->AddCircle(c, rad * 0.9f, grid, 64);
        dl->AddCircle(c, rad * 0.45f, grid, 48);
        dl->AddLine(ImVec2(r.Min.x, c.y), ImVec2(r.Max.x, c.y), grid);
        dl->AddLine(ImVec2(c.x, r.Min.y), ImVec2(c.x, r.Max.y), grid);
        // Targets (75 % primaries and secondaries) and the skin tone line.
        struct T { float r, g, b; const char* name; };
        static const T targets[] = {{0.75f, 0, 0, "R"}, {0.75f, 0.75f, 0, "Yl"}, {0, 0.75f, 0, "G"}, {0, 0.75f, 0.75f, "Cy"}, {0, 0, 0.75f, "B"}, {0.75f, 0, 0.75f, "Mg"}};
        auto at = [&](float cr, float cg, float cb) {
            const float y = Luma(cr, cg, cb), u = (cb - y) / 1.8556f, w = (cr - y) / 1.5748f;
            return ImVec2(c.x + u * r.GetWidth() * 0.9f, c.y - w * r.GetWidth() * 0.9f);
        };
        for (const T& t : targets) {
            const ImVec2 q = at(t.r, t.g, t.b);
            dl->AddRect(ImVec2(q.x - 5, q.y - 5), ImVec2(q.x + 5, q.y + 5), IM_COL32((int)(t.r * 340), (int)(t.g * 340), (int)(t.b * 340), 200));
            dl->AddText(font, fsSmall, ImVec2(q.x + 7, q.y - fsSmall * 0.5f), gridText, t.name);
        }
        const ImVec2 skin = at(0.87f, 0.64f, 0.5f);
        const ImVec2 dir(skin.x - c.x, skin.y - c.y);
        const float dlen = std::max(1.0f, std::sqrt(dir.x * dir.x + dir.y * dir.y));
        dl->AddLine(c, ImVec2(c.x + dir.x / dlen * rad, c.y + dir.y / dlen * rad), IM_COL32(240, 190, 150, 150), 1.5f);
        dl->AddText(font, fsSmall, ImVec2(c.x + dir.x / dlen * rad * 0.7f + 4, c.y + dir.y / dlen * rad * 0.7f), IM_COL32(240, 190, 150, 200), TrC("peau"));
    };

    auto drawSpectrum = [&](float bw, float bh) {
        const float side = std::min(bw, bh);
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + (bw - side) * 0.5f, ImGui::GetCursorScreenPos().y));
        ImRect r = scopeImage(AnalysisImage(*v, 2), 0x7E5FA00000000002ull, side, side, true);
        if (ImGui::IsMouseHoveringRect(r.Min, r.Max))
            ImGui::SetTooltip("Centre : basses fréquences (grandes formes) ; bords : détails fins et bruit.\n"
                              "Lignes : motifs répétés ou contours orientés (perpendiculaires à la ligne) ; croix : bords de l'image, trames.");
    };

    const float avail = ImGui::GetContentRegionAvail().x;
    if (A.quad) {
        // 2 x 2.
        const float cw = (avail - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        const float ch = std::max(110.0f * dpi, (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 4) * 0.5f - ImGui::GetTextLineHeightWithSpacing() * 2);
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Histogramme");
        drawHistogram(cw, ch);
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Waveform");
        drawWaveform(cw, ch);
        ImGui::EndGroup();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Vectorscope");
        drawVectorscope(cw, ch);
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Spectre");
        drawSpectrum(cw, ch);
        ImGui::EndGroup();
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##antabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        const float bh = std::max(120.0f * dpi, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 3);
        if (ImGui::BeginTabItem("Histogramme", nullptr, A.demoTab == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
            drawHistogram(avail, bh - ImGui::GetTextLineHeightWithSpacing());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Waveform", nullptr, A.demoTab == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
            drawWaveform(avail, bh);
            ImGui::TextDisabled("Horizontal : position dans l'image ; vertical : niveau (0 à 100).");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Vectorscope", nullptr, A.demoTab == 2 ? ImGuiTabItemFlags_SetSelected : 0)) {
            drawVectorscope(avail, bh);
            ImGui::TextDisabled("Angle : teinte ; distance au centre : saturation. Ligne : tons chair.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Spectre", nullptr, A.demoTab == 3 ? ImGuiTabItemFlags_SetSelected : 0)) {
            drawSpectrum(avail, bh);
            ImGui::TextDisabled("Amplitude des fréquences de la luminance (FFT 2D, échelle log).");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fausses couleurs", nullptr, A.demoTab == 4 ? ImGuiTabItemFlags_SetSelected : 0)) {
            scopeImage(AnalysisImage(*v, 3), 0x7E5FA00000000003ull, avail, bh - ImGui::GetTextLineHeightWithSpacing() * 3, true);
            float x0 = ImGui::GetCursorPosX();
            for (const Band& b : kBands) {
                ImGui::ColorButton(b.label, ImGui::ColorConvertU32ToFloat4(b.col), ImGuiColorEditFlags_NoTooltip, ImVec2(12 * dpi, 12 * dpi));
                ImGui::SameLine(0, 4);
                ImGui::TextDisabled("%s", b.label);
                if (ImGui::GetCursorPosX() + 140 * dpi < x0 + avail) ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Statistiques", nullptr, A.demoTab == 5 ? ImGuiTabItemFlags_SetSelected : 0)) {
            if (ImGui::BeginTable("##anstats", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
                for (const char* hcol : {"", "Min", "Max", "Moyenne", "Médiane", "Écart type"}) ImGui::TableSetupColumn(hcol);
                ImGui::TableHeadersRow();
                static const char* names[] = {"Rouge", "Vert", "Bleu", "Luminance", "Alpha"};
                for (int k = 0; k < 5; ++k) {
                    const auto& s = A.stats[k];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(names[k]);
                    for (float val : {s.min, s.max, s.mean, s.median}) {
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", val * 255);
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f", s.sd * 255);
                }
                ImGui::EndTable();
            }
            ImGui::TextDisabled("Valeurs de 0 à 255 (sRGB, comme à l'écran) sur les pixels non transparents.");
            const auto& l = A.stats[3];
            ImGui::Text("Noirs écrêtés : %.2f %%   Blancs écrêtés : %.2f %%", l.clipLow * 100, l.clipHigh * 100);
            ImGui::Text("Pixels transparents : %.1f %%", A.transparent * 100);
            ImGui::Text("Contraste (écart type de la luminance) : %.1f", l.sd * 255);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Couleurs dominantes", nullptr, A.demoTab == 6 ? ImGuiTabItemFlags_SetSelected : 0)) {
            ImGui::TextDisabled("Clic : couleur de premier plan ; Maj+clic : arrière-plan.");
            const float barW = avail;
            ImVec2 p = ImGui::GetCursorScreenPos();
            float x = p.x;
            for (auto& [c, share] : A.palette) {
                const float wpx = barW * share;
                dl->AddRectFilled(ImVec2(x, p.y), ImVec2(x + wpx, p.y + 40 * dpi), ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, 1)));
                x += wpx;
            }
            ImGui::Dummy(ImVec2(barW, 44 * dpi));
            for (size_t i = 0; i < A.palette.size(); ++i) {
                auto& [c, share] = A.palette[i];
                ImGui::PushID((int)i);
                if (ImGui::ColorButton("##dom", ImVec4(c.x, c.y, c.z, 1), 0, ImVec2(28 * dpi, 28 * dpi))) {
                    float4& target = ImGui::GetIO().KeyShift ? bg : fg;
                    target = {c.x, c.y, c.z, 1};
                }
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("#%02X%02X%02X   %.0f %%", (int)(c.x * 255 + 0.5f), (int)(c.y * 255 + 0.5f), (int)(c.z * 255 + 0.5f), share * 100);
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Netteté et bruit", nullptr, A.demoTab == 8 ? ImGuiTabItemFlags_SetSelected : 0)) {
            scopeImage(AnalysisImage(*v, 4), 0x7E5FA00000000004ull, avail, bh - ImGui::GetTextLineHeightWithSpacing() * 4, true);
            ImGui::Text("Netteté (contraste local) : %.1f", A.sharpness);
            ImGui::Text("Bruit estimé : %.2f niveaux (sur 255)", A.noise);
            ImGui::TextDisabled("Carte : zones nettes en clair, floues en sombre (par blocs de 16 px de la copie analysée).\n"
                                "Mesures sur la copie réduite : le bruit réel en pleine résolution est plus fort.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Comparer", nullptr, A.demoTab == 9 ? ImGuiTabItemFlags_SetSelected : 0)) {
            A.compareWanted = true;
            Document& d = *v->doc;
            const Layer* cur = d.FindLayer(A.compareLayer);
            ImGui::SetNextItemWidth(220 * dpi);
            if (ImGui::BeginCombo("Comparer avec", cur ? cur->name.c_str() : "Choisir un calque...")) {
                for (auto& l : d.S().layers)
                    if (ImGui::Selectable((l.name + "##cmp" + std::to_string(l.id)).c_str(), l.id == A.compareLayer)) {
                        A.compareLayer = l.id;
                        A.hashB = 0;
                        A.pxB.clear();
                    }
                ImGui::EndCombo();
            }
            ui::HelpTooltip("La source choisie en haut (image entière ou calque actif) comparée à ce calque.");
            if (!cur) ImGui::TextDisabled("Choisissez un calque à comparer à la source.");
            else if (A.pxB.size() != A.px.size()) ImGui::TextDisabled("Analyse en cours...");
            else {
                scopeImage(AnalysisImage(*v, 5), 0x7E5FA00000000005ull, avail, bh - ImGui::GetTextLineHeightWithSpacing() * 6, true);
                ImGui::Text("Écart moyen : %.2f   Écart max : %.0f   (sur 255)", A.diffMean, A.diffMax);
                if (A.psnr > 900) ImGui::Text("Identiques (PSNR infini)");
                else ImGui::Text("PSNR : %.1f dB", A.psnr);
                ImGui::Text("Pixels différents : %.1f %%", A.changedShare * 100);
                ImGui::TextDisabled("Carte : différences amplifiées (sombre : identique).");
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pixel", nullptr, A.demoTab == 7 ? ImGuiTabItemFlags_SetSelected : 0)) {
            A.wantProbe = true;
            if (!cursorInDoc) ImGui::TextDisabled("Survolez le canevas : la valeur du pixel sous la souris s'affiche ici.");
            else ImGui::Text("Pixel %d, %d", (int)A.probePos.x, (int)A.probePos.y);
            if (A.probeValid) {
                const float4 lin = A.probeValue;
                const float a = lin.w;
                float4 straight = a > 1e-5f ? float4{lin.x / a, lin.y / a, lin.z / a, a} : float4{0, 0, 0, 0};
                const float sr = LinearToSrgb(straight.x), sg = LinearToSrgb(straight.y), sb = LinearToSrgb(straight.z);
                ImGui::ColorButton("##probe", ImVec4(std::clamp(sr, 0.0f, 1.0f), std::clamp(sg, 0.0f, 1.0f), std::clamp(sb, 0.0f, 1.0f), 1), 0, ImVec2(48 * dpi, 48 * dpi));
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::Text("sRGB  %d  %d  %d   #%02X%02X%02X", (int)(std::clamp(sr, 0.0f, 1.0f) * 255 + 0.5f), (int)(std::clamp(sg, 0.0f, 1.0f) * 255 + 0.5f),
                            (int)(std::clamp(sb, 0.0f, 1.0f) * 255 + 0.5f), (int)(std::clamp(sr, 0.0f, 1.0f) * 255 + 0.5f),
                            (int)(std::clamp(sg, 0.0f, 1.0f) * 255 + 0.5f), (int)(std::clamp(sb, 0.0f, 1.0f) * 255 + 0.5f));
                ImGui::Text("Linéaire  %.4f  %.4f  %.4f   Alpha %.3f", straight.x, straight.y, straight.z, a);
                float hh, ss, vv;
                ImGui::ColorConvertRGBtoHSV(std::clamp(sr, 0.0f, 1.0f), std::clamp(sg, 0.0f, 1.0f), std::clamp(sb, 0.0f, 1.0f), hh, ss, vv);
                ImGui::Text("TSV  %.0f°  %.0f %%  %.0f %%   Luminance %.1f %%", hh * 360, ss * 100, vv * 100, Luma(straight.x, straight.y, straight.z) * 100);
                if (straight.x > 1 || straight.y > 1 || straight.z > 1) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "Au-delà de 1 (HDR)");
                ImGui::EndGroup();
            }
            // Pinned samplers.
            ImGui::SeparatorText("Points de mesure");
            if (A.samplers.empty()) ImGui::TextDisabled("Pipette + Maj+clic sur le canevas : un point de mesure (4 au plus), lu en continu.");
            int removeAt = -1;
            for (size_t k = 0; k < A.samplers.size(); ++k) {
                const auto& sp = A.samplers[k];
                ImGui::PushID((int)k);
                const float a = sp.value.w;
                const float4 st = a > 1e-5f ? float4{sp.value.x / a, sp.value.y / a, sp.value.z / a, a} : float4{0, 0, 0, 0};
                const float r = std::clamp(LinearToSrgb(st.x), 0.0f, 1.0f), g = std::clamp(LinearToSrgb(st.y), 0.0f, 1.0f), b = std::clamp(LinearToSrgb(st.z), 0.0f, 1.0f);
                ImGui::ColorButton("##sp", ImVec4(r, g, b, 1), 0, ImVec2(22 * dpi, 22 * dpi));
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                if (sp.valid)
                    ImGui::Text("%d  (%d, %d)   %3d %3d %3d   #%02X%02X%02X   L %.0f %%", (int)k + 1, (int)sp.pos.x, (int)sp.pos.y, (int)(r * 255 + 0.5f),
                                (int)(g * 255 + 0.5f), (int)(b * 255 + 0.5f), (int)(r * 255 + 0.5f), (int)(g * 255 + 0.5f), (int)(b * 255 + 0.5f),
                                Luma(st.x, st.y, st.z) * 100);
                else ImGui::Text("%d  (%d, %d)", (int)k + 1, (int)sp.pos.x, (int)sp.pos.y);
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) removeAt = (int)k;
                ImGui::PopID();
            }
            if (removeAt >= 0) A.samplers.erase(A.samplers.begin() + removeAt);
            if (!A.samplers.empty() && ImGui::SmallButton("Retirer tous les points")) A.samplers.clear();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    A.demoTab = -1;
    ImGui::End();
}

// ------------------------------------------------------------------ before / after

void App::TakeSnapshot(DocView& v) {
    if (!v.lastImage) return;
    auto& ab = abCompare;
    ab.snap = gpu::Dev().CreateTexture(v.lastImage->Width(), v.lastImage->Height(), v.lastImage->GetFormat(), "Snapshot");
    ctx->CopyTexture(ab.snap.get(), v.lastImage);
    ab.reduced.reset();
    ab.reducedFactor = 0;
    ab.view = &v;
    ab.on = true;
    Status("Instantané pris : comparez avant / après (Affichage > Avant / après)");
}

bool App::CompareSplitInput(DocView& v, ImVec2 canvasMin, ImVec2 size, bool hovered) {
    // Dragging the split line (its whole height): the tools do not get the click.
    auto& ab = abCompare;
    if (!(ab.on && ab.snap && ab.view == &v && ab.mode == 1)) { ab.dragging = false; return false; }
    const ImVec2 m = ImGui::GetMousePos();
    const float x = canvasMin.x + ab.split * size.x;
    const bool nearLine = hovered && std::abs(m.x - x) <= 7.0f;
    if (nearLine && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ab.dragging = true;
    if (ab.dragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) ab.split = std::clamp((m.x - canvasMin.x) / std::max(1.0f, size.x), 0.0f, 1.0f);
        else ab.dragging = false;
    }
    if (nearLine || ab.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    return nearLine || ab.dragging;
}

void App::DrawCompareMenu() {
    // View > Before / after.
    auto& ab = abCompare;
    DocView* v = Active();
    if (ImGui::MenuItem("Prendre un instantané (avant)", nullptr, false, v && v->lastImage)) TakeSnapshot(*v);
    const bool has = ab.snap && v && ab.view == v;
    ImGui::BeginDisabled(!has);
    if (ImGui::MenuItem("Comparer", nullptr, ab.on)) ab.on = !ab.on;
    ImGui::Separator();
    if (ImGui::MenuItem("Partage glissant", nullptr, ab.mode == 1)) { ab.mode = 1; ab.on = true; }
    if (ImGui::MenuItem("Avant seulement", nullptr, ab.mode == 2)) { ab.mode = 2; ab.on = true; }
    if (ImGui::MenuItem("Différence", nullptr, ab.mode == 3)) { ab.mode = 3; ab.on = true; }
    ImGui::Separator();
    if (ImGui::MenuItem("Supprimer l'instantané")) {
        ab.snap.reset();
        ab.reduced.reset();
        ab.on = false;
    }
    ImGui::EndDisabled();
    if (has && v->lastImage && (ab.snap->Width() != v->lastImage->Width() || ab.snap->Height() != v->lastImage->Height()))
        ImGui::TextDisabled("Taille du document changée : reprenez un instantané.");
}

}  // namespace sx
