// Image > Content-aware scale: reads the composite (and the selection) back, carves seams on a reduced copy in
// a worker thread, then applies the maps to every layer on the GPU (Renderer::CarveCanvas, one history step).
#include "ui/App.h"
#include "render/SeamCarve.h"

#include <cmath>

namespace sx {

namespace {
constexpr u32 kCarveMaxSide = 1024;  // seams are computed on at most this size (maps interpolated to the image)
}

struct App::CarveJob {
    DocView* view = nullptr;
    gpu::ReadbackPtr rbImg, rbSel;
    u32 newW = 0, newH = 0;
    int mode = 0;
    std::thread th;
    std::atomic<float> progress{0};
    std::atomic<bool> done{false}, cancel{false};
    SeamCarveResult res;
    ~CarveJob() {
        cancel = true;
        if (th.joinable()) th.join();
    }
};

void App::StartContentScale(DocView& v, u32 w, u32 h, int mode) {
    if (carveJob) return;
    if (xf.active && xf.view) TransformCommit(*xf.view);
    Document& d = *v.doc;
    auto J = std::make_shared<CarveJob>();
    J->view = &v;
    J->newW = w;
    J->newH = h;
    J->mode = mode;
    J->rbImg = v.renderer.ExportRGBA8(*ctx, v.renderer.EvaluateComplete(d, *ctx));
    if (!J->rbImg) {
        Status("Mise à l'échelle selon le contenu impossible");
        return;
    }
    if (mode) {
        if (Surface* s = d.Selection()) J->rbSel = v.renderer.ExportRGBA8(*ctx, s->tex.get(), false);
        else J->mode = 0;
    }
    carveJob = J;
    Status("Mise à l'échelle selon le contenu...");
}

void App::ContentScaleTick() {
    if (!carveJob) return;
    CarveJob& J = *carveJob;
    bool alive = false;
    for (auto& dv : docs) if (dv.get() == J.view) alive = true;
    if (!alive) {
        carveJob.reset();
        return;
    }
    if (!J.th.joinable()) {
        if (!J.rbImg->ready || (J.rbSel && !J.rbSel->ready)) return;
        // Box-reduced copies of the image and the selection, the target sizes scaled the same way.
        const u32 W = J.rbImg->width, H = J.rbImg->height;
        const double f = std::min(1.0, (double)kCarveMaxSide / std::max(W, H));
        SeamCarveInput in;
        in.w = std::max(4u, (u32)std::lround(W * f));
        in.h = std::max(4u, (u32)std::lround(H * f));
        in.newW = std::clamp((u32)std::lround((double)J.newW * in.w / W), 2u, in.w * 2);
        in.newH = std::clamp((u32)std::lround((double)J.newH * in.h / H), 2u, in.h * 2);
        in.maskMode = J.rbSel ? J.mode : 0;
        in.rgba.resize((size_t)in.w * in.h * 4);
        if (in.maskMode) in.mask.resize((size_t)in.w * in.h);
        const u8* src = J.rbImg->data.data();
        const u8* sel = J.rbSel ? J.rbSel->data.data() : nullptr;
        const bool selOk = sel && J.rbSel->width == W && J.rbSel->height == H && J.rbSel->data.size() >= (size_t)W * H * 4;
        for (u32 y = 0; y < in.h; ++y) {
            const u32 y0 = y * H / in.h, y1 = std::max(y0 + 1, (y + 1) * H / in.h);
            for (u32 x = 0; x < in.w; ++x) {
                const u32 x0 = x * W / in.w, x1 = std::max(x0 + 1, (x + 1) * W / in.w);
                u32 acc[5] = {0, 0, 0, 0, 0}, n = 0;
                for (u32 sy = y0; sy < y1; ++sy)
                    for (u32 sx = x0; sx < x1; ++sx, ++n) {
                        const u8* p = src + ((size_t)sy * W + sx) * 4;
                        for (int c = 0; c < 4; ++c) acc[c] += p[c];
                        if (selOk) acc[4] += sel[((size_t)sy * W + sx) * 4];
                    }
                u8* o = &in.rgba[((size_t)y * in.w + x) * 4];
                for (int c = 0; c < 4; ++c) o[c] = (u8)(acc[c] / n);
                if (in.maskMode) in.mask[(size_t)y * in.w + x] = (u8)(acc[4] / n);
            }
        }
        J.rbImg.reset();
        J.rbSel.reset();
        CarveJob* jp = &J;
        J.th = std::thread([jp, in = std::move(in)] {
            jp->res = SeamCarve(in, &jp->progress, &jp->cancel);
            jp->done = true;
        });
        return;
    }
    if (!J.done) return;
    J.th.join();
    if (!J.res.error.empty()) {
        Status("Mise à l'échelle selon le contenu : " + J.res.error);
    } else if (J.view->renderer.CarveCanvas(*J.view->doc, *ctx, J.view->history, J.newW, J.newH, J.res, "Mise à l'échelle selon le contenu")) {
        J.view->fitPending = true;
        Status("Mise à l'échelle selon le contenu : " + std::to_string(J.newW) + " x " + std::to_string(J.newH));
    } else {
        Status("Mise à l'échelle selon le contenu impossible");
    }
    carveJob.reset();
}

// Progress (bottom of the canvas area) while the seams are computed.
void App::DrawContentScaleProgress() {
    if (!carveJob) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y - 60 * dpi), ImGuiCond_Always, ImVec2(0.5f, 1));
    ImGui::SetNextWindowBgAlpha(0.9f);
    if (ImGui::Begin("##carveprog", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::TextUnformatted("Mise à l'échelle selon le contenu");
        ImGui::ProgressBar(carveJob->progress.load(), ImVec2(260 * dpi, 0));
        if (ImGui::Button("Interrompre", ImVec2(260 * dpi, 0))) carveJob->cancel = true;
    }
    ImGui::End();
}

}  // namespace sx
