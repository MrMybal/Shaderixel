// File > Automate > Contact Sheet (like Photoshop's Contact Sheet II): the images of a folder laid out as
// thumbnails in a grid, on as many pages as needed (a new document per page): the thumbnails on one layer, their file
// names below on another (two page-sized layers whatever the count); flattened on request. One image per frame,
// progress and cancel in the job window; an unreadable file takes no cell.
#include "ui/App.h"
#include "render/Dispatch.h"
#include "io/WicLoad.h"
#include "io/TextRaster.h"

#include <algorithm>
#include <cmath>
#include <imgui_stdlib.h>

namespace sx {

void App::DrawContactSheetDialog() {
    auto& C = contactSheet;
    if (!C.open) return;
    ImGui::SetNextWindowSize(ImVec2(460 * dpi, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Planche contact", &C.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    ImGui::SeparatorText("Images du dossier");
    ImGui::SetNextItemWidth(-90 * dpi);
    ImGui::InputTextWithHint("##folder", "dossier des images", &C.folder);
    ImGui::SameLine();
    if (ImGui::Button("Parcourir...")) {
        std::string f = platform::OpenFileDialog(hwnd, L"Un fichier du dossier\0*.*\0");
        if (!f.empty()) C.folder = Narrow(Utf8Path(f).parent_path().wstring());
    }
    ImGui::Checkbox("Inclure les sous-dossiers", &C.subfolders);
    ImGui::SeparatorText("Page");
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Largeur (px)", &C.width, 100);
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Hauteur (px)", &C.height, 100);
    if (ImGui::SmallButton("A4 300 ppp")) { C.width = 2480; C.height = 3508; }
    ImGui::SameLine();
    if (ImGui::SmallButton("A4 paysage")) { C.width = 3508; C.height = 2480; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Écran 1920 x 1080")) { C.width = 1920; C.height = 1080; }
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Colonnes", &C.cols);
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Lignes", &C.rows);
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Marge (px)", &C.margin, 10);
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Espacement (px)", &C.spacing, 5);
    C.width = std::clamp(C.width, 64, 16384);
    C.height = std::clamp(C.height, 64, 16384);
    C.cols = std::clamp(C.cols, 1, 20);
    C.rows = std::clamp(C.rows, 1, 20);
    C.margin = std::clamp(C.margin, 0, 2000);
    C.spacing = std::clamp(C.spacing, 0, 2000);
    ImGui::Checkbox("Nom du fichier sous chaque image", &C.captions);
    if (C.captions) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90 * dpi);
        ImGui::InputInt("Taille", &C.fontSize, 2);
        C.fontSize = std::clamp(C.fontSize, 6, 400);
    }
    ImGui::Checkbox("Aplatir chaque page", &C.flatten);
    ImGui::BeginDisabled(C.folder.empty() || C.running);
    if (ImGui::Button("Créer", ImVec2(120 * dpi, 0))) StartContactSheet();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Fermer", ImVec2(100 * dpi, 0))) C.open = false;
    ImGui::End();
}

void App::StartContactSheet() {
    auto& C = contactSheet;
    C.files.clear();
    static const char* exts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".psd", ".hdr", ".exr", ".gif", ".tif", ".tiff", ".webp"};
    auto consider = [&](const std::filesystem::path& p) {
        std::string e = Narrow(p.extension().wstring());
        for (auto& c : e) c = (char)tolower((unsigned char)c);
        bool image = IsRawExtension(e);
        for (auto* x : exts) image |= e == x;
        if (image) C.files.push_back(Narrow(p.wstring()));
    };
    std::error_code ec;
    if (C.subfolders) {
        for (auto& e : std::filesystem::recursive_directory_iterator(Utf8Path(C.folder), ec)) if (e.is_regular_file()) consider(e.path());
    } else {
        for (auto& e : std::filesystem::directory_iterator(Utf8Path(C.folder), ec)) if (e.is_regular_file()) consider(e.path());
    }
    std::sort(C.files.begin(), C.files.end());
    if (C.files.empty()) { Status("Planche contact : aucune image dans " + C.folder); return; }
    C.index = 0;
    C.placed = 0;
    C.page = nullptr;
    C.pages = 0;
    C.failed = 0;
    C.cancel = false;
    C.running = true;
    C.open = false;
}

namespace {
struct PerspRoot { u32 dw, dh, sw, sh; float h0[3]; u32 interp; float h1[3]; u32 pad0; float h2[3]; u32 pad1; };

// `src` (sw x sh) scaled by `sc` into a new w x h texture of `fmt` (supersampled when reduced, bicubic).
gpu::TexturePtr Scaled(gpu::CommandContext& ctx, gpu::Texture* src, u32 sw, u32 sh, float sc, u32 w, u32 h, gpu::Format fmt, u32 interp) {
    auto* k = Kernel("resample_persp.slang");
    auto out = k ? gpu::Dev().CreateTexture(w, h, fmt, "ContactSheet") : nullptr;
    if (!out) return nullptr;
    gpu::DispatchDesc dd;
    dd.pipeline = k;
    dd.srv[0] = src;
    dd.uav[0] = out.get();
    SetConstants(dd, PerspRoot{w, h, sw, sh, {1 / sc, 0, 0}, interp, {0, 1 / sc, 0}, 0, {0, 0, 1}, 0});
    Dispatch2D(ctx, dd, w, h);
    return out;
}

// Straight linear float RGBA -> a premultiplied RGBA32F texture (reduced by a whole factor when very large).
gpu::TexturePtr UploadFloat(gpu::CommandContext& ctx, std::vector<float>& px, int& w, int& h) {
    const int f = std::max(1, (std::max(w, h) + 4095) / 4096);
    if (f > 1) {
        const int nw = std::max(1, w / f), nh = std::max(1, h / f);
        std::vector<float> reduced((size_t)nw * nh * 4, 0.0f);
        for (int y = 0; y < nh; ++y)
            for (int x = 0; x < nw; ++x)
                for (int j = 0; j < f; ++j)
                    for (int i = 0; i < f; ++i)
                        for (int c = 0; c < 4; ++c) reduced[((size_t)y * nw + x) * 4 + c] += px[((size_t)(y * f + j) * w + (x * f + i)) * 4 + c] / (f * f);
        px.swap(reduced);
        w = nw;
        h = nh;
    }
    for (size_t i = 0; i < px.size(); i += 4)
        for (int c = 0; c < 3; ++c) px[i + c] *= px[i + 3];
    auto t = gpu::Dev().CreateTexture((u32)w, (u32)h, gpu::Format::RGBA32F, "ContactSheetSrc");
    if (t) ctx.Upload(t.get(), px.data(), (u32)w * 16);
    return t;
}
}  // namespace

void App::ContactSheetTick() {
    auto& C = contactSheet;
    if (!C.running) return;
    bool cancel = false;
    DrawJobModal("Planche contact", C.files.empty() ? 1.0f : (float)C.index / C.files.size(), &cancel);
    // The page in progress may have been closed meanwhile.
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == C.page;
    if (!alive) C.page = nullptr;
    const int perPage = C.cols * C.rows;
    auto finishPage = [&]() {
        if (!C.page) return;
        if (C.flatten) {
            Prepare(*C.page);
            SyncTextLayers(*C.page);
            MergeVisible(*C.page, true);
        }
        C.page->history.Clear();
        C.page->fitPending = true;
        C.page = nullptr;
    };
    if (cancel || C.cancel || C.index >= C.files.size()) {
        finishPage();
        C.running = false;
        Status("Planche contact : " + std::to_string(C.placed) + " image(s) sur " + std::to_string(C.pages) + " page(s)" +
               (C.failed ? ", " + std::to_string(C.failed) + " illisible(s)" : "") + (cancel || C.cancel ? " (arrêtée)" : ""));
        return;
    }
    const std::string file = C.files[C.index++];
    // The image: decoded first (an unreadable file takes no cell). TIFF, WebP, camera RAW through WIC.
    std::string ext = Narrow(Utf8Path(file).extension().wstring());
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    const bool wic = IsRawExtension(ext) || ext == ".tif" || ext == ".tiff" || ext == ".webp";
    gpu::TexturePtr img;
    u32 iw = 0, ih = 0;
    DocView* host = C.page ? C.page : Active();
    if (wic) {
        std::vector<float> px;
        int w = 0, h = 0;
        if (LoadWicFloat(file, px, w, h, nullptr) && w > 0 && h > 0) {
            img = UploadFloat(*ctx, px, w, h);
            iw = (u32)w;
            ih = (u32)h;
        }
    } else if (host) {
        img = LoadImageTexture(*host, file, iw, ih);
        loadedSubDoc.reset();  // (only the image is used)
    }
    if (!img || !iw || !ih) { ++C.failed; return; }
    // A new page when needed: the paper, the thumbnails' layer and the names' layer.
    const int slot = C.placed % perPage;
    if (slot == 0 || !C.page) {
        finishPage();
        NewDocument((u32)C.width, (u32)C.height, Precision::Half16, DocMode::Layers, 0, "Planche contact " + std::to_string(++C.pages));
        C.page = docs.back().get();
        Document& nd = *C.page->doc;
        C.thumbs = nd.AddPixelLayer("Vignettes", (int)nd.S().layers.size()).id;
        C.names = C.captions ? nd.AddPixelLayer("Noms", (int)nd.S().layers.size()).id : 0;
        nd.SyncGraph();
    }
    DocView& pv = *C.page;
    Document& pd = *pv.doc;
    Prepare(pv);
    const Layer* tl = pd.FindLayer(C.thumbs);
    Surface* thumbs = tl ? pd.GetSurface(tl->surface) : nullptr;
    if (!thumbs || !thumbs->tex) { ++C.failed; return; }
    ++C.placed;
    // The cell: the image and its caption centered together.
    const float W = (float)C.width, H = (float)C.height, m = (float)C.margin, s = (float)C.spacing;
    const float cellW = std::max(1.0f, (W - 2 * m - (C.cols - 1) * s) / C.cols), cellH = std::max(1.0f, (H - 2 * m - (C.rows - 1) * s) / C.rows);
    const float capH = C.captions ? C.fontSize * 1.5f : 0;
    const float areaH = std::max(1.0f, cellH - capH);
    const int col = slot % C.cols, row = slot / C.cols;
    const float cx = m + col * (cellW + s), cy = m + row * (cellH + s);
    const float sc = std::min(cellW / iw, areaH / ih);
    const u32 tw = std::max(1u, (u32)std::lround(iw * sc)), th = std::max(1u, (u32)std::lround(ih * sc));
    const int ox = (int)std::round(cx + (cellW - tw) * 0.5f), oy = (int)std::round(cy + (cellH - (th + capH)) * 0.5f);
    // The thumbnail drawn at its own size, then copied into its place (the other cells untouched).
    if (auto t = Scaled(*ctx, img.get(), iw, ih, (float)tw / iw, tw, th, thumbs->tex->GetFormat(), 2)) {
        const Rect r = {0, 0, std::min((int)tw, C.width - ox), std::min((int)th, C.height - oy)};
        if (ox >= 0 && oy >= 0 && r.x1 > 0 && r.y1 > 0) ctx->CopyRegion(thumbs->tex.get(), ox, oy, t.get(), r);
        pd.TouchSurface(tl->surface);
    }
    const Layer* nl = pd.FindLayer(C.names);
    Surface* names = nl ? pd.GetSurface(nl->surface) : nullptr;
    if (C.captions && names && names->tex) {
        // The name, shortened to fit the cell (about half an em per character), rasterized into the names' layer.
        std::string name = Narrow(Utf8Path(file).stem().wstring());
        const size_t fit = (size_t)std::max(4.0f, cellW / (C.fontSize * 0.55f));
        if (name.size() > fit) name = name.substr(0, fit - 3) + "...";
        TextStyle ts;
        ts.content = name;
        ts.size = (float)C.fontSize;
        ts.color = {0.1f, 0.1f, 0.1f, 1};
        ts.align = 1;
        ts.pos = {std::round(cx + cellW * 0.5f), std::round(oy + th + C.fontSize * 0.3f)};
        std::vector<float> px;
        Rect rect;
        if (RasterizeText(ts, C.width, C.height, px, rect) && !rect.Empty()) {
            int rw = rect.Width(), rh = rect.Height();
            if (auto up = UploadFloat(*ctx, px, rw, rh))
                if (auto t = Scaled(*ctx, up.get(), (u32)rw, (u32)rh, 1.0f, (u32)rw, (u32)rh, names->tex->GetFormat(), 0)) {
                    const int x0 = std::max(0, rect.x0), y0 = std::max(0, rect.y0);
                    const Rect r = {x0 - rect.x0, y0 - rect.y0, std::min(rw, C.width - rect.x0), std::min(rh, C.height - rect.y0)};
                    if (r.x1 > r.x0 && r.y1 > r.y0) ctx->CopyRegion(names->tex.get(), x0, y0, t.get(), r);
                    pd.TouchSurface(nl->surface);
                }
        }
    }
    pd.dirty = true;
}

}  // namespace sx
