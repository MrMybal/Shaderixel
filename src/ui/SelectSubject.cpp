// Select > Subject (rembg segmentation model run by Python, installable from
// the app) and Select > Sky (image analysis: smooth bright region connected to
// the top of the image). Both edges are then refined with the guided filter.
#include "ui/App.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <filesystem>
#include <queue>

namespace sx {

namespace {
int RunHidden(const std::wstring& cmd) {
    // A console program without a window; returns its exit code (-1: could not start).
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    std::wstring line = cmd;
    if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

std::string SegmentScript() {
    for (const std::string& p : {ExeDir() + "/ai/segment.py", ExeDir() + "/../../tools/ai/segment.py"})
        if (std::filesystem::exists(Utf8Path(p))) return p;
    return "";
}

// Sky: flood fill from the top rows over smooth, bright pixels with small steps of color.
std::vector<u8> SkyMask(const u8* rgba, int W, int H) {
    int s = 1;
    while ((long long)(W / s) * (H / s) > (1ll << 21)) ++s;
    const int w = W / s, h = H / s;
    std::vector<float> L((size_t)w * h), R((size_t)w * h), G((size_t)w * h), B((size_t)w * h), grad((size_t)w * h, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const u8* p = rgba + ((size_t)(y * s) * W + x * s) * 4;
            size_t i = (size_t)y * w + x;
            R[i] = p[0] / 255.0f;
            G[i] = p[1] / 255.0f;
            B[i] = p[2] / 255.0f;
            L[i] = 0.2126f * R[i] + 0.7152f * G[i] + 0.0722f * B[i];
        }
    for (int y = 1; y + 1 < h; ++y)
        for (int x = 1; x + 1 < w; ++x) {
            auto l = [&](int dx, int dy) { return L[(size_t)(y + dy) * w + x + dx]; };
            float gx = l(1, -1) + 2 * l(1, 0) + l(1, 1) - l(-1, -1) - 2 * l(-1, 0) - l(-1, 1);
            float gy = l(-1, 1) + 2 * l(0, 1) + l(1, 1) - l(-1, -1) - 2 * l(0, -1) - l(1, -1);
            grad[(size_t)y * w + x] = std::sqrt(gx * gx + gy * gy) * s;
        }
    auto skyLike = [&](size_t i) { return L[i] > 0.25f && grad[i] < 0.35f; };
    std::vector<u8> m((size_t)w * h, 0);
    std::queue<int> q;
    for (int y = 0; y < std::max(1, h / 50); ++y)
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * w + x;
            if (skyLike(i)) { m[i] = 255; q.push((int)i); }
        }
    // Reference colour of the sky (mean of the seeds) bounds the drift of the fill.
    double mr = 0, mg = 0, mb = 0;
    size_t n = 0;
    for (size_t i = 0; i < m.size(); ++i) if (m[i]) { mr += R[i]; mg += G[i]; mb += B[i]; ++n; }
    if (!n) return {};
    mr /= n; mg /= n; mb /= n;
    while (!q.empty()) {
        int i = q.front();
        q.pop();
        int x = i % w, y = i / w;
        const int nx[4] = {x + 1, x - 1, x, x}, ny[4] = {y, y, y + 1, y - 1};
        for (int k = 0; k < 4; ++k) {
            if (nx[k] < 0 || ny[k] < 0 || nx[k] >= w || ny[k] >= h) continue;
            size_t j = (size_t)ny[k] * w + nx[k];
            if (m[j] || !skyLike(j)) continue;
            float step = std::abs(R[j] - R[i]) + std::abs(G[j] - G[i]) + std::abs(B[j] - B[i]);
            float drift = (float)(std::abs(R[j] - mr) + std::abs(G[j] - mg) + std::abs(B[j] - mb));
            if (step > 0.06f || drift > 0.6f) continue;
            m[j] = 255;
            q.push((int)j);
        }
    }
    std::vector<u8> out((size_t)W * H, 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) out[(size_t)y * W + x] = m[(size_t)std::min(y / s, h - 1) * w + std::min(x / s, w - 1)];
    return out;
}

// Segmentations whose document was closed: left to finish in the background (destroying a running
// std::async future would block the interface until Python ends), dropped once done.
std::vector<std::future<int>> abandonedSubjectJobs;

void PollAbandonedSubjectJobs() {
    for (size_t i = 0; i < abandonedSubjectJobs.size();)
        if (abandonedSubjectJobs[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready) abandonedSubjectJobs.erase(abandonedSubjectJobs.begin() + i);
        else ++i;
}

// The document the running job belongs to (the view address alone may be reused by a new document).
const Document* subjectDoc = nullptr;
u64 subjectDocVersion = 0;
unsigned subjectRun = 0;  // unique temporary file names: an abandoned run never writes the mask of the next one
}  // namespace

void App::StartSelectSubject(DocView& v, bool sky) {
    if (subjectJob.active) { Status("Analyse déjà en cours..."); return; }
    if (!v.lastImage) return;
    subjectJob = {};
    subjectJob.active = true;
    subjectJob.sky = sky;
    subjectJob.view = &v;
    subjectJob.rb = v.renderer.ExportRGBA8(*ctx, v.lastImage, true);
    if (!subjectJob.rb) {
        subjectJob = {};
        Status("Sélection : lecture de l'image impossible");
        return;
    }
    subjectDoc = v.doc.get();
    subjectDocVersion = v.doc->ContentVersion();
    Status(sky ? "Sélection du ciel : analyse de l'image..." : "Sélection du sujet : analyse (modèle de segmentation)...");
}

void App::UpdateSelectSubject() {
    PollAbandonedSubjectJobs();
    auto& J = subjectJob;
    if (!J.active) return;
    bool alive = false;
    for (auto& d : docs) alive |= d.get() == J.view && d->doc.get() == subjectDoc && d->doc->ContentVersion() >= subjectDocVersion;
    if (!alive) {
        if (J.proc.valid()) abandonedSubjectJobs.push_back(std::move(J.proc));  // not waited for here
        J = {};
        return;
    }
    DocView& v = *J.view;
    Document& d = *v.doc;
    std::vector<u8> cov;
    if (J.rb) {
        if (!J.rb->ready) return;
        const int W = (int)J.rb->width, H = (int)J.rb->height;
        if (W <= 0 || H <= 0 || J.rb->data.size() < (size_t)W * H * 4) { Status("Sélection : lecture de l'image impossible"); J = {}; return; }
        J.w = W;
        J.h = H;
        if (J.sky) {
            cov = SkyMask(J.rb->data.data(), W, H);
            J.rb.reset();
            if (cov.empty()) { Status("Aucun ciel trouvé (haut de l'image trop sombre ou trop texturé)"); J = {}; return; }
        } else {
            std::string script = SegmentScript();
            if (script.empty()) { Status("Script de segmentation introuvable (ai/segment.py)"); J = {}; return; }
            std::string dir = Narrow(std::filesystem::temp_directory_path().wstring());
            const std::string tag = std::to_string(GetCurrentProcessId()) + "_" + std::to_string(++subjectRun);
            J.in = dir + "\\shaderixel_subject_in_" + tag + ".png";
            J.out = dir + "\\shaderixel_subject_mask_" + tag + ".png";
            stbi_write_png(J.in.c_str(), W, H, 4, J.rb->data.data(), W * 4);
            J.rb.reset();
            std::error_code ec;
            std::filesystem::remove(Utf8Path(J.out), ec);
            std::wstring cmd = L"\"" + Widen(aiSettings.python) + L"\" \"" + Widen(script) + L"\" \"" + Widen(J.in) + L"\" \"" + Widen(J.out) + L"\"";
            J.proc = std::async(std::launch::async, [cmd] { return RunHidden(cmd); });
            return;
        }
    } else if (J.proc.valid()) {
        if (J.proc.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        int code = J.proc.get();
        std::error_code rec;
        std::filesystem::remove(Utf8Path(J.in), rec);  // per-run temporary files
        if (code != 0) {
            std::filesystem::remove(Utf8Path(J.out), rec);
            if (code == 3) {
                subjectInstallPrompt = true;
                Status("Sélection du sujet : le module rembg n'est pas installé (Sélection > Installer le modèle de sujet)");
            } else {
                Status(code == -1 ? "Python introuvable : réglez-le dans Assistant IA > Configuration" : "Sélection du sujet : échec de la segmentation (code " + std::to_string(code) + ")");
            }
            J = {};
            return;
        }
        int mw = 0, mh = 0, comp = 0;
        FILE* f = nullptr;
        stbi_uc* m = nullptr;
        if (_wfopen_s(&f, Widen(J.out).c_str(), L"rb") == 0 && f) {
            m = stbi_load_from_file(f, &mw, &mh, &comp, 1);
            fclose(f);
        }
        std::filesystem::remove(Utf8Path(J.out), rec);
        if (!m || mw != J.w || mh != J.h) {
            if (m) stbi_image_free(m);
            Status("Sélection du sujet : masque illisible");
            J = {};
            return;
        }
        cov.assign(m, m + (size_t)mw * mh);
        stbi_image_free(m);
    } else {
        J = {};
        return;
    }
    // Selection from the mask, then edges refined by the image (hair...).
    v.history.BeginGroup();
    v.renderer.SelectCoverage(d, *ctx, v.history, cov, {0, 0, J.w, J.h}, SelMode::Replace, 0, J.sky ? "Sélectionner le ciel" : "Sélectionner le sujet");
    if (Surface* sel = d.Selection(); sel && v.lastImage) {
        Renderer::RefineParams p;
        p.radius = J.sky ? 14.0f : 8.0f;
        p.eps = 0.002f;
        if (gpu::Texture* refined = v.renderer.RefineMask(*ctx, v.lastImage, sel->tex.get(), p))
            v.renderer.SelectFrom(d, *ctx, v.history, refined, SelSource::Mask, SelMode::Replace, {}, 0, "Affiner le contour");
    }
    v.history.EndGroup();
    Status(J.sky ? "Ciel sélectionné" : "Sujet sélectionné (affinez avec Sélectionner et masquer si besoin)");
    J = {};
}

void App::InstallSubjectModel() {
    if (subjectInstall.valid()) return;
    std::wstring cmd = L"\"" + Widen(aiSettings.python) + L"\" -m pip install rembg onnxruntime pillow";
    subjectInstall = std::async(std::launch::async, [cmd] { return RunHidden(cmd); });
    Status("Installation de rembg (pip) en arrière-plan...");
}

void App::UpdateSubjectInstall() {
    if (!subjectInstall.valid() || subjectInstall.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    int code = subjectInstall.get();
    Status(code == 0 ? "rembg installé : Sélection > Sujet (le modèle se télécharge au premier usage)" : "Installation de rembg échouée (code " + std::to_string(code) + ")");
}

}  // namespace sx
