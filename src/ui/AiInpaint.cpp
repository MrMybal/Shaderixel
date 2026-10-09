// AI inpainting: a mask of the document (painted with the brush, from the selection...) marks what
// to regenerate; the area around it is sent with the mask to ComfyUI (built-in inpainting graph or
// your API workflow), OpenAI (images/edits) or Gemini; the answer comes back on a new layer whose
// mask is the inpainting mask grown and feathered, so the edit blends in and stays non-destructive.
// Several variations can be generated and compared on the layer. "Test local" (or SX_AI_MOCK)
// replaces the network call by a stand-in to try the whole chain without a server.
#include "ui/App.h"
#include "ui/Theme.h"
#include "ai/Comfy.h"
#include "ai/Net.h"

#include <imgui_stdlib.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <cmath>

namespace sx {

namespace {

std::string EnvValue(const std::string& name) {
    if (name.empty()) return {};
    char* v = nullptr;
    size_t n = 0;
    _dupenv_s(&v, &n, name.c_str());
    std::string out = v ? v : "";
    free(v);
    return out;
}

std::vector<u8> ResizeRGBA(const u8* src, int sw, int sh, int dw, int dh) {
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
                out[((size_t)y * dw + x) * 4 + c] = (u8)std::clamp((int)std::lround((a + (b - a) * tx) * (1 - ty) + (d + (e - d) * tx) * ty), 0, 255);
            }
        }
    return out;
}

std::string ToPng(const std::vector<u8>& rgba, int w, int h) {
    std::string s;
    stbi_write_png_to_func([](void* c, void* data, int size) { ((std::string*)c)->append((const char*)data, size); }, &s, w, h, 4, rgba.data(), w * 4);
    return s;
}

// Separable max (grow) then box blur (feather) of a 0-255 mask.
void GrowFeather(std::vector<u8>& m, int W, int H, int grow, int feather) {
    std::vector<u8> t(m.size());
    auto pass = [&](bool horizontal, int r, bool maxOp) {
        if (r <= 0) return;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int acc = 0, n = 0, mx = 0;
                for (int k = -r; k <= r; ++k) {
                    const int xx = horizontal ? std::clamp(x + k, 0, W - 1) : x, yy = horizontal ? y : std::clamp(y + k, 0, H - 1);
                    const int v = m[(size_t)yy * W + xx];
                    acc += v;
                    ++n;
                    mx = std::max(mx, v);
                }
                t[(size_t)y * W + x] = (u8)(maxOp ? mx : acc / n);
            }
        m.swap(t);
    };
    pass(true, grow, true);
    pass(false, grow, true);
    for (int i = 0; i < 2; ++i) {  // two box blurs ~ a soft falloff
        pass(true, feather / 2, false);
        pass(false, feather / 2, false);
    }
}

// Built-in ComfyUI inpainting graph: checkpoint, prompts, the image with its mask in the alpha,
// full regeneration of the masked area (or a partial one with the denoise strength).
std::string InpaintWorkflow(const std::string& ckpt, float denoise, int steps) {
    Json j = Json::Object();
    auto node = [&](const char* id, const char* cls, Json in) {
        Json n = Json::Object();
        n["class_type"] = cls;
        n["inputs"] = std::move(in);
        j[id] = std::move(n);
    };
    auto link = [](const char* id, int k) {
        Json a = Json::Array();
        a.Push(id);
        a.Push(k);
        return a;
    };
    Json c = Json::Object(); c["ckpt_name"] = ckpt; node("1", "CheckpointLoaderSimple", c);
    Json p = Json::Object(); p["text"] = "{prompt}"; p["clip"] = link("1", 1); node("2", "CLIPTextEncode", p);
    Json ng = Json::Object(); ng["text"] = "{negative}"; ng["clip"] = link("1", 1); node("3", "CLIPTextEncode", ng);
    Json li = Json::Object(); li["image"] = "input.png"; node("4", "LoadImage", li);
    const bool full = denoise >= 0.99f;
    if (full) {
        Json e = Json::Object(); e["pixels"] = link("4", 0); e["vae"] = link("1", 2); e["mask"] = link("4", 1); e["grow_mask_by"] = 6;
        node("5", "VAEEncodeForInpaint", e);
    } else {
        Json e = Json::Object(); e["pixels"] = link("4", 0); e["vae"] = link("1", 2); node("5", "VAEEncode", e);
        Json sm = Json::Object(); sm["samples"] = link("5", 0); sm["mask"] = link("4", 1); node("8", "SetLatentNoiseMask", sm);
    }
    Json k = Json::Object();
    k["model"] = link("1", 0); k["seed"] = 0; k["steps"] = steps; k["cfg"] = 7.0f; k["sampler_name"] = "euler"; k["scheduler"] = "normal";
    k["positive"] = link("2", 0); k["negative"] = link("3", 0); k["latent_image"] = full ? link("5", 0) : link("8", 0); k["denoise"] = denoise;
    node("6", "KSampler", k);
    Json dec = Json::Object(); dec["samples"] = link("6", 0); dec["vae"] = link("1", 2); node("7", "VAEDecode", dec);
    Json sv = Json::Object(); sv["filename_prefix"] = "shaderixel_inpaint"; sv["images"] = link("7", 0); node("9", "SaveImage", sv);
    return j.Dump(1);
}

}  // namespace

void App::EnsureInpaintMask(DocView& v) {
    Document& d = *v.doc;
    if (d.S().inpaintMask && d.GetSurface(d.S().inpaintMask)) return;
    DocState before = d.S();
    d.S().inpaintMask = d.CreateSurface({0, 0, 0, 1});  // opaque black = nothing to regenerate
    d.S().showInpaint = true;
    v.history.PushState("Masque d'inpainting", before, d.S());
}

void App::StartInpaint(DocView& v) {
    Document& d = *v.doc;
    auto& J = inpaint;
    if (J.busy) return;
    Surface* m = d.GetSurface(d.S().inpaintMask);
    if (!m) { Status("Inpainting : peignez d'abord le masque (zone à régénérer)"); return; }
    J.view = &v;
    J.launched = false;
    J.busy = true;
    J.image = v.renderer.ExportRGBA8(*ctx, v.renderer.EvaluateComplete(d, *ctx));
    J.mask = v.renderer.ExportRGBA8(*ctx, m->tex.get(), false);
    Status("Inpainting : préparation...");
}

void App::UpdateInpaint() {
    auto& J = inpaint;
    if (!J.busy || J.launched || !J.image || !J.mask || !J.image->ready || !J.mask->ready) return;
    J.launched = true;
    if (!ViewAlive(J.view)) { J.busy = false; return; }
    const auto& img = *J.image;
    const int W = (int)img.width, H = (int)img.height;
    // Mask (red channel of the mask surface), grown and feathered for the layer mask.
    std::vector<u8> mask((size_t)W * H);
    for (size_t i = 0; i < mask.size(); ++i) mask[i] = J.mask->data[i * 4];
    Rect r{W, H, 0, 0};
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (mask[(size_t)y * W + x] > 8) {
                r.x0 = std::min(r.x0, x); r.y0 = std::min(r.y0, y);
                r.x1 = std::max(r.x1, x + 1); r.y1 = std::max(r.y1, y + 1);
            }
    if (r.Empty()) { J.busy = false; Status("Inpainting : le masque est vide (peignez la zone à régénérer)"); return; }
    std::vector<u8> soft = mask;
    GrowFeather(soft, W, H, J.grow, J.feather);
    J.maskFull = soft;
    // Context around the masked area.
    const int margin = (int)(std::max(r.Width(), r.Height()) * J.margin / 100.0f) + J.grow + J.feather + 16;
    r = Rect{r.x0 - margin, r.y0 - margin, r.x1 + margin, r.y1 + margin}.Clipped(W, H);
    J.crop = r;
    const int cw = r.Width(), ch = r.Height();
    std::vector<u8> crop((size_t)cw * ch * 4), cropMask((size_t)cw * ch * 4);
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            const u8* s = &img.data[((size_t)(r.y0 + y) * W + r.x0 + x) * 4];
            u8* o = &crop[((size_t)y * cw + x) * 4];
            o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = 255;
            const u8 mv = soft[(size_t)(r.y0 + y) * W + r.x0 + x];
            u8* mo = &cropMask[((size_t)y * cw + x) * 4];
            mo[0] = mo[1] = mo[2] = mv;  // white = regenerate
            mo[3] = (u8)(255 - mv);      // transparent = regenerate (OpenAI, ComfyUI LoadImage)
        }
    // Size the providers work at: long side 1024 (multiple of 8).
    const float k = 1024.0f / std::max(cw, ch);
    const int tw = std::max(64, ((int)std::lround(cw * k) + 7) / 8 * 8), th = std::max(64, ((int)std::lround(ch * k) + 7) / 8 * 8);
    std::vector<u8> sized = ResizeRGBA(crop.data(), cw, ch, tw, th), sizedMask = ResizeRGBA(cropMask.data(), cw, ch, tw, th);
    // ComfyUI: the image carries the mask in its alpha (LoadImage: mask = 1 - alpha).
    std::vector<u8> withAlpha = sized;
    for (size_t i = 0; i < withAlpha.size(); i += 4) withAlpha[i + 3] = sizedMask[i + 3];
    const std::string imagePng = ToPng(sized, tw, th), maskPng = ToPng(sizedMask, tw, th), comfyPng = ToPng(withAlpha, tw, th);
    const int provider = (GetEnvironmentVariableA("SX_AI_MOCK", nullptr, 0) > 0) ? 3 : J.provider;
    const int count = std::clamp(J.variations, 1, 4);
    comfy::Job cj;
    cj.endpoint = aiSettings.comfyEndpoint;
    cj.checkpoint = aiSettings.comfyCheckpoint;
    cj.prompt = J.prompt;
    cj.negative = J.negative.empty() ? aiSettings.comfyNegative : J.negative;
    cj.steps = aiSettings.comfySteps;
    cj.width = tw;
    cj.height = th;
    cj.inputPng.assign(comfyPng.begin(), comfyPng.end());
    if (!J.workflow.empty()) {
        if (!ReadTextFile(J.workflow, cj.workflow)) { J.busy = false; Status("Inpainting : workflow introuvable (" + J.workflow + ")"); return; }
    } else {
        cj.workflow = InpaintWorkflow(aiSettings.comfyCheckpoint, J.denoise, aiSettings.comfySteps);
    }
    const std::string prov = provider == 1 ? "openai" : provider == 2 ? "gemini" : "";
    const std::string ep = provider == 2 ? aiSettings.geminiEndpoint : aiSettings.openaiEndpoint;
    const std::string mdl = provider == 2 ? aiSettings.geminiModel : aiSettings.openaiModel;
    const std::string key = EnvValue(provider == 2 ? aiSettings.geminiKeyEnv : aiSettings.openaiKeyEnv);
    const std::string prompt = J.prompt;
    Status("Inpainting : génération (" + std::to_string(count) + " variation(s))...");
    AiAsync([this, provider, count, cj, prov, ep, mdl, key, prompt, imagePng, maskPng, tw, th, cw, ch]() -> std::function<void()> {
        std::vector<std::vector<u8>> results;
        std::string err;
        for (int n = 0; n < count && err.empty(); ++n) {
            std::vector<u8> png;
            if (provider == 3) {
                // Stand-in: the masked area filled with a colored gradient (a different hue per variation).
                int iw, ih, ic;
                u8* src = stbi_load_from_memory((const u8*)imagePng.data(), (int)imagePng.size(), &iw, &ih, &ic, 4);
                u8* m = stbi_load_from_memory((const u8*)maskPng.data(), (int)maskPng.size(), &iw, &ih, &ic, 4);
                if (src && m) {
                    std::vector<u8> out(src, src + (size_t)iw * ih * 4);
                    for (int y = 0; y < ih; ++y)
                        for (int x = 0; x < iw; ++x) {
                            u8* p = &out[((size_t)y * iw + x) * 4];
                            const float a = m[((size_t)y * iw + x) * 4] / 255.0f;
                            const u8 g[3] = {(u8)(60 + 60 * n + 120 * y / ih), (u8)(200 - 40 * n), (u8)(240 - 160 * x / iw)};
                            for (int c = 0; c < 3; ++c) p[c] = (u8)std::lround(p[c] * (1 - a) + g[c] * a);
                        }
                    const std::string s = ToPng(out, iw, ih);
                    png.assign(s.begin(), s.end());
                }
                stbi_image_free(src);
                stbi_image_free(m);
            } else if (provider == 0) {
                comfy::Run(cj, png, err);
            } else if (key.empty()) {
                err = "clé API absente (variable d'environnement des réglages IA)";
            } else if (prov == "openai") {
                const std::string boundary = "ShaderixelBoundary" + net::RandomToken(8);
                std::string body;
                for (auto& [kf, vf] : std::vector<std::pair<std::string, std::string>>{{"model", mdl}, {"prompt", prompt}, {"n", "1"}})
                    body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + kf + "\"\r\n\r\n" + vf + "\r\n";
                for (auto& [kf, vf] : std::vector<std::pair<std::string, const std::string*>>{{"image", &imagePng}, {"mask", &maskPng}})
                    body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + kf + "\"; filename=\"" + kf + ".png\"\r\nContent-Type: image/png\r\n\r\n" + *vf + "\r\n";
                body += "--" + boundary + "--\r\n";
                auto resp = net::Http("POST", ep + "/images/edits", {{"Authorization", "Bearer " + key}, {"Content-Type", "multipart/form-data; boundary=" + boundary}}, body, 240000);
                Json j;
                if (!resp.error.empty()) err = resp.error;
                else if (!Json::Parse(resp.body, j)) err = "réponse invalide (HTTP " + std::to_string(resp.status) + ")";
                else if (j.Has("error")) err = j["error"]["message"].S("erreur");
                else if (j["data"].Size() > 0) png = net::Base64Decode(j["data"].At(0)["b64_json"].S(""));
            } else {
                Json parts = Json::Array();
                Json t = Json::Object();
                t["text"] = "Edit the first image. Regenerate only the area that is white in the second image (a mask) with: " + prompt +
                            ". Blend it naturally with its surroundings and keep everything outside the mask unchanged. Return the full image with the same framing.";
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
            }
            if (err.empty() && png.empty()) err = "aucune image dans la réponse";
            if (!err.empty()) break;
            int iw = 0, ih = 0, ic = 0;
            u8* res = stbi_load_from_memory(png.data(), (int)png.size(), &iw, &ih, &ic, 4);
            if (!res) { err = "image de réponse illisible"; break; }
            std::vector<u8> px = ResizeRGBA(res, iw, ih, cw, ch);
            for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
            stbi_image_free(res);
            results.push_back(std::move(px));
        }
        return [this, results, err]() { FinishInpaint(results, err); };
    });
}

void App::FinishInpaint(const std::vector<std::vector<u8>>& results, const std::string& err) {
    auto& J = inpaint;
    J.busy = false;
    if (results.empty()) { Status("Inpainting : " + (err.empty() ? std::string("aucun résultat") : err)); return; }
    if (!ViewAlive(J.view)) return;
    DocView& v = *J.view;
    Document& d = *v.doc;
    const int W = (int)d.Width(), H = (int)d.Height();
    J.results = results;
    J.thumbs.clear();
    J.current = 0;
    // Layer mask: the grown and feathered inpainting mask (gray, opaque).
    std::vector<u8> maskPx((size_t)W * H * 4);
    for (size_t i = 0; i < J.maskFull.size(); ++i) {
        u8* o = &maskPx[i * 4];
        o[0] = o[1] = o[2] = J.maskFull[i];
        o[3] = 255;
    }
    const std::string label = "Inpainting : " + J.prompt.substr(0, 40);
    Edit e(d, v.history, label);
    Id surface = 0, maskSurface = 0;
    if (d.Mode() == DocMode::Layers) {
        Layer& l = d.AddPixelLayer(J.prompt.empty() ? "Inpainting" : "Inpainting : " + J.prompt.substr(0, 40));
        surface = l.surface;
        l.mask = d.CreateSurface({1, 1, 1, 1});
        l.maskEnabled = true;
        maskSurface = l.mask;
        J.layer = l.id;
        d.S().editMask = false;
        d.SyncGraph();
    } else {
        // Node mode: the result and its mask blended over the current output.
        Graph& g = d.S().graph;
        Node* out = g.Find(g.output);
        const float2 at = out ? out->pos - float2{300, -260} : float2{0, 0};
        Node& rn = d.AddNode(NodeKind::Surface, at);
        rn.name = label;
        surface = rn.surface;
        const Id rid = rn.id;
        Node& mn = d.AddNode(NodeKind::Surface, at + float2{0, 300});
        mn.name = "Masque d'inpainting";
        maskSurface = mn.surface;
        const Id mid = mn.id;
        Node& b = d.AddNode(NodeKind::Blend, at + float2{300, 0});
        b.name = "Fusion : inpainting";
        const Id bid = b.id;
        out = g.Find(g.output);
        const Id prev = out && !out->inputs.empty() ? out->inputs[0] : 0;
        if (prev) d.Link(prev, bid, 0);
        d.Link(rid, bid, 1);
        d.Link(mid, bid, 2);
        d.Link(bid, g.output, 0);
        J.layer = 0;
    }
    e.Commit();
    v.renderer.Prepare(d, *ctx);
    d.CancelPendingClear(surface);
    d.CancelPendingClear(maskSurface);
    J.surface = surface;
    v.renderer.Import(d, *ctx, maskSurface, maskPx.data(), (u32)W, (u32)H, false, {0, 0});
    ShowInpaintVariant(0);
    d.S().editInpaint = false;
    d.S().showInpaint = false;  // the result is visible as it is
    Status("Inpainting terminé : " + std::to_string(results.size()) + " variation(s) (panneau Inpainting IA pour les comparer)" +
           (err.empty() ? "" : " ; " + err));
}

void App::ShowInpaintVariant(int k) {
    auto& J = inpaint;
    if (!ViewAlive(J.view) || k < 0 || k >= (int)J.results.size()) return;
    Document& d = *J.view->doc;
    if (!d.GetSurface(J.surface)) return;
    J.current = k;
    J.view->renderer.Import(d, *ctx, J.surface, J.results[k].data(), (u32)J.crop.Width(), (u32)J.crop.Height(), false, {J.crop.x0, J.crop.y0});
    d.TouchSurface(J.surface);
    d.dirty = true;
}

void App::DrawInpaintPanel() {
    if (!ImGui::Begin("Inpainting IA", PanelFlag("Inpainting IA"))) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v) { ImGui::TextDisabled("Aucun document"); ImGui::End(); return; }
    Document& d = *v->doc;
    auto& J = inpaint;
    ImGui::PushItemWidth(-110 * ImGui::GetFontSize() / 13.0f);
    ImGui::TextWrapped("Peignez la zone à régénérer, décrivez ce qu'il faut y mettre, puis Générer : le résultat arrive sur un nouveau calque masqué.");

    ImGui::SeparatorText("Masque");
    const bool hasMask = d.GetSurface(d.S().inpaintMask) != nullptr;
    bool editing = d.S().editInpaint;
    if (ImGui::Checkbox("Peindre le masque", &editing)) {
        if (editing) {
            EnsureInpaintMask(*v);
            if (tool != Tool::Brush && tool != Tool::Eraser) tool = Tool::Brush;
            d.S().showInpaint = true;
        }
        d.S().editInpaint = editing;
    }
    ui::HelpTooltip("Le pinceau peint la zone à régénérer (en rose), la gomme la retire. Décochez pour revenir à vos calques.");
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasMask);
    ImGui::Checkbox("Afficher", &d.S().showInpaint);
    ImGui::EndDisabled();
    if (ImGui::Button("Depuis la sélection")) {
        EnsureInpaintMask(*v);
        if (d.S().selection) {
            DocState before = d.S();
            v->renderer.SelectionToMask(d, *ctx, d.S().inpaintMask, false);
            d.S().showInpaint = true;
            v->history.PushState("Masque d'inpainting depuis la sélection", before, d.S());
        } else {
            Status("Inpainting : aucune sélection");
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasMask);
    if (ImGui::Button("Inverser")) {
        EffectInstance fx;
        fx.effectId = "invert";
        if (auto def = fx.Def()) fx.params = def->DefaultParams();
        v->renderer.ApplyEffectToSurface(d, *ctx, v->history, d.S().inpaintMask, fx, "Inverser le masque d'inpainting");
    }
    ImGui::SameLine();
    if (ImGui::Button("Effacer")) {
        if (Surface* s = d.GetSurface(d.S().inpaintMask)) {
            v->renderer.FillTexture(*ctx, s->tex.get(), {0, 0, 0, 1});
            d.TouchSurface(d.S().inpaintMask);
        }
    }
    ImGui::EndDisabled();
    ImGui::SliderInt("Agrandir (px)", &J.grow, 0, 64);
    ImGui::SliderInt("Adoucir (px)", &J.feather, 0, 96);
    ui::HelpTooltip("Le masque du calque de résultat est agrandi puis adouci : la retouche se fond dans l'image.");

    ImGui::SeparatorText("Génération");
    ImGui::InputTextMultiline("Prompt", &J.prompt, ImVec2(0, 64 * ImGui::GetFontSize() / 13.0f));
    const char* kProviders[] = {"ComfyUI", "OpenAI", "Gemini", "Test local (sans réseau)"};
    ImGui::Combo("Fournisseur", &J.provider, kProviders, 4);
    if (J.provider == 0) {
        ImGui::InputText("Négatif", &J.negative);
        ImGui::SliderFloat("Denoise", &J.denoise, 0.2f, 1.0f, "%.2f");
        ui::HelpTooltip("1 : la zone est entièrement recréée. Plus bas : elle part de l'image actuelle (retouche plus légère).");
        ImGui::InputText("Workflow (API)", &J.workflow);
        ImGui::SameLine();
        if (ImGui::SmallButton("...##wf")) {
            std::string p = platform::OpenFileDialog(hwnd, L"Workflow ComfyUI (format API)\0*.json\0");
            if (!p.empty()) J.workflow = p;
        }
        ui::HelpTooltip("Vide : graphe d'inpainting intégré (modèle des réglages IA). Votre workflow : LoadImage reçoit l'image avec le masque dans l'alpha, le prompt va dans les CLIPTextEncode.");
        ImGui::TextDisabled("Serveur : %s - modèle : %s", aiSettings.comfyEndpoint.c_str(), aiSettings.comfyCheckpoint.c_str());
    } else if (J.provider < 3) {
        ImGui::TextDisabled("Clé et modèle : réglages du panneau IA.");
    }
    ImGui::SliderInt("Variations", &J.variations, 1, 4);
    ImGui::SliderInt("Contexte (%)", &J.margin, 0, 100);
    ui::HelpTooltip("Marge d'image autour du masque envoyée avec lui : plus de contexte = résultat mieux intégré.");
    ImGui::BeginDisabled(J.busy || !hasMask);
    if (ImGui::Button(J.busy ? "Génération en cours..." : "Générer", ImVec2(-FLT_MIN, 0))) StartInpaint(*v);
    ImGui::EndDisabled();

    // Variations of the last result.
    if (!J.results.empty() && ViewAlive(J.view) && J.view == v) {
        ImGui::SeparatorText("Variations");
        const float tw = 96 * ImGui::GetFontSize() / 13.0f;
        if (J.thumbs.size() != J.results.size()) {
            J.thumbs.clear();
            for (auto& px : J.results) {
                const int cw = J.crop.Width(), ch = J.crop.Height();
                const float k = std::min(1.0f, 192.0f / std::max(cw, ch));
                const int w = std::max(1, (int)(cw * k)), h = std::max(1, (int)(ch * k));
                std::vector<u8> thumbPx = ResizeRGBA(px.data(), cw, ch, w, h);
                auto t = gpu::Dev().CreateTexture((u32)w, (u32)h, gpu::Format::RGBA8, "InpaintThumb");
                ctx->Upload(t.get(), thumbPx.data(), (u32)w * 4);
                J.thumbs.push_back(t);
            }
        }
        for (int k = 0; k < (int)J.thumbs.size(); ++k) {
            if (k) ImGui::SameLine();
            const float th = tw * J.crop.Height() / std::max(1, J.crop.Width());
            ImGui::PushID(k);
            if (ImGui::ImageButton("##var", (ImTextureID)gpu::Dev().UiTextureId(J.thumbs[k].get()), ImVec2(tw, std::min(th, tw * 1.5f)), ImVec2(0, 0), ImVec2(1, 1),
                                   ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, k == J.current ? 1.0f : 0.6f)))
                ShowInpaintVariant(k);
            ImGui::PopID();
        }
        ImGui::TextDisabled("Variation %d / %zu sur le calque de résultat", J.current + 1, J.results.size());
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

}  // namespace sx
