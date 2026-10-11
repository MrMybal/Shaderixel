// Photo tools: Photomerge (panorama from files), auto-align / auto-blend layers
// (panorama seams or focus stacking), HDR merge, camera RAW / WIC opening.
// Feature matching and the heavy per-pixel work run on worker threads; the
// results are applied on the main thread (GPU warps, layer masks).
#include "ui/App.h"
#include "io/Align.h"
#include "io/WicLoad.h"
#include "doc/Effects.h"

#include <imgui_internal.h>
#include <stb_image.h>
#include <filesystem>
#include <thread>

namespace sx {

struct PhotoJob {
    int kind = 0;  // 1 photomerge, 2 auto-align layers, 3 auto-blend layers, 4 HDR merge
    int stage = 0;
    DocView* view = nullptr;
    std::vector<std::string> files;
    int layout = 0;     // photomerge / align: 0 auto, 1 perspective, 2 cylindrical, 3 reposition
    int blendMode = 0;  // auto-blend: 0 panorama, 1 stack (focus)
    bool blend = true;
    struct Img {
        std::string name;
        int w = 0, h = 0;
        std::vector<u8> rgba;
        Id layer = 0, surface = 0;
        gpu::ReadbackPtr rb;
        align::Mat3 m = align::Identity();  // image -> canvas / reference
        bool placed = false;
        std::vector<float> mask;            // low resolution, maskW x maskH
    };
    std::vector<Img> imgs;
    float focal = 0;
    bool cylindrical = false;
    int canvasW = 0, canvasH = 0, maskW = 0, maskH = 0;
    std::vector<float> hdr;
    std::vector<float> exposures;
    std::future<bool> work;
    std::atomic<float> progress{0};
    std::string error;
};

namespace {

float SrgbLin(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }

// Source pixel of image i for a canvas point (inverse of cylinder + matrix).
bool CanvasToImage(const PhotoJob& J, const PhotoJob::Img& im, const align::Mat3& inv, float2 p, float2& s) {
    float2 q = align::Apply(inv, p);
    if (J.cylindrical) q = align::FromCylinder(q, J.focal, im.w * 0.5f, im.h * 0.5f);
    s = q;
    return q.x >= 0 && q.y >= 0 && q.x < im.w && q.y < im.h;
}

float2 ImageToCanvas(const PhotoJob& J, const PhotoJob::Img& im, float2 p) {
    if (J.cylindrical) p = align::ToCylinder(p, J.focal, im.w * 0.5f, im.h * 0.5f);
    return align::Apply(im.m, p);
}

// Cumulative normalization: with "over" compositing bottom -> top, alpha_i = w_i / sum_{j<=i} w_j
// gives the weighted average of the images.
void NormalizeMasks(PhotoJob& J, float power) {
    const size_t n = (size_t)J.maskW * J.maskH;
    std::vector<float> acc(n, 0.0f);
    for (auto& im : J.imgs) {
        if (im.mask.size() != n) continue;
        for (size_t i = 0; i < n; ++i) {
            float w = std::pow(std::max(im.mask[i], 0.0f), power);
            acc[i] += w;
            im.mask[i] = acc[i] > 1e-12f ? w / acc[i] : 0.0f;
        }
    }
}

// Separable box blur (radius r) of a float map.
void BoxBlur(std::vector<float>& m, int w, int h, int r) {
    if (r <= 0) return;
    std::vector<float> t(m.size());
    for (int y = 0; y < h; ++y) {
        float acc = 0;
        int n = 0;
        for (int x = 0; x <= r && x < w; ++x) { acc += m[(size_t)y * w + x]; ++n; }
        for (int x = 0; x < w; ++x) {
            t[(size_t)y * w + x] = acc / n;
            if (x - r >= 0) { acc -= m[(size_t)y * w + x - r]; --n; }
            if (x + r + 1 < w) { acc += m[(size_t)y * w + x + r + 1]; ++n; }
        }
    }
    for (int x = 0; x < w; ++x) {
        float acc = 0;
        int n = 0;
        for (int y = 0; y <= r && y < h; ++y) { acc += t[(size_t)y * w + x]; ++n; }
        for (int y = 0; y < h; ++y) {
            m[(size_t)y * w + x] = acc / n;
            if (y - r >= 0) { acc -= t[(size_t)(y - r) * w + x]; --n; }
            if (y + r + 1 < h) { acc += t[(size_t)(y + r + 1) * w + x]; ++n; }
        }
    }
}

// Places every image relative to the best connected one (maximum spanning tree of the inlier counts).
bool PlaceImages(PhotoJob& J, align::Model model) {
    const int n = (int)J.imgs.size();
    std::vector<std::vector<align::Feature>> feats(n);
    for (int i = 0; i < n; ++i) {
        auto& im = J.imgs[i];
        align::Gray g = align::ToGray(im.rgba.data(), im.w, im.h);
        feats[i] = align::Detect(g);
        if (J.cylindrical)
            for (auto& f : feats[i]) {
                float2 q = align::ToCylinder({f.x, f.y}, J.focal, im.w * 0.5f, im.h * 0.5f);
                f.x = q.x;
                f.y = q.y;
            }
        J.progress = 0.1f + 0.3f * (i + 1) / n;
    }
    std::vector<align::Result> pair((size_t)n * n);  // [i * n + j]: j -> i
    int done = 0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            if (j < i) {
                // Reuse the inverse of i -> j.
                const auto& r = pair[(size_t)j * n + i];
                if (r.ok) {
                    align::Mat3 inv;
                    if (align::Invert(r.m, inv)) pair[(size_t)i * n + j] = {true, inv, r.matches, r.inliers};
                }
                continue;
            }
            pair[(size_t)i * n + j] = align::Estimate(feats[i], feats[j], model, 3.0f);
            J.progress = 0.4f + 0.4f * (++done) / std::max(1, n * (n - 1) / 2);
        }
    // Reference: the most connected image.
    int ref = 0, bestSum = -1;
    for (int i = 0; i < n; ++i) {
        int s = 0;
        for (int j = 0; j < n; ++j) if (i != j && pair[(size_t)i * n + j].ok && pair[(size_t)i * n + j].inliers >= 12) s += pair[(size_t)i * n + j].inliers;
        if (s > bestSum) { bestSum = s; ref = i; }
    }
    J.imgs[ref].placed = true;
    J.imgs[ref].m = align::Identity();
    for (int added = 1; added < n; ++added) {
        int bi = -1, bj = -1, bInl = 11;
        for (int i = 0; i < n; ++i) {
            if (!J.imgs[i].placed) continue;
            for (int j = 0; j < n; ++j) {
                if (J.imgs[j].placed) continue;
                const auto& r = pair[(size_t)i * n + j];
                if (r.ok && r.inliers > bInl) { bInl = r.inliers; bi = i; bj = j; }
            }
        }
        if (bj < 0) break;
        J.imgs[bj].m = align::Mul(J.imgs[bi].m, pair[(size_t)bi * n + bj].m);
        J.imgs[bj].placed = true;
    }
    return true;
}

}  // namespace

// ------------------------------------------------------------------ start

void App::StartPhotoJob(int kind, DocView* v) {
    if (photoJob && photoJob->kind) { Status("Une opération photo est déjà en cours"); return; }
    auto J = std::make_shared<PhotoJob>();
    J->kind = kind;
    J->view = v;
    J->files = photoDlg.files;
    J->layout = photoDlg.layout;
    J->blend = photoDlg.blend;
    J->blendMode = photoDlg.blendMode;
    photoJob = J;
    if (kind == 2 || kind == 3) {
        // Layers: read back every visible pixel layer (bottom to top).
        Document& d = *v->doc;
        for (auto& l : d.S().layers) {
            if (l.kind != LayerKind::Pixel || !l.visible || !l.surface) continue;
            Surface* s = d.GetSurface(l.surface);
            if (!s) continue;
            PhotoJob::Img im;
            im.name = l.name;
            im.layer = l.id;
            im.surface = l.surface;
            im.w = (int)d.Width();
            im.h = (int)d.Height();
            im.rb = v->renderer.ExportRGBA8(*ctx, s->tex.get());
            J->imgs.push_back(std::move(im));
        }
        if (J->imgs.size() < 2) { Status("Il faut au moins deux calques de pixels visibles"); photoJob.reset(); return; }
    }
    Status(kind == 1 ? "Photomerge : analyse des images..." : kind == 2 ? "Alignement automatique des calques..." : kind == 3 ? "Fusion automatique des calques..." : "Fusion HDR : analyse des images...");
}

void App::UpdatePhotoJob() {
    if (!photoJob) return;
    PhotoJob& J = *photoJob;
    if (J.view) {
        bool alive = false;
        for (auto& dv : docs) alive |= dv.get() == J.view;
        if (!alive) { if (J.work.valid()) J.work.wait(); photoJob.reset(); return; }
    }
    auto finishError = [&](const std::string& msg) {
        Status(msg);
        photoJob.reset();
    };
    if (J.stage == 0) {
        // Inputs: files decoded on the worker, or layer readbacks to wait for.
        for (auto& im : J.imgs) if (im.rb && !im.rb->ready) return;
        for (auto& im : J.imgs)
            if (im.rb) {
                im.rgba = std::move(im.rb->data);
                im.rb.reset();
            }
        auto job = photoJob;
        J.work = std::async(std::launch::async, [job]() -> bool {
            PhotoJob& J = *job;
            if (J.kind == 1 || J.kind == 4) {
                for (auto& f : J.files) {
                    PhotoJob::Img im;
                    int c = 0;
                    u8* p = stbi_load(f.c_str(), &im.w, &im.h, &c, 4);
                    if (!p) {
                        // WIC (TIFF, RAW...): linear float -> 8-bit sRGB for the analysis.
                        std::vector<float> fl;
                        if (!LoadPhotoFloat(f, fl, im.w, im.h, nullptr, nullptr)) { J.error = "illisible : " + f; return false; }
                        im.rgba.resize((size_t)im.w * im.h * 4);
                        for (size_t i = 0; i < fl.size(); ++i) {
                            float v = std::clamp(fl[i], 0.0f, 1.0f);
                            if (i % 4 != 3) v = v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1 / 2.4f) - 0.055f;
                            im.rgba[i] = (u8)std::lround(v * 255);
                        }
                    } else {
                        im.rgba.assign(p, p + (size_t)im.w * im.h * 4);
                        stbi_image_free(p);
                    }
                    im.name = Narrow(Utf8Path(f).filename().wstring());
                    J.imgs.push_back(std::move(im));
                    J.progress = 0.1f * J.imgs.size() / J.files.size();
                }
                if (J.imgs.size() < 2) { J.error = "il faut au moins deux images"; return false; }
            }
            if (J.kind == 1 || J.kind == 2) {
                int maxW = 0;
                for (auto& im : J.imgs) maxW = std::max(maxW, im.w);
                J.cylindrical = J.kind == 1 && (J.layout == 2 || (J.layout == 0 && J.imgs.size() >= 3));
                J.focal = maxW * 1.1f;
                align::Model model = J.layout == 3 ? align::Model::Translation : J.cylindrical ? align::Model::Similarity : align::Model::Homography;
                PlaceImages(J, model);
                int placed = 0;
                for (auto& im : J.imgs) placed += im.placed;
                if (placed < 2) { J.error = "aucune correspondance fiable entre les images"; return false; }
                if (J.kind == 2) return true;
                // Canvas: bounding box of the placed images (limited to 16384 px).
                float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
                for (auto& im : J.imgs) {
                    if (!im.placed) continue;
                    for (int k = 0; k <= 32; ++k) {
                        float t = k / 32.0f;
                        for (float2 p : {float2{t * im.w, 0}, float2{t * im.w, (float)im.h}, float2{0, t * im.h}, float2{(float)im.w, t * im.h}}) {
                            float2 q = ImageToCanvas(J, im, p);
                            x0 = std::min(x0, q.x); y0 = std::min(y0, q.y); x1 = std::max(x1, q.x); y1 = std::max(y1, q.y);
                        }
                    }
                }
                double sc = std::min(1.0, 16384.0 / std::max(1.0, (double)std::max(x1 - x0, y1 - y0)));
                align::Mat3 T = {sc, 0, -x0 * sc, 0, sc, -y0 * sc, 0, 0, 1};
                for (auto& im : J.imgs) im.m = align::Mul(T, im.m);
                J.canvasW = std::max(1, (int)std::ceil((x1 - x0) * sc));
                J.canvasH = std::max(1, (int)std::ceil((y1 - y0) * sc));
                for (auto& im : J.imgs) { J.canvasW = std::max(J.canvasW, im.w); J.canvasH = std::max(J.canvasH, im.h); }
                if (!J.blend) return true;
                // Blend weights at low resolution: distance to the image border, sharpened into seams.
                int s = 1;
                while ((long long)(J.canvasW / s) * (J.canvasH / s) > 2500000) ++s;
                J.maskW = J.canvasW / s;
                J.maskH = J.canvasH / s;
                for (auto& im : J.imgs) {
                    if (!im.placed) continue;
                    align::Mat3 inv;
                    if (!align::Invert(im.m, inv)) continue;
                    im.mask.assign((size_t)J.maskW * J.maskH, 0.0f);
                    const float half = 0.5f * std::min(im.w, im.h);
                    for (int y = 0; y < J.maskH; ++y)
                        for (int x = 0; x < J.maskW; ++x) {
                            float2 src;
                            if (!CanvasToImage(J, im, inv, {(x + 0.5f) * s, (y + 0.5f) * s}, src)) continue;
                            float d = std::min(std::min(src.x, im.w - src.x), std::min(src.y, im.h - src.y));
                            im.mask[(size_t)y * J.maskW + x] = std::clamp(d / half, 1e-4f, 1.0f);
                        }
                }
                NormalizeMasks(J, 4.0f);
                J.progress = 1;
                return true;
            }
            if (J.kind == 3) {
                // Auto-blend: panorama (coverage distance) or stack (local sharpness), low resolution maps.
                int s = J.blendMode == 1 ? 1 : 2;
                while ((long long)(J.imgs[0].w / s) * (J.imgs[0].h / s) > 4000000) ++s;
                J.maskW = J.imgs[0].w / s;
                J.maskH = J.imgs[0].h / s;
                for (auto& im : J.imgs) {
                    std::vector<float> lum((size_t)J.maskW * J.maskH), alpha(lum.size());
                    for (int y = 0; y < J.maskH; ++y)
                        for (int x = 0; x < J.maskW; ++x) {
                            const u8* p = &im.rgba[((size_t)(y * s) * im.w + x * s) * 4];
                            lum[(size_t)y * J.maskW + x] = (0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]) / 255.0f;
                            alpha[(size_t)y * J.maskW + x] = p[3] / 255.0f;
                        }
                    im.mask.assign(lum.size(), 0.0f);
                    if (J.blendMode == 1) {
                        // Focus: Laplacian energy, smoothed.
                        for (int y = 1; y < J.maskH - 1; ++y)
                            for (int x = 1; x < J.maskW - 1; ++x) {
                                size_t i = (size_t)y * J.maskW + x;
                                float l = 4 * lum[i] - lum[i - 1] - lum[i + 1] - lum[i - J.maskW] - lum[i + J.maskW];
                                im.mask[i] = l * l;
                            }
                        BoxBlur(im.mask, J.maskW, J.maskH, std::max(2, J.maskW / 200));
                        for (size_t i = 0; i < im.mask.size(); ++i) im.mask[i] = (im.mask[i] + 1e-9f) * (alpha[i] > 0.5f ? 1.0f : 0.0f);
                    } else {
                        // Seams away from the edges: blurred coverage.
                        std::vector<float> cov = alpha;
                        BoxBlur(cov, J.maskW, J.maskH, std::max(4, J.maskW / 40));
                        for (size_t i = 0; i < cov.size(); ++i) im.mask[i] = alpha[i] > 0.5f ? std::max(cov[i] - 0.5f, 1e-4f) : 0.0f;
                    }
                }
                if (J.blendMode == 1) {
                    // Normalize sharpness per pixel, then the max wins (with a soft transition).
                    for (size_t i = 0; i < J.imgs[0].mask.size(); ++i) {
                        float mx = 1e-12f;
                        for (auto& im : J.imgs) mx = std::max(mx, im.mask[i]);
                        for (auto& im : J.imgs) im.mask[i] /= mx;
                    }
                }
                NormalizeMasks(J, J.blendMode == 1 ? 12.0f : 4.0f);
                if (J.blendMode == 1)
                    for (auto& im : J.imgs) BoxBlur(im.mask, J.maskW, J.maskH, std::max(1, J.maskW / 400));
                return true;
            }
            // HDR: order by brightness, align on the middle exposure, estimate the exposure ratios, merge.
            std::vector<double> mean;
            for (auto& im : J.imgs) {
                double s = 0;
                for (size_t i = 0; i < im.rgba.size(); i += 4) s += im.rgba[i] + im.rgba[i + 1] + im.rgba[i + 2];
                mean.push_back(s);
            }
            std::vector<int> order(J.imgs.size());
            for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
            std::sort(order.begin(), order.end(), [&](int a, int b) { return mean[a] < mean[b]; });
            std::vector<PhotoJob::Img> sorted;
            for (int i : order) sorted.push_back(std::move(J.imgs[i]));
            J.imgs = std::move(sorted);
            const int ref = (int)J.imgs.size() / 2;
            const int W = J.imgs[ref].w, H = J.imgs[ref].h;
            for (auto& im : J.imgs) if (im.w != W || im.h != H) { J.error = "les images doivent avoir la même taille"; return false; }
            std::vector<std::vector<align::Feature>> feats;
            for (auto& im : J.imgs) feats.push_back(align::Detect(align::ToGray(im.rgba.data(), im.w, im.h)));
            float lut[256];
            for (int i = 0; i < 256; ++i) lut[i] = SrgbLin(i / 255.0f);
            // Aligned linear copies (bilinear, inverse mapping).
            std::vector<std::vector<float>> lin(J.imgs.size());
            for (size_t k = 0; k < J.imgs.size(); ++k) {
                auto& im = J.imgs[k];
                align::Mat3 m = align::Identity();
                if ((int)k != ref) {
                    auto r = align::Estimate(feats[ref], feats[k], align::Model::Similarity, 2.0f);
                    if (r.ok && r.inliers >= 12) m = r.m;
                }
                align::Mat3 inv;
                align::Invert(m, inv);
                lin[k].assign((size_t)W * H * 4, -1.0f);  // -1: outside
                for (int y = 0; y < H; ++y)
                    for (int x = 0; x < W; ++x) {
                        float2 s = align::Apply(inv, {x + 0.5f, y + 0.5f});
                        float fx = s.x - 0.5f, fy = s.y - 0.5f;
                        int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
                        if (x0 < 0 || y0 < 0 || x0 + 1 >= W || y0 + 1 >= H) continue;
                        float ax = fx - x0, ay = fy - y0;
                        for (int c = 0; c < 3; ++c) {
                            auto P = [&](int xx, int yy) { return (float)im.rgba[((size_t)yy * W + xx) * 4 + c]; };
                            float v = (P(x0, y0) * (1 - ax) + P(x0 + 1, y0) * ax) * (1 - ay) + (P(x0, y0 + 1) * (1 - ax) + P(x0 + 1, y0 + 1) * ax) * ay;
                            lin[k][((size_t)y * W + x) * 4 + c] = v;  // 0..255 encoded for now
                        }
                    }
                J.progress = 0.2f + 0.5f * (k + 1) / J.imgs.size();
            }
            // Exposure ratios against the reference (median over well exposed pixels).
            J.exposures.assign(J.imgs.size(), 1.0f);
            for (size_t k = 0; k < J.imgs.size(); ++k) {
                if ((int)k == ref) continue;
                std::vector<float> ratios;
                for (size_t i = 0; i < (size_t)W * H * 4; i += 4 * 7) {
                    for (int c = 0; c < 3; ++c) {
                        float a = lin[k][i + c], b = lin[ref][i + c];
                        if (a < 20 || a > 235 || b < 20 || b > 235) continue;
                        ratios.push_back(lut[(int)a] / std::max(lut[(int)b], 1e-4f));
                    }
                }
                if (ratios.size() > 100) {
                    std::nth_element(ratios.begin(), ratios.begin() + ratios.size() / 2, ratios.end());
                    J.exposures[k] = ratios[ratios.size() / 2];
                }
            }
            J.hdr.assign((size_t)W * H * 4, 0.0f);
            for (size_t p = 0; p < (size_t)W * H; ++p) {
                for (int c = 0; c < 3; ++c) {
                    float num = 0, den = 0;
                    for (size_t k = 0; k < J.imgs.size(); ++k) {
                        float z = lin[k][p * 4 + c];
                        if (z < 0) continue;
                        float t = z / 255.0f;
                        float w = 1.0f - std::pow(std::abs(2 * t - 1), 6.0f);
                        float v = lut[std::clamp((int)std::lround(z), 0, 255)] / J.exposures[k];
                        num += w * v;
                        den += w;
                    }
                    if (den < 1e-4f) {
                        // Clipped everywhere: darkest image for highlights, brightest for shadows.
                        float zr = lin[ref][p * 4 + c];
                        int k = zr > 127 ? 0 : (int)J.imgs.size() - 1;
                        float z = std::max(lin[k][p * 4 + c], 0.0f);
                        num = lut[std::clamp((int)std::lround(z), 0, 255)] / J.exposures[k];
                        den = 1;
                    }
                    J.hdr[p * 4 + c] = num / den;
                }
                J.hdr[p * 4 + 3] = 1;
            }
            J.canvasW = W;
            J.canvasH = H;
            J.progress = 1;
            return true;
        });
        J.stage = 1;
        return;
    }
    if (J.stage == 1) {
        if (J.work.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            char b[96];
            snprintf(b, sizeof(b), "%s %.0f %%", J.kind == 1 ? "Photomerge :" : J.kind == 4 ? "Fusion HDR :" : "Analyse :", J.progress.load() * 100);
            Status(b);
            return;
        }
        if (!J.work.get()) { finishError("Échec : " + J.error); return; }
        ApplyPhotoJob(J);
        photoJob.reset();
    }
}

void App::WriteMaskData(Document& d, Id surface, const std::vector<float>& m, int mw, int mh) {
    Surface* s = d.GetSurface(surface);
    if (!s || m.size() != (size_t)mw * mh) return;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    auto sample = [&](u32 x, u32 y) {
        float fx = (x + 0.5f) * mw / W - 0.5f, fy = (y + 0.5f) * mh / H - 0.5f;
        int x0 = std::clamp((int)std::floor(fx), 0, mw - 1), y0 = std::clamp((int)std::floor(fy), 0, mh - 1);
        int x1 = std::min(x0 + 1, mw - 1), y1 = std::min(y0 + 1, mh - 1);
        float ax = std::clamp(fx - x0, 0.0f, 1.0f), ay = std::clamp(fy - y0, 0.0f, 1.0f);
        float a = m[(size_t)y0 * mw + x0] * (1 - ax) + m[(size_t)y0 * mw + x1] * ax;
        float b = m[(size_t)y1 * mw + x0] * (1 - ax) + m[(size_t)y1 * mw + x1] * ax;
        return std::clamp(a * (1 - ay) + b * ay, 0.0f, 1.0f);
    };
    if (s->tex->GetFormat() == gpu::Format::RGBA32F) {
        std::vector<float> px((size_t)W * H * 4);
        for (u32 y = 0; y < H; ++y)
            for (u32 x = 0; x < W; ++x) { float v = sample(x, y); for (int k = 0; k < 4; ++k) px[((size_t)y * W + x) * 4 + k] = v; }
        ctx->Upload(s->tex.get(), px.data(), W * 16);
    } else {
        std::vector<u16> px((size_t)W * H * 4);
        auto toHalf = [](float v) -> u16 {
            if (v <= 0) return 0;
            u32 b;
            memcpy(&b, &v, 4);
            int e = (int)((b >> 23) & 255) - 127 + 15;
            if (e <= 0) return 0;
            if (e >= 31) return 0x7BFF;
            return (u16)((e << 10) | ((b >> 13) & 0x3FF));
        };
        for (u32 y = 0; y < H; ++y)
            for (u32 x = 0; x < W; ++x) { u16 v = toHalf(sample(x, y)); for (int k = 0; k < 4; ++k) px[((size_t)y * W + x) * 4 + k] = v; }
        ctx->Upload(s->tex.get(), px.data(), W * 8);
    }
    d.CancelPendingClear(surface);
    d.TouchSurface(surface);
}

void App::ApplyPhotoJob(PhotoJob& J) {
    if (J.kind == 1) {
        NewDocument((u32)J.canvasW, (u32)J.canvasH, Precision::Half16, DocMode::Layers, 3, "Panorama");
        DocView* v = Active();
        Document& d = *v->doc;
        int n = 0;
        for (auto& im : J.imgs) {
            if (!im.placed) continue;
            Layer& l = d.AddPixelLayer(im.name);
            Id lid = l.id, surf = l.surface;
            d.pendingClears.clear();
            v->renderer.Prepare(d, *ctx);
            v->renderer.Import(d, *ctx, surf, im.rgba.data(), (u32)im.w, (u32)im.h, false, {0, 0});
            Surface* s = d.GetSurface(surf);
            auto original = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "PanoSource");
            ctx->CopyTexture(original.get(), s->tex.get());
            WarpMesh mesh;
            mesh.nx = mesh.ny = 48;
            for (int y = 0; y <= 48; ++y)
                for (int x = 0; x <= 48; ++x) {
                    float2 p{im.w * x / 48.0f, im.h * y / 48.0f};
                    mesh.src.push_back(p);
                    mesh.dst.push_back(ImageToCanvas(J, im, p));
                }
            v->renderer.WarpPreview(d, *ctx, surf, original.get(), nullptr, Rect{0, 0, im.w, im.h}, mesh, false);
            if (J.blend && !im.mask.empty()) {
                d.AddMask(lid);
                d.S().editMask = false;
                v->renderer.Prepare(d, *ctx);
                WriteMaskData(d, d.FindLayer(lid)->mask, im.mask, J.maskW, J.maskH);
            }
            ++n;
        }
        d.SyncGraph();
        v->history.Clear();
        d.dirty = true;
        Status("Panorama : " + std::to_string(n) + "/" + std::to_string(J.imgs.size()) + " images assemblées" + (J.cylindrical ? " (cylindrique)" : ""));
        return;
    }
    if (J.kind == 4) {
        NewDocument((u32)J.canvasW, (u32)J.canvasH, Precision::Float32, DocMode::Layers, 3, "Fusion HDR");
        DocView* v = Active();
        Document& d = *v->doc;
        Layer& l = d.AddPixelLayer("Fusion HDR");
        Id surf = l.surface, lid = l.id;
        d.pendingClears.clear();
        v->renderer.Prepare(d, *ctx);
        v->renderer.Import(d, *ctx, surf, J.hdr.data(), (u32)J.canvasW, (u32)J.canvasH, true, {0, 0});
        if (auto def = Effects().Find("camera_raw")) {
            EffectInstance fx;
            fx.effectId = "camera_raw";
            fx.params = def->DefaultParams();
            if (Layer* lp = d.FindLayer(lid)) lp->filters.push_back(fx);
            d.SyncGraph();
        }
        v->history.Clear();
        std::string ev;
        for (float e : J.exposures) {
            char b[24];
            snprintf(b, sizeof(b), "%s%+.1f", ev.empty() ? "" : " / ", std::log2(std::max(e, 1e-6f)));
            ev += b;
        }
        Status("Fusion HDR (32 bits) : " + std::to_string(J.imgs.size()) + " images, écarts d'exposition " + ev + " IL ; réglez le rendu avec le filtre Photo RAW");
        return;
    }
    DocView* v = J.view;
    Document& d = *v->doc;
    if (J.kind == 2) {
        v->history.BeginGroup();
        int moved = 0;
        for (auto& im : J.imgs) {
            if (!im.placed) continue;
            bool identity = true;
            align::Mat3 I = align::Identity();
            for (int k = 0; k < 9; ++k) identity &= std::abs(im.m[k] - I[k]) < 1e-4;
            if (identity) continue;
            Surface* s = d.GetSurface(im.surface);
            if (!s) continue;
            auto before = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "UndoBefore");
            ctx->CopyTexture(before.get(), s->tex.get());
            WarpMesh mesh;
            mesh.nx = mesh.ny = 16;
            for (int y = 0; y <= 16; ++y)
                for (int x = 0; x <= 16; ++x) {
                    float2 p{im.w * x / 16.0f, im.h * y / 16.0f};
                    mesh.src.push_back(p);
                    mesh.dst.push_back(align::Apply(im.m, p));
                }
            v->renderer.WarpPreview(d, *ctx, im.surface, before.get(), nullptr, Rect{0, 0, im.w, im.h}, mesh, false);
            auto after = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "UndoAfter");
            ctx->CopyTexture(after.get(), s->tex.get());
            v->history.PushPaint("Alignement automatique", im.surface, {0, 0, im.w, im.h}, before, after);
            ++moved;
        }
        v->history.EndGroup();
        int failed = 0;
        for (auto& im : J.imgs) failed += !im.placed;
        Status("Alignement automatique : " + std::to_string(moved) + " calque(s) déplacé(s)" + (failed ? ", " + std::to_string(failed) + " sans correspondance" : ""));
        return;
    }
    // Auto-blend: one new mask per layer.
    Edit e(d, v->history, J.blendMode == 1 ? "Fusion automatique (empilement)" : "Fusion automatique (panorama)");
    std::vector<std::pair<Id, size_t>> todo;
    for (size_t i = 0; i < J.imgs.size(); ++i) {
        Layer* l = d.FindLayer(J.imgs[i].layer);
        if (!l) continue;
        l->mask = d.CreateSurface({1, 1, 1, 1});
        l->maskEnabled = true;
        todo.push_back({l->mask, i});
    }
    d.S().editMask = false;
    v->renderer.Prepare(d, *ctx);
    for (auto& [mask, i] : todo) WriteMaskData(d, mask, J.imgs[i].mask, J.maskW, J.maskH);
    d.SyncGraph();
    e.Commit();
    Status("Fusion automatique : masques créés sur " + std::to_string(todo.size()) + " calques");
}

// ------------------------------------------------------------------ RAW / WIC

bool App::OpenWithWic(const std::string& path, bool asLayer) {
    std::vector<float> px;
    int w = 0, h = 0;
    std::string err;
    RawInfo info;
    std::string ext = Narrow(Utf8Path(path).extension().wstring());
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    const bool raw = IsRawExtension(ext);
    if (!LoadPhotoFloat(path, px, w, h, &info, &err)) {
        Status("Impossible d'ouvrir " + path + " : " + err);
        return false;
    }
    std::string name = Narrow(Utf8Path(path).filename().wstring());
    DocView* v = Active();
    Id layerId = 0;
    if (!asLayer || !v) {
        NewDocument((u32)w, (u32)h, Precision::Float32, DocMode::Layers, 3, name);
        v = Active();
        Layer& l = v->doc->AddPixelLayer(raw ? "Développement RAW" : "Arrière-plan");
        layerId = l.id;
        v->doc->pendingClears.clear();
        v->renderer.Prepare(*v->doc, *ctx);
        v->renderer.Import(*v->doc, *ctx, l.surface, px.data(), (u32)w, (u32)h, true, {0, 0});
        v->doc->path = path;
    } else {
        Document& d = *v->doc;
        DocState before = d.S();
        Layer& l = d.AddPixelLayer(name);
        layerId = l.id;
        Id surface = l.surface;
        d.pendingClears.pop_back();
        d.pendingClears.push_back({surface, {0, 0, 0, 0}});
        v->renderer.Prepare(d, *ctx);
        v->renderer.Import(d, *ctx, surface, px.data(), (u32)w, (u32)h, true, {((int)d.Width() - w) / 2, ((int)d.Height() - h) / 2});
        v->history.PushState("Importer " + name, before, d.S());
    }
    if (raw) {
        if (auto def = Effects().Find("camera_raw")) {
            EffectInstance fx;
            fx.effectId = "camera_raw";
            fx.params = def->DefaultParams();
            if (Layer* l = v->doc->FindLayer(layerId)) l->filters.push_back(fx);
            v->doc->SyncGraph();
        }
        v->doc->dirty = false;
        if (!asLayer) {
            Document::Shot& s = v->doc->shot;
            s.make = info.make;
            s.model = info.model;
            s.lens = info.lens;
            s.focal = info.focal;
            s.aperture = info.aperture;
            s.crop = info.cropFactor;
            s.summary = info.Summary();
        }
        Status("RAW ouvert (32 bits linéaire" + std::string(info.model.empty() ? "" : ", " + info.Summary()) + ")");
        if (v->doc->ActiveLayer() && v->doc->ActiveLayer()->id == layerId && !Automated() && !batch.running) OpenCameraRaw(*v);
    } else {
        Status("Ouvert : " + path);
    }
    if (!asLayer) AddRecent(path);
    return true;
}

// ------------------------------------------------------------------ dialog

void App::DrawPhotoDialog() {
    auto& P = photoDlg;
    if (!P.open) return;
    static const char* titles[] = {"", "Photomerge", "Alignement automatique des calques", "Fusion automatique des calques", "Fusion HDR"};
    ImGui::SetNextWindowSize(ImVec2(560 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin((std::string(titles[std::clamp(P.kind, 1, 4)]) + "###photodlg").c_str(), &P.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    const bool files = P.kind == 1 || P.kind == 4;
    if (files) {
        ImGui::SeparatorText("Images");
        ImGui::BeginChild("##files", ImVec2(520 * dpi, 150 * dpi), true);
        int remove = -1;
        for (int i = 0; i < (int)P.files.size(); ++i) {
            ImGui::PushID(i);
            if (ImGui::SmallButton("x")) remove = i;
            ImGui::SameLine();
            ImGui::TextUnformatted(Narrow(Utf8Path(P.files[i]).filename().wstring()).c_str());
            ImGui::PopID();
        }
        if (remove >= 0) P.files.erase(P.files.begin() + remove);
        if (P.files.empty()) ImGui::TextDisabled("Ajoutez des images (au moins deux).");
        ImGui::EndChild();
        if (ImGui::Button("Ajouter un fichier...")) {
            std::string f = platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tif;*.tiff;*.bmp;*.tga;*.dng;*.cr2;*.cr3;*.nef;*.arw;*.orf;*.rw2;*.raf\0Tous\0*.*\0");
            if (!f.empty()) P.files.push_back(f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Ajouter tout le dossier...")) {
            std::string f = platform::OpenFileDialog(hwnd, L"Une image du dossier\0*.png;*.jpg;*.jpeg;*.tif;*.tiff;*.bmp;*.tga\0Tous\0*.*\0");
            if (!f.empty()) {
                std::error_code ec;
                std::vector<std::string> add;
                for (auto& e : std::filesystem::directory_iterator(Utf8Path(f).parent_path(), ec)) {
                    std::string x = Narrow(e.path().extension().wstring());
                    for (auto& c : x) c = (char)tolower((unsigned char)c);
                    if (x == ".png" || x == ".jpg" || x == ".jpeg" || x == ".tif" || x == ".tiff" || x == ".bmp" || x == ".tga") add.push_back(Narrow(e.path().wstring()));
                }
                std::sort(add.begin(), add.end());
                for (auto& a : add) P.files.push_back(a);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Documents ouverts"))
            for (auto& dv : docs) if (!dv->doc->path.empty() && std::filesystem::exists(Utf8Path(dv->doc->path))) P.files.push_back(dv->doc->path);
    }
    ImGui::SeparatorText("Options");
    if (P.kind == 1 || P.kind == 2) {
        if (P.kind == 1) ImGui::Combo("Disposition", &P.layout, "Automatique\0Perspective\0Cylindrique\0Repositionnement\0");
        else {
            int l = P.layout == 3 ? 1 : 0;
            if (ImGui::Combo("Projection", &l, "Automatique (perspective)\0Repositionnement (translation)\0")) P.layout = l == 1 ? 3 : 0;
        }
        if (P.kind == 1) ImGui::Checkbox("Fusionner les images (masques de fondu)", &P.blend);
    }
    if (P.kind == 3) {
        ImGui::RadioButton("Panorama", &P.blendMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Empiler les images (mise au point)", &P.blendMode, 1);
        ImGui::TextDisabled("Crée un masque par calque de pixels visible.");
    }
    if (P.kind == 4) ImGui::TextDisabled("Expositions différentes d'une même scène (trépied ou à main levée :\nles images sont alignées). Résultat : document 32 bits linéaire.");
    if (P.kind == 2) ImGui::TextDisabled("Aligne les calques de pixels visibles sur le calque du bas.");
    ImGui::Separator();
    bool busy = photoJob != nullptr;
    ImGui::BeginDisabled(busy || (files && P.files.size() < 2) || (!files && !Active()));
    if (ImGui::Button("OK", ImVec2(120 * dpi, 0))) {
        StartPhotoJob(P.kind, files ? nullptr : Active());
        P.open = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(120 * dpi, 0))) P.open = false;
    ImGui::End();
}

}  // namespace sx
