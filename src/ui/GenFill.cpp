// Generative fill: replace the selected area with AI-generated content.
//
// The composite around the selection (plus context) is cropped, resized to a
// size the provider accepts and sent with a mask: OpenAI /images/edits takes a
// real mask (transparent = edit); Gemini gets the mask as a second image with
// instructions. The answer is resized back and placed on a new layer whose
// layer mask is the selection, so only the selected pixels change (undoable,
// non-destructive). SX_AI_MOCK=1 replaces the network call by a local
// stand-in so the whole pipeline can be tested without an API key.
#include "ui/App.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <cmath>

namespace sx {

namespace {
std::string EnvVar(const std::string& name) {
    if (name.empty()) return {};
    char* v = nullptr;
    size_t n = 0;
    _dupenv_s(&v, &n, name.c_str());
    std::string out = v ? v : "";
    free(v);
    return out;
}

// Bilinear RGBA8 resize.
std::vector<u8> Resize(const u8* src, int sw, int sh, int dw, int dh) {
    std::vector<u8> out((size_t)dw * dh * 4);
    for (int y = 0; y < dh; ++y)
        for (int x = 0; x < dw; ++x) {
            float fx = (x + 0.5f) * sw / dw - 0.5f, fy = (y + 0.5f) * sh / dh - 0.5f;
            int x0 = std::clamp((int)std::floor(fx), 0, sw - 1), y0 = std::clamp((int)std::floor(fy), 0, sh - 1);
            int x1 = std::min(x0 + 1, sw - 1), y1 = std::min(y0 + 1, sh - 1);
            float tx = std::clamp(fx - x0, 0.0f, 1.0f), ty = std::clamp(fy - y0, 0.0f, 1.0f);
            for (int c = 0; c < 4; ++c) {
                float a = src[((size_t)y0 * sw + x0) * 4 + c], b = src[((size_t)y0 * sw + x1) * 4 + c];
                float d = src[((size_t)y1 * sw + x0) * 4 + c], e = src[((size_t)y1 * sw + x1) * 4 + c];
                float v = (a + (b - a) * tx) * (1 - ty) + (d + (e - d) * tx) * ty;
                out[((size_t)y * dw + x) * 4 + c] = (u8)std::clamp((int)std::lround(v), 0, 255);
            }
        }
    return out;
}

std::string Png(const std::vector<u8>& rgba, int w, int h) {
    std::string s;
    stbi_write_png_to_func([](void* ctx, void* data, int size) { ((std::string*)ctx)->append((const char*)data, size); }, &s, w, h, 4, rgba.data(), w * 4);
    return s;
}

std::string Multipart(const std::string& boundary, const std::vector<std::pair<std::string, std::string>>& fields,
                      const std::vector<std::pair<std::string, std::string>>& pngFiles) {
    std::string b;
    for (auto& [k, v] : fields) b += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + k + "\"\r\n\r\n" + v + "\r\n";
    for (auto& [k, data] : pngFiles)
        b += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + k + "\"; filename=\"" + k + ".png\"\r\nContent-Type: image/png\r\n\r\n" + data + "\r\n";
    b += "--" + boundary + "--\r\n";
    return b;
}

// The document the running fill belongs to: a view closed and another opened at the same address
// must not receive the result (same view and document addresses, and a version counter that never went back).
struct FillOwner {
    const DocView* view = nullptr;
    const Document* doc = nullptr;
    u64 version = 0;
} fillOwner;

bool FillOwnerAlive(const std::vector<std::unique_ptr<DocView>>& docs) {
    for (auto& dv : docs)
        if (dv.get() == fillOwner.view) return dv->doc.get() == fillOwner.doc && dv->doc->ContentVersion() >= fillOwner.version;
    return false;
}
}  // namespace

void App::StartGenerativeFill(DocView& v, const std::string& prompt) {
    Document& d = *v.doc;
    Surface* sel = d.Selection();
    if (!sel) { Status("Remplissage génératif : faites d'abord une sélection"); return; }
    if (genFill.busy) return;
    genFill = {};
    genFill.view = &v;
    genFill.prompt = prompt;
    genFill.selection = d.S().selection;
    genFill.image = v.renderer.ExportRGBA8(*ctx, v.renderer.EvaluateComplete(d, *ctx));
    genFill.mask = ctx->ReadbackTexture(sel->tex.get());
    if (!genFill.image || !genFill.mask) {
        genFill = {};
        Status("Remplissage génératif : lecture de l'image impossible");
        return;
    }
    fillOwner = {&v, v.doc.get(), d.ContentVersion()};
    genFill.busy = true;
    Status("Remplissage génératif : préparation...");
}

void App::UpdateGenerativeFill() {
    if (!genFill.busy || !genFill.image || !genFill.mask || !genFill.image->ready || !genFill.mask->ready || genFill.launched) return;
    genFill.launched = true;
    const auto& img = *genFill.image;
    const auto& msk = *genFill.mask;  // R8
    const int W = (int)img.width, H = (int)img.height;
    // A failed readback comes back empty: both must cover the whole image before indexing them.
    if (W <= 0 || H <= 0 || msk.width != img.width || msk.height != img.height || img.data.size() < (size_t)W * H * 4 ||
        msk.data.size() < (size_t)W * H) {
        genFill.busy = false;
        Status("Remplissage génératif : lecture de l'image impossible");
        return;
    }
    // Selection bounds + 15% context.
    Rect r{W, H, 0, 0};
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (msk.data[(size_t)y * W + x] > 8) {
                r.x0 = std::min(r.x0, x); r.y0 = std::min(r.y0, y);
                r.x1 = std::max(r.x1, x + 1); r.y1 = std::max(r.y1, y + 1);
            }
    if (r.Empty()) { genFill.busy = false; Status("Remplissage génératif : sélection vide"); return; }
    int margin = (int)(std::max(r.Width(), r.Height()) * 0.15f) + 16;
    r = Rect{r.x0 - margin, r.y0 - margin, r.x1 + margin, r.y1 + margin}.Clipped(W, H);
    const int cw = r.Width(), ch = r.Height();
    std::vector<u8> crop((size_t)cw * ch * 4), cropMask((size_t)cw * ch * 4);
    for (int y = 0; y < ch; ++y) {
        memcpy(&crop[(size_t)y * cw * 4], &img.data[((size_t)(r.y0 + y) * W + r.x0) * 4], (size_t)cw * 4);
        for (int x = 0; x < cw; ++x) {
            u8 s = msk.data[(size_t)(r.y0 + y) * W + r.x0 + x];
            crop[((size_t)y * cw + x) * 4 + 3] = 255;  // providers expect an opaque image
            u8* m = &cropMask[((size_t)y * cw + x) * 4];
            m[0] = m[1] = m[2] = s;  // white = edit (Gemini)
            m[3] = (u8)(255 - s);    // transparent = edit (OpenAI)
        }
    }
    float aspect = (float)cw / ch;
    int tw = aspect > 1.25f ? 1536 : 1024, th = aspect < 0.8f ? 1536 : 1024;
    std::string imagePng = Png(Resize(crop.data(), cw, ch, tw, th), tw, th);
    std::string maskPng = Png(Resize(cropMask.data(), cw, ch, tw, th), tw, th);
    std::string prov = aiSettings.imageProvider;
    std::string ep = prov == "gemini" ? aiSettings.geminiEndpoint : aiSettings.openaiEndpoint;
    std::string mdl = prov == "gemini" ? aiSettings.geminiModel : aiSettings.openaiModel;
    std::string key = EnvVar(prov == "gemini" ? aiSettings.geminiKeyEnv : aiSettings.openaiKeyEnv);
    bool mock = GetEnvironmentVariableA("SX_AI_MOCK", nullptr, 0) > 0;
    std::string prompt = genFill.prompt;
    Status("Remplissage génératif : requête en cours...");
    AiAsync([this, prov, ep, mdl, key, mock, prompt, imagePng, maskPng, tw, th, r, crop, cw, ch]() -> std::function<void()> {
        std::vector<u8> png;
        std::string err;
        if (mock) {
            // Stand-in for tests: the crop with the masked area replaced by a vertical gradient.
            int iw, ih, ic;
            u8* src = stbi_load_from_memory((const u8*)imagePng.data(), (int)imagePng.size(), &iw, &ih, &ic, 4);
            u8* m = stbi_load_from_memory((const u8*)maskPng.data(), (int)maskPng.size(), &iw, &ih, &ic, 4);
            if (src && m) {
                std::vector<u8> out(src, src + (size_t)iw * ih * 4);
                for (int y = 0; y < ih; ++y)
                    for (int x = 0; x < iw; ++x) {
                        u8* p = &out[((size_t)y * iw + x) * 4];
                        float k = m[((size_t)y * iw + x) * 4] / 255.0f;
                        u8 g[3] = {(u8)(40 + 200 * y / ih), 200, (u8)(240 - 200 * y / ih)};
                        for (int c = 0; c < 3; ++c) p[c] = (u8)std::lround(p[c] * (1 - k) + g[c] * k);
                    }
                std::string s = Png(out, iw, ih);
                png.assign(s.begin(), s.end());
            }
            stbi_image_free(src);
            stbi_image_free(m);
        } else if (key.empty()) {
            err = "clé API absente (variable d'environnement)";
        } else if (prov == "openai") {
            std::string boundary = "ShaderixelBoundary" + net::RandomToken(8);
            std::string body = Multipart(boundary, {{"model", mdl}, {"prompt", prompt}, {"size", std::to_string(tw) + "x" + std::to_string(th)}, {"n", "1"}},
                                         {{"image", imagePng}, {"mask", maskPng}});
            auto resp = net::Http("POST", ep + "/images/edits", {{"Authorization", "Bearer " + key}, {"Content-Type", "multipart/form-data; boundary=" + boundary}},
                                  body, 240000);
            Json j;
            if (!resp.error.empty()) err = resp.error;
            else if (!Json::Parse(resp.body, j)) err = "réponse invalide (HTTP " + std::to_string(resp.status) + ")";
            else if (j.Has("error")) err = j["error"]["message"].S("erreur");
            else if (j["data"].Size() > 0) png = net::Base64Decode(j["data"].At(0)["b64_json"].S(""));
        } else if (prov == "gemini") {
            Json parts = Json::Array();
            Json t = Json::Object();
            t["text"] = "Edit the first image. Replace only the area that is white in the second image (a mask) with: " + prompt +
                        ". Blend it naturally with its surroundings and keep everything outside the mask unchanged. Return the full edited image with the same framing.";
            parts.Push(t);
            for (const std::string* data : {&imagePng, &maskPng}) {
                Json inl = Json::Object();
                inl["mimeType"] = "image/png";
                inl["data"] = net::Base64Encode(data->data(), data->size());
                Json part = Json::Object();
                part["inlineData"] = inl;
                parts.Push(part);
            }
            Json content = Json::Object();
            content["role"] = "user";
            content["parts"] = parts;
            Json contents = Json::Array();
            contents.Push(content);
            Json mods = Json::Array();
            mods.Push("IMAGE");
            Json gc = Json::Object();
            gc["responseModalities"] = mods;
            Json b = Json::Object();
            b["contents"] = contents;
            b["generationConfig"] = gc;
            auto resp = net::Http("POST", ep + "/models/" + mdl + ":generateContent", {{"x-goog-api-key", key}, {"Content-Type", "application/json"}}, b.Dump(0), 240000);
            Json j;
            if (!resp.error.empty()) err = resp.error;
            else if (!Json::Parse(resp.body, j)) err = "réponse invalide (HTTP " + std::to_string(resp.status) + ")";
            else if (j.Has("error")) err = j["error"]["message"].S("erreur");
            else
                for (auto& c : j["candidates"].Elements())
                    for (auto& part : c["content"]["parts"].Elements())
                        if (part["inlineData"].IsObject() && png.empty()) png = net::Base64Decode(part["inlineData"]["data"].S(""));
        } else {
            err = "fournisseur non pris en charge pour le remplissage génératif";
        }
        if (err.empty() && png.empty()) err = "aucune image dans la réponse";
        std::vector<u8> pixels;
        if (err.empty()) {
            int iw = 0, ih = 0, ic = 0;
            u8* res = stbi_load_from_memory(png.data(), (int)png.size(), &iw, &ih, &ic, 4);
            if (!res) err = "image de réponse illisible";
            else {
                pixels = Resize(res, iw, ih, cw, ch);
                for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
                stbi_image_free(res);
            }
        }
        return [this, err, pixels, r, cw, ch, prompt] {
            genFill.busy = false;
            DocView* v = genFill.view;
            const bool alive = v == fillOwner.view && FillOwnerAlive(docs);
            if (!err.empty() || !alive) { Status("Remplissage génératif : " + (err.empty() ? std::string("document fermé") : err)); return; }
            Document& d = *v->doc;
            Edit e(d, v->history, "Remplissage génératif");
            Layer& l = d.AddPixelLayer("IA : " + prompt.substr(0, 40));
            Id layerId = l.id, surface = l.surface;
            d.CancelPendingClear(surface);
            v->renderer.Import(d, *ctx, surface, pixels.data(), (u32)cw, (u32)ch, false, {r.x0, r.y0});
            // Layer mask = the selection the fill was requested with.
            Id current = d.S().selection;
            if (d.GetSurface(genFill.selection)) d.S().selection = genFill.selection;
            MaskFromSelection(*v, layerId, true);
            d.S().selection = current == genFill.selection ? 0 : current;
            d.S().editMask = false;
            d.SyncGraph();
            e.Commit();
            Status("Remplissage génératif terminé");
        };
    });
}

}  // namespace sx
