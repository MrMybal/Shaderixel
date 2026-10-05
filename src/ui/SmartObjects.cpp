// Smart objects (objets dynamiques): layers whose contents keep their own
// resolution and are placed by a projective transform recomputed from the
// source (lossless transforms), embedded (inner layered document edited in its
// own tab, the layer follows its edits) or linked to an image file (reloaded
// when the file changes). Filters on the layer act as smart filters.
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/DocFile.h"

#include <stb_image.h>

#include <filesystem>

namespace sx {

namespace {
long long FileTime(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(Utf8Path(path), ec);
    return ec ? 0 : (long long)t.time_since_epoch().count();
}

gpu::Format ContentFormat(const Document& d) { return d.GetPrecision() == Precision::Float32 ? gpu::Format::RGBA32F : gpu::Format::RGBA16F; }

// Contents fitted in the document, centered (corners TL TR BR BL).
void FitCorners(const Document& d, u32 w, u32 h, float2 out[4]) {
    float s = std::min(1.0f, std::min((float)d.Width() / w, (float)d.Height() / h));
    float cw = w * s, ch = h * s, x0 = (d.Width() - cw) * 0.5f, y0 = (d.Height() - ch) * 0.5f;
    out[0] = {x0, y0};
    out[1] = {x0 + cw, y0};
    out[2] = {x0 + cw, y0 + ch};
    out[3] = {x0, y0 + ch};
}
}  // namespace

gpu::TexturePtr App::LoadImageTexture(DocView& v, const std::string& path, u32& w, u32& h) {
    {
        std::string ext = Narrow(Utf8Path(path).extension().wstring());
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        if (ext == ".sxl" || ext == ".sx3d") {
            // SubGraph: the document is loaded off-tab and its output (the whole graph) baked as the contents.
            const int prevActive = activeDoc;
            const size_t before = docs.size();
            if (!LoadSxl(path) || docs.size() != before + 1) return nullptr;
            std::shared_ptr<DocView> hidden(std::move(docs.back()));
            docs.pop_back();
            activeDoc = prevActive;
            focusDocRequest = prevActive;
            hidden->renderer.Prepare(*hidden->doc, *ctx);
            SyncTextLayers(*hidden);
            SyncSmartLayers(*hidden);
            gpu::Texture* out = hidden->renderer.EvaluateComplete(*hidden->doc, *ctx);
            if (!out) return nullptr;
            w = hidden->doc->Width();
            h = hidden->doc->Height();
            gpu::TexturePtr tex = v.renderer.PreviewCopy(*ctx, out, w, h);
            deferredViews.push_back({frameCounter, hidden});  // its textures are read by this frame's commands
            loadedSubDoc = std::move(hidden->doc);
            return tex;
        }
    }
    int iw = 0, ih = 0;
    stbi_uc* px = LoadImagePixels(path, iw, ih);  // images and Unreal textures
    if (!px) return nullptr;
    w = (u32)iw;
    h = (u32)ih;
    auto tex = v.renderer.ImportTexture(*ctx, px, w, h, false, ContentFormat(*v.doc));
    stbi_image_free(px);
    return tex;
}

SmartObject* App::NewSmartObject(Document& d, const std::string& name, gpu::TexturePtr content) {
    auto so = std::make_shared<SmartObject>();
    so->id = d.NewId();
    so->name = name;
    so->content = std::move(content);
    so->width = so->content ? so->content->Width() : 0;
    so->height = so->content ? so->content->Height() : 0;
    d.smartObjects[so->id] = so;
    return so.get();
}

Layer* App::AddSmartLayer(DocView& v, SmartObject& so, const float2 corners[4]) {
    Document& d = *v.doc;
    Layer& l = d.AddPixelLayer(so.name);
    l.kind = LayerKind::Smart;
    l.smart = so.id;
    for (int i = 0; i < 4; ++i) l.smartCorners[i] = corners[i];
    d.SyncGraph();
    return &l;
}

void App::SyncSmartLayers(DocView& v) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) return;
    for (auto& l : d.S().layers) {
        if (l.kind != LayerKind::Smart || !l.surface) continue;
        SmartObject* so = d.GetSmart(l.smart);
        if (!so || !so->content) continue;
        u64 h = (u64)(uintptr_t)so->content.get() * 1099511628211ull ^ so->version * 0x9E3779B97F4A7C15ull ^ ((u64)d.Width() << 32) ^ d.Height() ^
                ((u64)l.smartInterp << 60);
        for (auto& c : l.smartCorners) {
            u32 bits[2];
            memcpy(bits, &c, 8);
            h = (h ^ bits[0]) * 1099511628211ull;
            h = (h ^ bits[1]) * 1099511628211ull;
        }
        auto it = v.textHashes.find(l.surface);
        if (it != v.textHashes.end() && it->second == h) continue;
        d.CancelPendingClear(l.surface);
        bool wasDirty = d.dirty;  // derived pixels: not a change of the document
        v.renderer.PlaceSmart(d, *ctx, l.surface, so->content.get(), l.smartCorners, l.smartInterp);
        d.dirty = wasDirty;
        v.textHashes[l.surface] = h;
    }
}

void App::ConvertToSmartObject(DocView& v) {
    Document& d = *v.doc;
    Layer* l = d.ActiveLayer();
    if (!l || d.Mode() != DocMode::Layers) return;
    if (l->kind == LayerKind::Smart) { Status("Ce calque est déjà un objet dynamique"); return; }
    if (l->kind != LayerKind::Pixel && l->kind != LayerKind::Text && l->kind != LayerKind::Shape) {
        Status("Objet dynamique : choisissez un calque de pixels, de texte ou de forme");
        return;
    }
    Surface* s = d.GetSurface(l->surface);
    if (!s) return;
    v.renderer.Prepare(d, *ctx);
    // Contents: the layer pixels at the document resolution; inner document: one layer with them.
    auto content = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "SmartContent");
    ctx->CopyTexture(content.get(), s->tex.get());
    Edit e(d, v.history, "Convertir en objet dynamique");
    SmartObject* so = NewSmartObject(d, l->name, content);
    so->child = std::make_unique<Document>(d.Width(), d.Height(), d.GetPrecision(), DocMode::Layers, l->name);
    Layer& cl = so->child->AddPixelLayer(l->name);
    so->child->CancelPendingClear(cl.surface);
    ctx->CopyTexture(so->child->GetSurface(cl.surface)->tex.get(), s->tex.get());
    so->child->TouchSurface(cl.surface);
    so->child->dirty = false;
    l->kind = LayerKind::Smart;
    l->smart = so->id;
    float2 c[4] = {{0, 0}, {(float)d.Width(), 0}, {(float)d.Width(), (float)d.Height()}, {0, (float)d.Height()}};
    for (int i = 0; i < 4; ++i) l->smartCorners[i] = c[i];
    v.textHashes.erase(l->surface);
    d.SyncGraph();
    e.Commit();
    Status("Objet dynamique : transformations sans perte, filtres dynamiques, contenu modifiable (double-clic sur la vignette)");
}

void App::PlaceSmartFile(DocView& v, const std::string& path, bool linked, bool nativeSize) {
    if (path.empty()) return;
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) { Status("Les objets dynamiques utilisent le mode calques"); return; }
    u32 w = 0, h = 0;
    auto tex = LoadImageTexture(v, path, w, h);
    if (!tex) { Status("Image illisible : " + path); return; }
    Edit e(d, v.history, linked ? "Importer (lié)" : "Importer (incorporé)");
    SmartObject* so = NewSmartObject(d, Narrow(Utf8Path(path).stem().wstring()), tex);
    if (loadedSubDoc && !linked) {
        // Embedded document: "Modifier le contenu" opens its graph; saving it updates this layer.
        so->child = std::move(loadedSubDoc);
        so->child->path.clear();
    }
    if (linked) {
        so->linkedPath = path;
        so->linkedTime = FileTime(path);
    }
    float2 c[4];
    FitCorners(d, w, h, c);
    if (nativeSize) {
        // At its own size (1 pixel = 1 pixel), centered: Ctrl+T transforms it without loss.
        float x0 = (d.Width() - (float)w) * 0.5f, y0 = (d.Height() - (float)h) * 0.5f;
        c[0] = {x0, y0};
        c[1] = {x0 + w, y0};
        c[2] = {x0 + w, y0 + h};
        c[3] = {x0, y0 + h};
    }
    AddSmartLayer(v, *so, c);
    e.Commit();
    Status(std::string(linked ? "Objet dynamique lié : " : "Objet dynamique incorporé : ") + so->name + " (" + std::to_string(w) + " x " + std::to_string(h) + ")");
}

void App::ReplaceSmartContents(DocView& v, SmartObject& so, const std::string& path) {
    u32 w = 0, h = 0;
    auto tex = LoadImageTexture(v, path, w, h);
    if (!tex) { Status("Image illisible : " + path); return; }
    so.content = tex;
    so.width = w;
    so.height = h;
    so.child.reset();  // the inner document described the old contents
    if (!so.linkedPath.empty()) {
        so.linkedPath = path;
        so.linkedTime = FileTime(path);
    }
    ++so.version;
    v.doc->dirty = true;
    Status("Contenu remplacé : " + Narrow(Utf8Path(path).filename().wstring()));
}

void App::UpdateLinkedSmartObjects(DocView& v, bool force) {
    for (auto& [id, so] : v.doc->smartObjects) {
        if (so->linkedPath.empty()) continue;
        long long t = FileTime(so->linkedPath);
        if (!force && (t == 0 || t == so->linkedTime)) continue;
        u32 w = 0, h = 0;
        if (auto tex = LoadImageTexture(v, so->linkedPath, w, h)) {
            so->content = tex;
            so->width = w;
            so->height = h;
            so->linkedTime = t;
            ++so->version;
            Status("Objet lié mis à jour : " + so->name);
        }
    }
}

void App::EditSmartContents(DocView& parent, Layer& l) {
    SmartObject* so = parent.doc->GetSmart(l.smart);
    if (!so) return;
    if (!so->linkedPath.empty()) {
        // Linked: open the file itself; the layer follows when the file is saved.
        OpenImage(so->linkedPath, false);
        Status("Fichier lié ouvert : enregistrez-le (même nom) et le calque se met à jour");
        return;
    }
    for (int i = 0; i < (int)docs.size(); ++i)
        if (docs[i]->smartLink.parent == &parent && docs[i]->smartLink.smart == so->id) {
            activeDoc = focusDocRequest = i;
            return;
        }
    if (!so->child) {
        // Contents only (placed image...): an inner document made of them.
        so->child = std::make_unique<Document>(so->width, so->height, parent.doc->GetPrecision(), DocMode::Layers, so->name);
        Layer& cl = so->child->AddPixelLayer(so->name);
        so->child->CancelPendingClear(cl.surface);
        if (Surface* cs = so->child->GetSurface(cl.surface); cs && cs->tex->GetFormat() == so->content->GetFormat())
            ctx->CopyTexture(cs->tex.get(), so->content.get());
        so->child->TouchSurface(cl.surface);
    }
    auto view = std::make_unique<DocView>();
    view->doc = std::move(so->child);
    view->doc->SetName(so->name + " (objet dynamique)");
    view->doc->dirty = false;
    view->smartLink.parent = &parent;
    view->smartLink.smart = so->id;
    so->childOpen = true;
    docs.push_back(std::move(view));
    activeDoc = focusDocRequest = (int)docs.size() - 1;
    Status("Contenu de l'objet dynamique : vos modifications mettent à jour le calque");
}

void App::UpdateSmartChildren() {
    // Inner documents open in tabs: their image becomes the contents after each change.
    for (auto& vp : docs) {
        DocView& v = *vp;
        if (!v.smartLink.parent) continue;
        bool parentAlive = false;
        for (auto& p : docs) if (p.get() == v.smartLink.parent) parentAlive = true;
        if (!parentAlive) { v.smartLink = {}; continue; }
        SmartObject* so = v.smartLink.parent->doc->GetSmart(v.smartLink.smart);
        if (!so) { v.smartLink = {}; continue; }
        u64 key = ((u64)v.history.Cursor() << 32) ^ v.history.Count() ^ ((u64)v.doc->Width() << 16) ^ v.doc->Height();
        if (key == v.smartLink.syncKey) continue;
        v.renderer.Prepare(*v.doc, *ctx);
        SyncTextLayers(v);
        SyncSmartLayers(v);
        gpu::Texture* img = v.renderer.EvaluateComplete(*v.doc, *ctx);
        if (!img) continue;  // shaders still compiling: retry next frame
        v.smartLink.syncKey = key;
        if (!so->content || so->content->Width() != img->Width() || so->content->Height() != img->Height() || so->content->GetFormat() != img->GetFormat()) {
            so->content = gpu::Dev().CreateTexture(img->Width(), img->Height(), img->GetFormat(), "SmartContent");
            so->width = img->Width();
            so->height = img->Height();
        }
        ctx->CopyTexture(so->content.get(), img);
        ++so->version;
        v.smartLink.parent->doc->dirty = true;
    }
}

void App::DetachSmartChild(DocView& v) {
    // Closing the tab of an inner document: it goes back into its smart object.
    if (!v.smartLink.parent) return;
    for (auto& p : docs)
        if (p.get() == v.smartLink.parent)
            if (SmartObject* so = p->doc->GetSmart(v.smartLink.smart)) {
                so->child = std::move(v.doc);
                so->childOpen = false;
                so->child->dirty = false;
            }
    v.smartLink = {};
}

void App::SmartPropertiesUI(DocView& v, Layer& l) {
    Document& d = *v.doc;
    SmartObject* so = d.GetSmart(l.smart);
    ImGui::SeparatorText("Objet dynamique");
    if (!so) { ImGui::TextDisabled("Contenu introuvable"); return; }
    ImGui::Text("%s  -  %u x %u", so->name.c_str(), so->width, so->height);
    if (!so->linkedPath.empty()) {
        ImGui::TextDisabled("Lié : %s", so->linkedPath.c_str());
        if (!std::filesystem::exists(Utf8Path(so->linkedPath))) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "Fichier introuvable");
    } else {
        ImGui::TextDisabled(so->childOpen ? "Incorporé (contenu ouvert dans un onglet)" : "Incorporé");
    }
    int users = 0;
    for (auto& x : d.S().layers) users += x.kind == LayerKind::Smart && x.smart == so->id;
    if (users > 1) ImGui::TextDisabled("Partagé par %d calques", users);
    if (ImGui::Button("Modifier le contenu")) EditSmartContents(v, l);
    ImGui::SameLine();
    if (ImGui::Button("Remplacer le contenu...")) {
        std::string path = platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.psd;*.hdr\0");
        if (!path.empty()) ReplaceSmartContents(v, *so, path);
    }
    if (!so->linkedPath.empty()) {
        if (ImGui::Button("Mettre à jour")) UpdateLinkedSmartObjects(v, true);
        ImGui::SameLine();
        if (ImGui::Button("Incorporer")) {
            so->linkedPath.clear();
            d.dirty = true;
        }
    }
    if (ImGui::Button("Transformation (Ctrl+T)")) TransformBegin(v, false);
    ImGui::SameLine();
    if (ImGui::Button("Nouvel objet par copie")) {
        // Independent copy (its own contents), unlike Dupliquer which shares them.
        Edit e(d, v.history, "Nouvel objet dynamique par copie");
        auto content = gpu::Dev().CreateTexture(so->width, so->height, so->content->GetFormat(), "SmartContent");
        ctx->CopyTexture(content.get(), so->content.get());
        SmartObject* copy = NewSmartObject(d, so->name + " copie", content);
        copy->linkedPath = so->linkedPath;
        copy->linkedTime = so->linkedTime;
        Layer& dl = d.DuplicateLayer(l.id);
        dl.smart = copy->id;
        dl.name = copy->name;
        d.SyncGraph();
        e.Commit();
        return;
    }
    if (ImGui::Button("Pixelliser")) {
        Edit e(d, v.history, "Pixelliser l'objet dynamique");
        l.kind = LayerKind::Pixel;
        d.SyncGraph();
        e.Commit();
    }
    ImGui::TextDisabled("Les filtres du calque restent modifiables (filtres dynamiques).");
}

void App::SmartObjectsMenu(DocView* v) {
    Layer* l = v ? v->doc->ActiveLayer() : nullptr;
    bool layers = v && v->doc->Mode() == DocMode::Layers;
    if (!ImGui::BeginMenu("Objets dynamiques", layers)) return;
    if (ImGui::MenuItem("Convertir en objet dynamique", nullptr, false, l && l->kind != LayerKind::Smart)) ConvertToSmartObject(*v);
    if (ImGui::MenuItem("Modifier le contenu", nullptr, false, l && l->kind == LayerKind::Smart)) EditSmartContents(*v, *l);
    if (ImGui::MenuItem("Importer (incorporé)...")) PlaceSmartFile(*v, platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.psd;*.hdr\0"), false);
    if (ImGui::MenuItem("Importer (lié)...")) PlaceSmartFile(*v, platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.psd;*.hdr\0"), true);
    if (ImGui::MenuItem("Mettre à jour les objets liés")) UpdateLinkedSmartObjects(*v, true);
    if (ImGui::MenuItem("Pixelliser", nullptr, false, l && l->kind == LayerKind::Smart)) {
        Edit e(*v->doc, v->history, "Pixelliser l'objet dynamique");
        l->kind = LayerKind::Pixel;
        v->doc->SyncGraph();
        e.Commit();
    }
    ImGui::EndMenu();
}

}  // namespace sx
