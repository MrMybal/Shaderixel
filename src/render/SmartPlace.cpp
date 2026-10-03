// Smart objects on the GPU: contents imported at their own size, placed on a
// layer surface by a projective transform (shaders/smart_place.slang).
#include "render/Renderer.h"
#include "render/Dispatch.h"

#include <cmath>

namespace sx {

using gpu::Shaders;

namespace {
struct ImportRootS { u32 sw, sh; i32 ox, oy; u32 dw, dh, flags; };
struct PlaceCB {
    float4 h0, h1, h2;
    u32 w, h, super, pad;
};

void Mul3(const double* a, const double* b, double* out) {
    double r[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) r[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
    for (int k = 0; k < 9; ++k) out[k] = r[k];
}

bool Invert3(const double* m, double* out) {
    double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
    double A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    double det = a * A + b * B + c * C;
    if (std::abs(det) < 1e-12) return false;
    double inv = 1.0 / det;
    out[0] = A * inv;
    out[1] = -(b * i - c * h) * inv;
    out[2] = (b * f - c * e) * inv;
    out[3] = B * inv;
    out[4] = (a * i - c * g) * inv;
    out[5] = -(a * f - c * d) * inv;
    out[6] = C * inv;
    out[7] = -(a * h - b * g) * inv;
    out[8] = (a * e - b * d) * inv;
    return true;
}

// Unit square -> quad (Heckbert).
void SquareToQuad(const float2* q, double* m) {
    double sx = q[0].x - q[1].x + q[2].x - q[3].x, sy = q[0].y - q[1].y + q[2].y - q[3].y;
    double g = 0, h = 0;
    if (std::abs(sx) > 1e-9 || std::abs(sy) > 1e-9) {
        double dx1 = q[1].x - q[2].x, dx2 = q[3].x - q[2].x, dy1 = q[1].y - q[2].y, dy2 = q[3].y - q[2].y;
        double den = dx1 * dy2 - dx2 * dy1;
        if (std::abs(den) > 1e-12) {
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
}  // namespace

gpu::TexturePtr Renderer::ImportTexture(gpu::CommandContext& ctx, const void* pixels, u32 w, u32 h, bool isFloat, gpu::Format format) {
    auto* p = Shaders().CompileNow("import.slang", Shaders().File("import.slang"))->pipeline.get();
    if (!p || !w || !h) return nullptr;
    auto tmp = gpu::Dev().CreateTexture(w, h, isFloat ? gpu::Format::RGBA32F : gpu::Format::RGBA8, "ImportSrc");
    auto out = gpu::Dev().CreateTexture(w, h, format, "SmartContent");
    if (!tmp || !out) return nullptr;
    ctx.Upload(tmp.get(), pixels, w * (isFloat ? 16 : 4));
    gpu::DispatchDesc d;
    d.pipeline = p;
    d.srv[0] = tmp.get();
    d.uav[0] = out.get();
    SetConstants(d, ImportRootS{w, h, 0, 0, w, h, isFloat ? 0u : 1u});
    d.threadsX = w;
    d.threadsY = h;
    ctx.Dispatch(d);
    return out;
}

void Renderer::PlaceSmart(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* content, const float2 corners[4]) {
    Surface* s = doc.GetSurface(surface);
    auto* k = Kernel("smart_place.slang");
    if (!s || !content || !k) return;
    // Contents pixel -> unit square -> quad; inverted for the shader (layer pixel -> contents pixel).
    double m[9], hm[9], inv[9];
    SquareToQuad(corners, m);
    const double cw = content->Width(), ch = content->Height();
    double norm[9] = {1.0 / cw, 0, 0, 0, 1.0 / ch, 0, 0, 0, 1};
    Mul3(m, norm, hm);
    if (!Invert3(hm, inv)) return;
    PlaceCB cb;
    cb.h0 = {(float)inv[0], (float)inv[1], (float)inv[2], 0};
    cb.h1 = {(float)inv[3], (float)inv[4], (float)inv[5], 0};
    cb.h2 = {(float)inv[6], (float)inv[7], (float)inv[8], 0};
    cb.w = s->tex->Width();
    cb.h = s->tex->Height();
    // Shrunk contents: supersample.
    float area = std::abs((corners[1].x - corners[0].x) * (corners[3].y - corners[0].y) - (corners[1].y - corners[0].y) * (corners[3].x - corners[0].x));
    cb.super = area < cw * ch * 0.6 ? 2u : 1u;
    cb.pad = 0;
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = content;
    d.uav[0] = s->tex.get();
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    Dispatch2D(ctx, d, cb.w, cb.h);
    doc.TouchSurface(surface);
}

}  // namespace sx
