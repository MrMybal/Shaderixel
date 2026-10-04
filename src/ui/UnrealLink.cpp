// Live link with the Unreal Engine plugin (unreal/ShaderixelLink): file-based sessions.
//
// The plugin writes a session folder (%LOCALAPPDATA%\Shaderixel\Link\sessions\<id>) with
// session.json, the mesh (OBJ with the asset's UVs) and the material's current textures, then
// starts Shaderixel with --link <session.json> or, when an instance runs (instance.json), drops
// a request into Link\inbox. Shaderixel opens a Paint 3D project (or an image document for a
// texture session) with the Unreal textures as a layer; "Envoyer vers Unreal" (or Ctrl+S)
// writes the requested outputs and result.json, which the plugin reimports.
#include "ui/App.h"

#include <ShlObj.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace sx {

namespace fs = std::filesystem;

struct LinkSession {
    std::string file, dir, id, kind, name, asset, host;
    struct Output {
        std::string role, file, asset;
    };
    std::vector<Output> outputs;
    std::string imageOut;              // texture session: the image written back
    int revision = 0;
    std::vector<std::string> pending;  // files still being written
    bool resultPending = false;
};

namespace {

std::string LinkDir() {
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) out = Narrow(p) + "\\Shaderixel\\Link";
    CoTaskMemFree(p);
    return out;
}

std::string Abs(const std::string& dir, const std::string& f) {
    if (f.empty()) return f;
    fs::path p = Utf8Path(f);
    if (p.is_relative()) p = Utf8Path(dir) / p;
    return Narrow(p.lexically_normal().wstring());
}

// Float image, 4 channels, values 0..1 as stored in the file (no color decoding).
struct LinkImage {
    int w = 0, h = 0;
    std::vector<float> px;
    float At(float u, float v, int c) const {  // bilinear, clamped
        float x = u * w - 0.5f, y = v * h - 0.5f;
        int x0 = std::clamp((int)std::floor(x), 0, w - 1), y0 = std::clamp((int)std::floor(y), 0, h - 1);
        int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
        float fx = std::clamp(x - x0, 0.0f, 1.0f), fy = std::clamp(y - y0, 0.0f, 1.0f);
        auto p = [&](int xx, int yy) { return px[((size_t)yy * w + xx) * 4 + c]; };
        return (p(x0, y0) * (1 - fx) + p(x1, y0) * fx) * (1 - fy) + (p(x0, y1) * (1 - fx) + p(x1, y1) * fx) * fy;
    }
};

bool LoadLinkImage(const std::string& path, LinkImage& img) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(path).c_str(), L"rb") != 0 || !f) return false;
    int comp = 0;
    stbi_us* data = stbi_load_from_file_16(f, &img.w, &img.h, &comp, 4);
    fclose(f);
    if (!data) return false;
    img.px.resize((size_t)img.w * img.h * 4);
    for (size_t i = 0; i < img.px.size(); ++i) img.px[i] = data[i] / 65535.0f;
    stbi_image_free(data);
    return true;
}

u16 ToHalf(float x) {
    u32 b;
    memcpy(&b, &x, 4);
    int e = (int)((b >> 23) & 255) - 127 + 15;
    return x <= 0 || e <= 0 ? 0 : e >= 31 ? 0x7BFF : (u16)((e << 10) | ((b >> 13) & 0x3FF));
}

void WriteSolidPng(const std::string& path, u8 r, u8 g, u8 b) {
    u8 px[4 * 4 * 4];
    for (int i = 0; i < 16; ++i) { px[i * 4] = r; px[i * 4 + 1] = g; px[i * 4 + 2] = b; px[i * 4 + 3] = 255; }
    stbi_write_png(path.c_str(), 4, 4, 4, px, 16);
}

}  // namespace

void App::InitLink() {
    std::error_code ec;
    fs::create_directories(Utf8Path(LinkDir() + "\\inbox"), ec);
    // This instance accepts sessions through the inbox (the plugin checks the process is alive).
    if (demoMode || testEffects) return;
    Json j = Json::Object();
    j["pid"] = (u32)GetCurrentProcessId();
    j["exe"] = ExeDir() + "\\Shaderixel.exe";
    std::ofstream(Utf8Path(LinkDir() + "\\instance.json"), std::ios::binary) << j.Dump(1);
    // Kept after exit: the plugin starts this Shaderixel when none runs.
    std::ofstream(Utf8Path(LinkDir() + "\\last_exe.txt"), std::ios::binary) << ExeDir() + "\\Shaderixel.exe";
    linkInstanceFile = true;
}

void App::ShutdownLink() {
    if (!linkInstanceFile) return;
    std::error_code ec;
    fs::remove(Utf8Path(LinkDir() + "\\instance.json"), ec);
}

bool App::OpenLinkSession(const std::string& sessionFile) {
    std::string text;
    Json j;
    if (!ReadTextFile(sessionFile, text) || !Json::Parse(text, j) || j["format"].S("") != "shaderixel-link") {
        Status("Lien Unreal : session illisible (" + sessionFile + ")");
        return false;
    }
    auto L = std::make_shared<LinkSession>();
    L->file = sessionFile;
    L->dir = Narrow(Utf8Path(sessionFile).parent_path().wstring());
    L->id = j["id"].S("");
    L->kind = j["kind"].S("mesh");
    L->name = j["name"].S("Unreal");
    L->asset = j["asset"].S("");
    L->host = j["host"].S("Unreal Engine");
    // Already open: bring it forward.
    for (int i = 0; i < (int)docs.size(); ++i)
        if (docs[i]->link && docs[i]->link->id == L->id) {
            activeDoc = i;
            focusDocRequest = i;
            Status("Lien Unreal : " + L->name + " (déjà ouvert)");
            return true;
        }
    const Json& outs = j["outputs"];
    for (size_t i = 0; i < outs.Size(); ++i)
        L->outputs.push_back({outs.At(i)["role"].S(""), Abs(L->dir, outs.At(i)["file"].S("")), outs.At(i)["asset"].S("")});
    std::error_code ec;
    fs::create_directories(Utf8Path(L->dir + "\\out"), ec);
    const std::string project = j["project"].S("");
    const bool hasProject = !project.empty() && fs::exists(Utf8Path(Abs(L->dir, project)));

    if (L->kind == "texture") {
        L->imageOut = Abs(L->dir, j["output"].S("out/image.png"));
        if (hasProject) LoadSxl(Abs(L->dir, project));
        else OpenImage(Abs(L->dir, j["image"].S("")), false);
        DocView* v = Active();
        if (!v) return false;
        v->doc->path = L->dir + "\\image.sxl";
        v->doc->SetName(L->name + " (Unreal)");
        v->link = L;
        Status("Lien Unreal : texture " + L->name + ". Ctrl+S ou « Envoyer vers Unreal » pour la renvoyer.");
        return true;
    }

    // Mesh session: the saved project if any, otherwise a new Paint 3D project with the Unreal textures.
    if (hasProject) {
        if (!LoadSxl(Abs(L->dir, project))) return false;
    } else {
        struct In { std::string role; LinkImage img; };
        std::vector<In> inputs;
        int maxW = 0;
        const Json& texs = j["textures"];
        for (size_t i = 0; i < texs.Size(); ++i) {
            In in;
            in.role = texs.At(i)["role"].S("");
            if (!LoadLinkImage(Abs(L->dir, texs.At(i)["file"].S("")), in.img)) {
                Log("Unreal link: cannot read %s", texs.At(i)["file"].S("").c_str());
                continue;
            }
            maxW = std::max({maxW, in.img.w, in.img.h});
            inputs.push_back(std::move(in));
        }
        int res = j["resolution"].I(maxW > 0 ? maxW : 2048);
        auto has = [&](const char* role) {
            for (auto& in : inputs) if (in.role == role) return true;
            for (auto& o : L->outputs) if (o.role == role) return true;
            return false;
        };
        p3dNew = {};
        p3dNew.mesh = 3;
        p3dNew.meshPath = Abs(L->dir, j["mesh"].S("mesh.obj"));
        p3dNew.resolution = res <= 1024 ? 0 : res <= 2048 ? 1 : 2;
        p3dNew.exportTemplate = 1;
        p3dNew.channels[(int)PbrChannel::Emissive] = has("Emissive");
        for (int i = 0; i < MaterialPresetCount(); ++i)
            if (std::string(MaterialPresetName(i)) == "Plastique blanc") p3dNew.basePreset = i;
        p3dNew.skipPaintLayer = true;
        NewPaint3DProject();
        p3dNew.skipPaintLayer = false;
        DocView* v = Active();
        if (!v || !v->mesh || v->doc->kind != 1) return false;
        Document& d = *v->doc;
        if (!inputs.empty()) {
            // The material's current textures, as a 3D paint layer (one layer per channel).
            Id mat = AddPaintLayer3D(*v, "Unreal (textures d'origine)");
            const int W = (int)d.Width();
            auto upload = [&](PbrChannel c, const LinkImage& img, int srcChannel /* -1: RGB */) {
                Layer* l = MaterialChannelLayer(d, mat, c);
                Surface* sf = l ? d.GetSurface(l->surface) : nullptr;
                if (!sf) return;
                v->renderer.Prepare(d, *ctx);
                std::vector<float> f((size_t)W * W * 4);
                for (int y = 0; y < W; ++y)
                    for (int x = 0; x < W; ++x) {
                        float u = (x + 0.5f) / W, vv = (y + 0.5f) / W;
                        float* o = &f[((size_t)y * W + x) * 4];
                        for (int k = 0; k < 3; ++k) o[k] = SrgbToLinear(img.At(u, vv, srcChannel < 0 ? k : srcChannel));  // visual values
                        o[3] = 1;
                    }
                if (sf->tex->GetFormat() == gpu::Format::RGBA32F) {
                    ctx->Upload(sf->tex.get(), f.data(), W * 16);
                } else {
                    std::vector<u16> h(f.size());
                    for (size_t i = 0; i < f.size(); ++i) h[i] = ToHalf(f[i]);
                    ctx->Upload(sf->tex.get(), h.data(), W * 8);
                }
                d.CancelPendingClear(l->surface);
                d.TouchSurface(l->surface);
            };
            for (auto& in : inputs) {
                if (in.role == "BaseColor") upload(PbrChannel::BaseColor, in.img, -1);
                else if (in.role == "Normal") upload(PbrChannel::Normal, in.img, -1);
                else if (in.role == "Emissive") upload(PbrChannel::Emissive, in.img, -1);
                else if (in.role == "Roughness") upload(PbrChannel::Roughness, in.img, 0);
                else if (in.role == "Metallic") upload(PbrChannel::Metallic, in.img, 0);
                else if (in.role == "AO") upload(PbrChannel::Occlusion, in.img, 0);
                else if (in.role == "Height") upload(PbrChannel::Height, in.img, 0);
                else if (in.role == "ORM") {
                    upload(PbrChannel::Occlusion, in.img, 0);
                    upload(PbrChannel::Roughness, in.img, 1);
                    upload(PbrChannel::Metallic, in.img, 2);
                }
            }
        }
        AddPaintLayer3D(*v, "Peinture 1");
        v->history.Clear();
        d.dirty = false;
    }
    DocView* v = Active();
    if (!v) return false;
    v->doc->path = L->dir + "\\project.sx3d";
    v->doc->SetName(L->name + " (Unreal)");
    v->link = L;
    Status("Lien Unreal : " + L->name + " (" + L->asset + "). Peignez, puis Ctrl+S ou « Envoyer vers Unreal ».");
    return true;
}

void App::SendToUnreal(DocView& v) {
    auto L = v.link;
    if (!L) return;
    Document& d = *v.doc;
    // The project (or image document) stays next to the session: editing again keeps every layer.
    SaveDocument(v, d.path);
    L->pending.clear();
    std::error_code ec;
    if (L->kind == "texture") {
        fs::create_directories(Utf8Path(L->imageOut).parent_path(), ec);
        gpu::Texture* img = v.renderer.EvaluateComplete(d, *ctx);
        if (auto rb = img ? v.renderer.ExportRGBA8(*ctx, img) : nullptr) {
            exports.push_back({L->imageOut, rb, {}});
            L->pending.push_back(L->imageOut);
        }
    } else {
        v.renderer.EvaluateComplete(d, *ctx);
        auto rb = [&](PbrChannel c) -> gpu::ReadbackPtr {
            gpu::Texture* t = ChannelTexture(v, c);
            return t ? v.renderer.ExportRGBA8(*ctx, t) : nullptr;
        };
        gpu::Texture* n = ChannelTexture(v, PbrChannel::Normal);
        gpu::Texture* h = ChannelTexture(v, PbrChannel::Height);
        gpu::TexturePtr baked = h ? v.renderer.BakeNormal(*ctx, n, h, d.Width(), d.Height()) : nullptr;
        for (auto& o : L->outputs) {
            fs::create_directories(Utf8Path(o.file).parent_path(), ec);
            gpu::Texture* t = nullptr;
            u8 def[3] = {0, 0, 0};
            if (o.role == "ORM") {
                PackJob job;
                job.path = o.file;
                job.ao = rb(PbrChannel::Occlusion);
                job.rough = rb(PbrChannel::Roughness);
                job.metal = rb(PbrChannel::Metallic);
                packJobs.push_back(job);
                L->pending.push_back(o.file);
                continue;
            }
            if (o.role == "BaseColor") { t = ChannelTexture(v, PbrChannel::BaseColor); def[0] = def[1] = def[2] = 188; }
            else if (o.role == "Normal") { t = baked ? baked.get() : n; def[0] = def[1] = 128; def[2] = 255; }
            else if (o.role == "Emissive") t = ChannelTexture(v, PbrChannel::Emissive);
            else if (o.role == "Roughness") { t = ChannelTexture(v, PbrChannel::Roughness); def[0] = def[1] = def[2] = 128; }
            else if (o.role == "Metallic") t = ChannelTexture(v, PbrChannel::Metallic);
            else if (o.role == "AO") { t = ChannelTexture(v, PbrChannel::Occlusion); def[0] = def[1] = def[2] = 255; }
            else if (o.role == "Height") { t = ChannelTexture(v, PbrChannel::Height); def[0] = def[1] = def[2] = 128; }
            if (auto r = t ? v.renderer.ExportRGBA8(*ctx, t) : nullptr) {
                exports.push_back({o.file, r, {}});
                L->pending.push_back(o.file);
            } else {
                WriteSolidPng(o.file, def[0], def[1], def[2]);  // channel absent from the project: its neutral value
            }
        }
        v.renderer.DropNodeCaches(d);
    }
    L->resultPending = true;
    Status("Envoi vers Unreal...");
}

void App::UpdateLink() {
    // Requests dropped by the plugin while this instance runs.
    static double next = 0;
    const double now = ImGui::GetTime();
    if (linkInstanceFile && now >= next) {
        next = now + 0.5;
        std::error_code ec;
        for (auto& e : fs::directory_iterator(Utf8Path(LinkDir() + "\\inbox"), ec)) {
            if (e.path().extension() != ".json") continue;
            std::string text, session;
            Json j;
            if (ReadTextFile(Narrow(e.path().wstring()), text) && Json::Parse(text, j)) session = j["session"].S("");
            fs::remove(e.path(), ec);
            if (!session.empty() && OpenLinkSession(session) && !Automated()) {
                // In front of Unreal (the plugin allowed it with AllowSetForegroundWindow).
                if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            }
        }
    }
    if (!pendingLinkOpen.empty() && frameCounter >= 5) {  // --link: once the app runs (the first frames set the GPU up)
        std::string s = pendingLinkOpen;
        pendingLinkOpen.clear();
        OpenLinkSession(s);
    }
    // Results: once every output file is written.
    for (auto& v : docs) {
        auto L = v->link;
        if (!L || !L->resultPending) continue;
        bool busy = false;
        for (auto& f : L->pending) {
            for (auto& e : exports) busy |= e.path == f;
            for (auto& p : packJobs) busy |= p.path == f;
        }
        if (busy) continue;
        L->resultPending = false;
        ++L->revision;
        Json r = Json::Object();
        r["format"] = "shaderixel-link-result";
        r["version"] = 1;
        r["session"] = L->id;
        r["revision"] = L->revision;
        r["time"] = (double)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        Json outs = Json::Array();
        if (L->kind == "texture") {
            Json o = Json::Object();
            o["role"] = "Image";
            o["file"] = L->imageOut;
            o["asset"] = L->asset;
            outs.Push(o);
        }
        for (auto& out : L->outputs) {
            Json o = Json::Object();
            o["role"] = out.role;
            o["file"] = out.file;
            o["asset"] = out.asset;
            outs.Push(o);
        }
        r["outputs"] = outs;
        r["project"] = Narrow(Utf8Path(v->doc->path).filename().wstring());
        // Written then renamed: the plugin never reads a half-written file.
        std::string tmp = L->dir + "\\result.tmp", dst = L->dir + "\\result.json";
        { std::ofstream(Utf8Path(tmp), std::ios::binary) << r.Dump(1); }
        std::error_code ec;
        fs::rename(Utf8Path(tmp), Utf8Path(dst), ec);
        Status(ec ? "Lien Unreal : écriture du résultat impossible" : "Envoyé vers Unreal : " + L->name + " (révision " + std::to_string(L->revision) + ")");
    }
}

}  // namespace sx
