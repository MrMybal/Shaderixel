// Graph tools (NodeKind::Tool): nodes that take their input as pixels. Bake stores the input,
// Upscale stores it enlarged (bicubic on the GPU, or an upscale model through ComfyUI), ComfyUI
// sends it to a workflow and keeps the result. Once baked a tool outputs its pixels and the nodes
// before it are frozen (not evaluated, dimmed in the graph); "PassLive" lets the input through
// again to edit it, then bake again. "Convertir en image" turns the tool into a pixel node.
#include "ui/App.h"
#include "ui/Theme.h"
#include <unordered_set>
#include "ai/Comfy.h"

#include <stb_image.h>
#include <stb_image_write.h>
#include <imgui_stdlib.h>

namespace sx {

namespace {

gpu::Format ContentFormat(const Document& d) { return d.GetPrecision() == Precision::Float32 ? gpu::Format::RGBA32F : gpu::Format::RGBA16F; }

std::string EncodePng(const u8* rgba, int w, int h) {
    std::string s;
    stbi_write_png_to_func([](void* c, void* data, int size) { ((std::string*)c)->append((const char*)data, size); }, &s, w, h, 4, rgba, w * 4);
    return s;
}

// ComfyUI: input image -> upscale model -> exact size.
std::string UpscaleWorkflow(const std::string& model, u32 w, u32 h) {
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
    Json in1 = Json::Object();
    in1["image"] = "input.png";
    node("1", "LoadImage", in1);
    Json in2 = Json::Object();
    in2["model_name"] = model;
    node("2", "UpscaleModelLoader", in2);
    Json in3 = Json::Object();
    in3["upscale_model"] = link("2", 0);
    in3["image"] = link("1", 0);
    node("3", "ImageUpscaleWithModel", in3);
    Json in4 = Json::Object();
    in4["image"] = link("3", 0);
    in4["upscale_method"] = "lanczos";
    in4["width"] = (int)w;
    in4["height"] = (int)h;
    in4["crop"] = "disabled";
    node("4", "ImageScale", in4);
    Json in5 = Json::Object();
    in5["filename_prefix"] = "shaderixel_upscale";
    in5["images"] = link("4", 0);
    node("5", "SaveImage", in5);
    return j.Dump(1);
}

}  // namespace

void GraphFrozenNodes(const Graph& g, std::unordered_set<Id>& frozen) {
    // Live: reached from the output without going through a baked tool.
    std::unordered_set<Id> live, behind;
    std::vector<Id> stack = {g.output};
    while (!stack.empty()) {
        Id id = stack.back();
        stack.pop_back();
        const Node* n = g.Find(id);
        if (!n || !live.insert(id).second) continue;
        if (NodeBaked(*n)) {
            for (size_t i = 0; i < n->inputs.size(); ++i)
                if (n->inputs[i] && NodeInputType(*n, (int)i) != PinType::Exec) behind.insert(n->inputs[i]);  // the flow still runs
            continue;
        }
        for (Id in : n->inputs) if (in) stack.push_back(in);
    }
    // Frozen: feeding a baked tool, and not live through another path.
    stack.assign(behind.begin(), behind.end());
    std::unordered_set<Id> seen;
    while (!stack.empty()) {
        Id id = stack.back();
        stack.pop_back();
        const Node* n = g.Find(id);
        if (!n || !seen.insert(id).second) continue;
        if (!live.count(id)) frozen.insert(id);
        if (NodeBaked(*n)) continue;
        for (size_t i = 0; i < n->inputs.size(); ++i)
            if (n->inputs[i] && NodeInputType(*n, (int)i) != PinType::Exec) stack.push_back(n->inputs[i]);
    }
}

bool App::ViewAlive(const DocView* v) const {
    for (auto& d : docs) if (d.get() == v) return true;
    return false;
}

void App::RequestToolAction(DocView& v, Id node, ToolAction a) {
    if (a == ToolAction::Bake && toolBusy.count(node)) return;
    toolRequests.push_back({&v, node, a, 0});
}

// Bakes `n` from its input: false = not ready yet (shaders compiling), try again next frame.
bool App::ToolBakeNow(DocView& v, Id nodeId, int tries) {
    Document& d = *v.doc;
    Node* n = d.S().graph.Find(nodeId);
    if (!n || n->kind != NodeKind::Tool) return true;
    const Id inId = n->inputs.empty() ? 0 : n->inputs[0];
    if (!inId) {
        Status(n->name + " : reliez d'abord une entrée");
        return true;
    }
    gpu::Texture* in = v.renderer.EvaluateNode(d, *ctx, inId);
    const u64 inHash = v.renderer.NodeHash(inId);
    if (!in || (!inHash && tries < 240)) return false;
    const u32 W = d.Width(), H = d.Height();
    const std::string tool = n->tool;
    const std::string name = n->name;

    if (tool == "bake" || (tool == "upscale" && n->Prop("method") != "comfy")) {
        DocState before = d.S();
        const Id s = d.CreateSurfaceRaw(in->GetFormat(), "Bake");
        ctx->CopyTexture(d.GetSurface(s)->tex.get(), in);
        d.TouchSurface(s);
        n = d.S().graph.Find(nodeId);
        n->surface = s;
        n->passLive = false;
        n->smart = 0;
        n->bakedHash = inHash;
        std::string msg = name + " : entrée bakée (" + std::to_string(W) + " x " + std::to_string(H) + ")";
        if (tool == "upscale") {
            const int f = std::clamp((int)n->PropF("factor", 2), 2, 8);
            const u32 uw = std::min<u32>(W * f, 16384), uh = std::min<u32>(H * f, 16384);
            auto content = gpu::Dev().CreateTexture(uw, uh, ContentFormat(d), "Upscale");
            if (!content) { Status(name + " : image agrandie trop grande pour la carte graphique"); return true; }  // done (no retry)
            const float2 c[4] = {{0, 0}, {(float)uw, 0}, {(float)uw, (float)uh}, {0, (float)uh}};
            v.renderer.PlaceTexture(*ctx, content.get(), in, c, 2);
            SmartObject* so = NewSmartObject(d, name, content);
            n->smart = so->id;
            msg = name + " : agrandi en " + std::to_string(uw) + " x " + std::to_string(uh) + " (bicubique)";
        }
        d.dirty = true;
        v.history.PushState("Bake : " + name, before, d.S());
        Status(msg);
        return true;
    }

    // ComfyUI (workflow or upscale model): the input goes out as a PNG, the result comes back.
    ToolJob job;
    job.view = &v;
    job.node = nodeId;
    job.inHash = inHash;
    job.image = v.renderer.ExportRGBA8(*ctx, in);
    toolJobs.push_back(std::move(job));
    toolBusy.insert(nodeId);
    Status(name + " : envoi à ComfyUI...");
    return true;
}

void App::ToolLaunchComfy(ToolJob& job) {
    Document& d = *job.view->doc;
    const Node* n = d.S().graph.Find(job.node);
    if (!n) { toolBusy.erase(job.node); return; }
    const auto& img = *job.image;
    comfy::Job cj;
    cj.endpoint = aiSettings.comfyEndpoint;
    cj.checkpoint = aiSettings.comfyCheckpoint;
    cj.negative = aiSettings.comfyNegative;
    cj.steps = aiSettings.comfySteps;
    cj.width = (int)img.width;
    cj.height = (int)img.height;
    std::vector<u8> opaque(img.data.begin(), img.data.end());
    for (size_t i = 3; i < opaque.size(); i += 4) opaque[i] = 255;  // ComfyUI reads alpha as a mask
    std::string png = EncodePng(opaque.data(), (int)img.width, (int)img.height);
    cj.inputPng.assign(png.begin(), png.end());
    u32 outW = img.width, outH = img.height;
    if (n->tool == "upscale") {
        const int f = std::clamp((int)n->PropF("factor", 2), 2, 8);
        outW = std::min<u32>(img.width * f, 16384);
        outH = std::min<u32>(img.height * f, 16384);
        cj.workflow = UpscaleWorkflow(n->Prop("model", "4x-UltraSharp.pth"), outW, outH);
    } else {
        cj.prompt = n->Prop("prompt");
        cj.denoise = n->PropF("denoise", 0.5f);
        const std::string wf = n->Prop("workflow");
        if (!wf.empty() && !ReadTextFile(wf, cj.workflow)) {
            Status(n->name + " : workflow introuvable (" + wf + ")");
            toolBusy.erase(job.node);
            return;
        }
    }
    DocView* view = job.view;
    const Id nodeId = job.node;
    const u64 inHash = job.inHash;
    const std::string name = n->name;
    AiAsync([this, cj, view, nodeId, inHash, name]() -> std::function<void()> {
        std::vector<u8> out;
        std::string err;
        bool ok = comfy::Run(cj, out, err);
        return [this, ok, out = std::move(out), err, view, nodeId, inHash, name]() {
            toolBusy.erase(nodeId);
            if (!ok || out.empty()) { Status(name + " : " + (err.empty() ? std::string("ComfyUI n'a renvoyé aucune image") : err)); return; }
            if (!ViewAlive(view)) return;
            Document& d = *view->doc;
            if (!d.S().graph.Find(nodeId)) return;
            int w = 0, h = 0, comp = 0;
            stbi_uc* px = stbi_load_from_memory(out.data(), (int)out.size(), &w, &h, &comp, 4);
            if (!px) { Status(name + " : image ComfyUI illisible"); return; }
            auto content = view->renderer.ImportTexture(*ctx, px, (u32)w, (u32)h, false, ContentFormat(d));
            stbi_image_free(px);
            if (!content) return;
            DocState before = d.S();
            const Id s = d.CreateSurfaceRaw(SurfaceFormat(d.GetPrecision()), "Bake");
            const float W = (float)d.Width(), H = (float)d.Height();
            const float2 c[4] = {{0, 0}, {W, 0}, {W, H}, {0, H}};
            view->renderer.PlaceSmart(d, *ctx, s, content.get(), c, 2);
            SmartObject* so = NewSmartObject(d, name, content);
            Node* n = d.S().graph.Find(nodeId);
            n->surface = s;
            n->smart = so->id;
            n->passLive = false;
            n->bakedHash = inHash;
            d.dirty = true;
            view->history.PushState("ComfyUI : " + name, before, d.S());
            Status(name + " : résultat ComfyUI reçu (" + std::to_string(w) + " x " + std::to_string(h) + ")");
        };
    });
}

void App::ToolConvertToImage(DocView& v, Id nodeId) {
    Document& d = *v.doc;
    Node* n = d.S().graph.Find(nodeId);
    if (!n || n->kind != NodeKind::Tool) return;
    if (!n->surface) { Status(n->name + " : bakez d'abord l'entrée"); return; }
    DocState before = d.S();
    n->kind = NodeKind::Surface;
    n->inputs.clear();
    n->inputChannel.clear();
    n->tool.clear();
    n->props.clear();
    n->passLive = false;
    n->smart = 0;
    n->bakedHash = 0;
    d.dirty = true;
    v.history.PushState("Convertir en image", before, d.S());
    Status(n->name + " : converti en image (nœud de pixels, modifiable au pinceau)");
}

void App::ToolExport(DocView& v, Id nodeId) {
    Document& d = *v.doc;
    const Node* n = d.S().graph.Find(nodeId);
    if (!n) return;
    gpu::Texture* src = nullptr;
    if (SmartObject* so = n->smart ? d.GetSmart(n->smart) : nullptr) src = so->content.get();
    if (!src) if (Surface* s = d.GetSurface(n->surface)) src = s->tex.get();
    if (!src) { Status(n->name + " : rien à exporter (bakez d'abord)"); return; }
    std::string path = platform::SaveFileDialog(hwnd, L"Image PNG\0*.png\0", L"png");
    if (path.empty()) return;
    toolExports.push_back({v.renderer.ExportRGBA8(*ctx, src), path});
}

void App::UpdateGraphTools() {
    for (size_t i = 0; i < toolRequests.size();) {
        ToolRequest& r = toolRequests[i];
        bool done = true;
        if (ViewAlive(r.view)) {
            Document& d = *r.view->doc;
            Node* n = d.S().graph.Find(r.node);
            if (n && n->kind == NodeKind::Function) {
                switch (r.action) {
                    case ToolAction::Bake: done = FunctionApplyNow(*r.view, r.node, r.tries++); break;
                    case ToolAction::ToggleLive: {
                        DocState before = d.S();
                        n->applyMode = !n->applyMode;
                        d.dirty = true;
                        r.view->history.PushState(n->applyMode ? "Mode Apply" : "Mode Live", before, d.S());
                        break;
                    }
                    case ToolAction::ConvertToImage:
                        if (FunctionApplied(*n)) {
                            DocState before = d.S();
                            n->kind = NodeKind::Surface;
                            n->surface = n->outSurf[0];
                            n->outSurf.clear();
                            n->sub.reset();
                            n->inputs.clear();
                            n->inputChannel.clear();
                            d.dirty = true;
                            r.view->history.PushState("Convertir en image", before, d.S());
                        }
                        break;
                    case ToolAction::Export: break;
                }
            } else if (n && n->kind == NodeKind::Tool) {
                switch (r.action) {
                    case ToolAction::Bake: done = ToolBakeNow(*r.view, r.node, r.tries++); break;
                    case ToolAction::ToggleLive: {
                        DocState before = d.S();
                        n->passLive = !n->passLive;
                        d.dirty = true;
                        r.view->history.PushState(n->passLive ? "PassLive" : "Utiliser le bake", before, d.S());
                        break;
                    }
                    case ToolAction::ConvertToImage: ToolConvertToImage(*r.view, r.node); break;
                    case ToolAction::Export: ToolExport(*r.view, r.node); break;
                }
            }
        }
        if (done) toolRequests.erase(toolRequests.begin() + i);
        else ++i;
    }
    for (size_t i = 0; i < toolJobs.size();) {
        ToolJob& j = toolJobs[i];
        if (!ViewAlive(j.view)) { toolBusy.erase(j.node); toolJobs.erase(toolJobs.begin() + i); continue; }
        if (!j.image || !j.image->ready) { ++i; continue; }
        ToolLaunchComfy(j);
        toolJobs.erase(toolJobs.begin() + i);
    }
    for (size_t i = 0; i < toolExports.size();) {
        auto& e = toolExports[i];
        if (!e.image || !e.image->ready) { ++i; continue; }
        std::string png = EncodePng(e.image->data.data(), (int)e.image->width, (int)e.image->height);
        FILE* f = nullptr;
        if (_wfopen_s(&f, Widen(e.path).c_str(), L"wb") == 0 && f) {
            fwrite(png.data(), 1, png.size(), f);
            fclose(f);
            Status("Image exportée : " + e.path + " (" + std::to_string(e.image->width) + " x " + std::to_string(e.image->height) + ")");
        } else {
            Status("Impossible d'écrire " + e.path);
        }
        toolExports.erase(toolExports.begin() + i);
    }
}

void App::ToolNodeUI(DocView& v, Node& n) {
    Document& d = *v.doc;
    const Id id = n.id;
    static DocState snap;
    auto track = [&](const char* label) {
        if (ImGui::IsItemActivated()) snap = d.S();
        if (ImGui::IsItemDeactivatedAfterEdit()) v.history.PushState(label, snap, d.S());
    };
    auto setProp = [&](const char* k, const std::string& val) {
        n.props[k] = val;
        d.dirty = true;
    };
    const bool busy = toolBusy.count(id) > 0;
    const Id inId = n.inputs.empty() ? 0 : n.inputs[0];
    ImGui::TextDisabled("Outil du graphe - %s", n.tool.c_str());
    if (!inId) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "Aucune entrée reliée");
    if (busy) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Traitement en cours...");
    else if (!n.surface) ImGui::TextDisabled("Pas encore baké : l'entrée passe en direct");
    else if (n.passLive) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1), "PassLive : l'entrée passe en direct (le bake est gardé)");
        const u64 h = inId ? v.renderer.NodeHash(inId) : 0;
        if (h && n.bakedHash && h != n.bakedHash) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "L'entrée a changé depuis le bake");
    } else {
        ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "Baké : les nœuds avant sont figés (non calculés)");
    }
    if (SmartObject* so = n.smart ? d.GetSmart(n.smart) : nullptr) ImGui::TextDisabled("Résultat : %u x %u px", so->width, so->height);

    // Settings
    if (n.tool == "upscale") {
        ImGui::SeparatorText("Upscale");
        int f = std::clamp((int)n.PropF("factor", 2), 2, 8);
        const char* kF[] = {"x2", "x3", "x4", "x6", "x8"};
        const int kV[] = {2, 3, 4, 6, 8};
        int cur = 0;
        for (int i = 0; i < 5; ++i) if (kV[i] == f) cur = i;
        if (ImGui::Combo("Facteur", &cur, kF, 5)) {
            DocState before = d.S();
            setProp("factor", std::to_string(kV[cur]));
            v.history.PushState("Facteur d'upscale", before, d.S());
        }
        ImGui::TextDisabled("Résultat : %u x %u px", std::min<u32>(d.Width() * kV[cur], 16384), std::min<u32>(d.Height() * kV[cur], 16384));
        int m = n.Prop("method") == "comfy" ? 1 : 0;
        const char* kM[] = {"Bicubique (GPU)", "Modèle IA (ComfyUI)"};
        if (ImGui::Combo("Méthode", &m, kM, 2)) {
            DocState before = d.S();
            setProp("method", m ? "comfy" : "bicubic");
            v.history.PushState("Méthode d'upscale", before, d.S());
        }
        if (m == 1) {
            std::string model = n.Prop("model", "4x-UltraSharp.pth");
            if (ImGui::InputText("Modèle", &model)) setProp("model", model);
            track("Modèle d'upscale");
            ui::HelpTooltip("Fichier du dossier models/upscale_models de ComfyUI (4x-UltraSharp.pth, RealESRGAN_x4plus.pth...).");
        }
    } else if (n.tool == "comfyui") {
        ImGui::SeparatorText("ComfyUI");
        std::string prompt = n.Prop("prompt");
        if (ImGui::InputTextMultiline("Prompt", &prompt, ImVec2(0, 70 * ImGui::GetFontSize() / 13.0f))) setProp("prompt", prompt);
        track("Prompt");
        float dn = n.PropF("denoise", 0.5f);
        if (ImGui::SliderFloat("Denoise", &dn, 0.05f, 1.0f, "%.2f")) setProp("denoise", std::to_string(dn));
        track("Denoise");
        std::string wf = n.Prop("workflow");
        if (ImGui::InputText("Workflow", &wf)) setProp("workflow", wf);
        track("Workflow");
        ImGui::SameLine();
        if (ImGui::SmallButton("...")) {
            std::string p = platform::OpenFileDialog(hwnd, L"Workflow ComfyUI (format API)\0*.json\0");
            if (!p.empty()) {
                DocState before = d.S();
                setProp("workflow", p);
                v.history.PushState("Workflow", before, d.S());
            }
        }
        ui::HelpTooltip("Vide : image vers image intégré (modèle des réglages IA). Sinon un workflow exporté avec « Save (API format) » : "
                        "l'entrée va dans LoadImage, le prompt dans les CLIPTextEncode, la première image de sortie revient.");
        ImGui::TextDisabled("Serveur : %s", aiSettings.comfyEndpoint.c_str());
    }

    ImGui::SeparatorText("Bake");
    ImGui::BeginDisabled(busy || !inId);
    if (ImGui::Button(n.surface ? "Rebaker" : "Baker")) RequestToolAction(v, id, ToolAction::Bake);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!n.surface);
    bool live = n.passLive;
    if (ImGui::Checkbox("PassLive", &live)) RequestToolAction(v, id, ToolAction::ToggleLive);
    ui::HelpTooltip("L'entrée passe en direct pour la modifier (les nœuds avant ne sont plus figés), puis « Rebaker ».");
    if (ImGui::Button("Convertir en image")) RequestToolAction(v, id, ToolAction::ConvertToImage);
    ImGui::SameLine();
    if (ImGui::Button("Exporter l'image...")) RequestToolAction(v, id, ToolAction::Export);
    ImGui::EndDisabled();
}

}  // namespace sx
