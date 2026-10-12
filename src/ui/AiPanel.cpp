// AI panels: MCP bridge / companion control, assistant chat, image generation.
#include "ui/App.h"
#include "ai/Comfy.h"
#include "ui/Theme.h"

#include <imgui_stdlib.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace sx {

namespace {
std::string EnvSet(const std::string& name) {
    if (name.empty()) return {};
    char* v = nullptr;
    size_t n = 0;
    _dupenv_s(&v, &n, name.c_str());
    std::string out = v ? v : "";
    free(v);
    return out;
}

// One CyTools protocol call to the companion.
Json CompanionCall(const std::string& url, const std::string& token, const std::string& method, const Json& params, std::string& err) {
    static std::atomic<int> nextId{1};
    Json req = Json::Object();
    req["protocolVersion"] = "1.0";
    req["id"] = nextId++;
    req["method"] = method;
    req["params"] = params;
    auto r = net::Http("POST", url, {{"Authorization", "Bearer " + token}, {"Content-Type", "application/json"}}, req.Dump(0), 30000);
    if (!r.error.empty()) { err = r.error; return {}; }
    Json j;
    if (!Json::Parse(r.body, j)) { err = "réponse invalide (HTTP " + std::to_string(r.status) + ")"; return {}; }
    if (j.Has("error")) { err = j["error"]["code"].S("") + " : " + j["error"]["message"].S(""); return {}; }
    return j["result"];
}

const Json& JobOutputs(const Json& job) {
    if (job["outputs"].IsObject()) return job["outputs"];
    if (job["result"]["outputs"].IsObject()) return job["result"]["outputs"];
    return job["result"];
}
}  // namespace

void App::LoadAiSettings() {
    std::string text;
    Json j;
    if (ReadTextFile(settingsPath, text) && Json::Parse(text, j)) {
        aiSettings.FromJson(j["ai"]);
        penPressureGamma = std::clamp(j["tablet"]["pressureGamma"].F(1.0f), 0.2f, 5.0f);
        lensfunDir = j["lensfun"]["dir"].S("");
        if (j["tablet"]["api"].S("ink") == "wintab" && !platform::SetTabletApi(platform::TabletApi::Wintab))
            Log("Tablet: Wintab unavailable, Windows Ink used");
    }
    // CyUEMCPSDK companion: %CYUEMCPSDK_DIR%, else a CyUEMCPSDK folder next to the Shaderixel sources.
    fs::path sdk;
    if (const char* e = getenv("CYUEMCPSDK_DIR")) sdk = Utf8Path(e);
    else sdk = Utf8Path(SX_SHADER_DIR).parent_path().parent_path() / "CyUEMCPSDK";
    std::error_code ec;
    if (aiSettings.companionScript.empty() && fs::exists(sdk / "Bridge" / "run.py", ec))
        aiSettings.companionScript = Narrow((sdk / "Bridge" / "run.py").generic_wstring());
    if (aiSettings.python == "python" && fs::exists(sdk / "Runtime" / "python" / "Scripts" / "python.exe", ec))
        aiSettings.python = Narrow((sdk / "Runtime" / "python" / "Scripts" / "python.exe").generic_wstring());
    bridge.allowWrites = aiSettings.allowWrites;
}

void App::SaveAiSettings() {
    Json j = Json::Object();
    aiSettings.allowWrites = bridge.allowWrites;
    j["ai"] = aiSettings.ToJson();
    Json t = Json::Object();
    t["api"] = platform::GetTabletApi() == platform::TabletApi::Wintab ? "wintab" : "ink";
    t["pressureGamma"] = penPressureGamma;
    j["tablet"] = t;
    Json lf = Json::Object();
    lf["dir"] = lensfunDir;
    j["lensfun"] = lf;
    std::ofstream(Utf8Path(settingsPath), std::ios::binary) << j.Dump(1);
}

// Run `fn` on a worker; the result is applied on the main thread by PollAiJobs.
void App::AiAsync(std::function<std::function<void()>()> fn) {
    aiJobs.push_back(std::async(std::launch::async, std::move(fn)));
}

void App::PollAiJobs() {
    for (size_t i = 0; i < aiJobs.size();) {
        if (aiJobs[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto apply = aiJobs[i].get();
            if (apply) apply();
            aiJobs.erase(aiJobs.begin() + i);
        } else {
            ++i;
        }
    }
}

void App::DrawAiPanel() {
    if (!ImGui::Begin("Assistant IA", PanelFlag("Assistant IA"))) { ImGui::End(); return; }
    ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.35f);

    // ---------------------------------------------------------- bridge / MCP
    if (ImGui::CollapsingHeader("Pont MCP (CyToolsCore)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (bridge.Running()) ImGui::TextColored(ImVec4(0.45f, 0.9f, 0.55f, 1), "Pont actif : 127.0.0.1:%d", bridge.Port());
        else ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "Pont arrêté");
        bool comp = bridge.CompanionRunning();
        ImGui::Text("Compagnon : %s", !comp ? "arrêté" : bridge.ServiceReady() ? "prêt" : "démarrage...");
        ImGui::Text("Appels servis : %d%s%s", bridge.callsServed.load(), bridge.lastOperation.empty() ? "" : "  (dernier : ", bridge.lastOperation.empty() ? "" : (bridge.lastOperation + ")").c_str());
        bool w = bridge.allowWrites;
        if (ImGui::Checkbox("Autoriser l'IA à modifier le document", &w)) {
            bridge.allowWrites = w;
            SaveAiSettings();
        }
        ui::HelpTooltip("Désactivé : les agents peuvent seulement lire le document et le rendu.");
        if (!comp) {
            if (ImGui::Button("Démarrer le compagnon")) {
                std::string err;
                if (!bridge.LaunchCompanion(aiSettings, &err)) Status("Compagnon : " + err);
            }
        } else if (ImGui::Button("Arrêter le compagnon")) {
            bridge.StopCompanion();
            assistant.Reset();
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Démarrage auto", &aiSettings.autoStart)) SaveAiSettings();
        if (ImGui::TreeNode("Configuration")) {
            if (ImGui::InputText("Python", &aiSettings.python)) SaveAiSettings();
            if (ImGui::InputText("Script compagnon", &aiSettings.companionScript)) SaveAiSettings();
            ImGui::TextDisabled("CyUEMCPSDK/Bridge/run.py (CyToolsCore requis dans cet environnement Python)");
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Brancher un agent externe (MCP)")) {
            std::string script = aiSettings.companionScript;
            std::string conn = bridge.ConnectionFile();
            for (auto* s : {&script, &conn})
                for (auto& c : *s) if (c == '\\') c = '/';
            std::string cfg = "{\n  \"mcpServers\": {\n    \"shaderixel\": {\n      \"command\": \"" + aiSettings.python + "\",\n      \"args\": [\"" + script +
                              "\", \"--editor\", \"" + conn + "\", \"--mcp\"]\n    }\n  }\n}";
            ImGui::PushFont(ui::GetFonts().mono, 0.0f);
            ImGui::InputTextMultiline("##mcpcfg", &cfg, ImVec2(-1, 140 * dpi), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopFont();
            if (ImGui::Button("Copier")) ImGui::SetClipboardText(cfg.c_str());
            ImGui::TextDisabled("Le compagnon doit tourner. Outils : op_shaderixel.* (document_info, layer_add, render...).");
            ImGui::TreePop();
        }
    }

    // ---------------------------------------------------------- assistant chat
    if (ImGui::CollapsingHeader("Assistant", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool ready = bridge.ServiceReady();
        if (!ready) ImGui::TextDisabled("Démarrez le compagnon pour discuter avec un assistant.");
        ImGui::BeginDisabled(!ready || assistant.busy);
        if (ImGui::InputText("Connecteur", &aiSettings.assistantConnector)) SaveAiSettings();
        ui::HelpTooltip("codex, claude-code, ollama, lmstudio... (voir CyUEMCPSDK/Docs/Providers.md)");
        if (ImGui::InputText("Modèle", &aiSettings.assistantModel)) SaveAiSettings();
        if (ImGui::Button(assistant.conversationId.empty() ? "Ouvrir la conversation" : "Nouvelle conversation")) {
            std::string url = bridge.ServiceUrl(), token = bridge.ServiceToken();
            std::string connector = aiSettings.assistantConnector, model = aiSettings.assistantModel;
            std::string session = assistant.sessionId;
            assistant.busy = true;
            assistant.log.push_back({false, "Ouverture de « " + connector + " »..."});
            AiAsync([this, url, token, connector, model, session]() -> std::function<void()> {
                std::string err, sid = session;
                if (sid.empty()) {
                    Json p = Json::Object();
                    p["temporary"] = false;
                    sid = CompanionCall(url, token, "CreateSession", p, err)["sessionId"].S("");
                }
                std::string conv;
                if (err.empty()) {
                    Json params = Json::Object();
                    params["connector"] = connector;
                    if (!model.empty()) params["model"] = model;
                    Json p = Json::Object();
                    p["sessionId"] = sid;
                    p["operationId"] = "assistant_open";
                    p["parameters"] = params;
                    std::string jobId = CompanionCall(url, token, "SubmitJob", p, err)["jobId"].S("");
                    for (int i = 0; err.empty() && i < 600; ++i) {
                        Json q = Json::Object();
                        q["jobId"] = jobId;
                        Json job = CompanionCall(url, token, "GetJob", q, err);
                        std::string st = job["state"].S("");
                        if (st == "Completed") {
                            const Json& o = JobOutputs(job);
                            conv = o["conversationId"].S(o["id"].S(""));
                            break;
                        }
                        if (st == "Failed" || st == "Cancelled") { err = job["error"]["message"].S(st); break; }
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    }
                }
                return [this, sid, conv, err] {
                    assistant.busy = false;
                    assistant.sessionId = sid;
                    if (!err.empty() || conv.empty()) assistant.log.push_back({false, "Erreur : " + (err.empty() ? "conversation non créée" : err)});
                    else {
                        assistant.conversationId = conv;
                        assistant.log.push_back({false, "Conversation ouverte."});
                    }
                };
            });
        }
        ImGui::EndDisabled();

        float logH = std::max(120.0f * dpi, ImGui::GetContentRegionAvail().y - 110 * dpi);
        ImGui::BeginChild("##chat", ImVec2(0, logH), ImGuiChildFlags_Borders);
        for (auto& m : assistant.log) {
            ImGui::PushStyleColor(ImGuiCol_Text, m.user ? ImVec4(0.7f, 0.65f, 1, 1) : ImGui::GetStyleColorVec4(ImGuiCol_Text));
            ImGui::TextWrapped("%s%s", m.user ? "Vous : " : "", m.text.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
        {
            std::lock_guard lock(assistant.streamMutex);
            if (!assistant.streaming.empty()) ImGui::TextWrapped("%s", assistant.streaming.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::BeginDisabled(!ready || assistant.conversationId.empty() || assistant.busy);
        ImGui::InputTextMultiline("##msg", &assistant.input, ImVec2(-80 * dpi, 60 * dpi));
        ImGui::SameLine();
        bool send = ImGui::Button("Envoyer", ImVec2(-1, 60 * dpi)) || (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl && !assistant.input.empty());
        ImGui::EndDisabled();
        if (send && !assistant.input.empty() && ready && !assistant.busy) {
            std::string text = assistant.input;
            assistant.input.clear();
            assistant.log.push_back({true, text});
            assistant.busy = true;
            std::string url = bridge.ServiceUrl(), token = bridge.ServiceToken(), sid = assistant.sessionId, conv = assistant.conversationId;
            std::string docName = Active() ? Active()->doc->Name() : "";
            AiAsync([this, url, token, sid, conv, text, docName]() -> std::function<void()> {
                std::string err;
                Json params = Json::Object();
                params["conversationId"] = conv;
                params["text"] = text;
                Json context = Json::Object();
                context["label"] = "Shaderixel";
                context["id"] = docName;
                params["context"] = context;
                Json p = Json::Object();
                p["sessionId"] = sid;
                p["operationId"] = "assistant_send";
                p["parameters"] = params;
                std::string jobId = CompanionCall(url, token, "SubmitJob", p, err)["jobId"].S("");
                std::string answer;
                long long cursor = assistant.eventCursor;
                for (int i = 0; err.empty() && i < 3000; ++i) {
                    Json q = Json::Object();
                    q["after"] = (double)cursor;
                    Json ev = CompanionCall(url, token, "GetEvents", q, err);
                    err.clear();  // events are best effort
                    for (auto& e : ev["events"].Elements()) {
                        cursor = std::max<long long>(cursor, (long long)e["sequence"].Num(0));
                        const Json& data = e["data"];
                        std::string kind = data["kind"].S(data["type"].S(""));
                        if (kind == "TextDelta") {
                            std::lock_guard lock(assistant.streamMutex);
                            assistant.streaming += data["text"].S("");
                        }
                    }
                    if (ev.Has("cursor")) cursor = std::max<long long>(cursor, (long long)ev["cursor"].Num(0));
                    Json jq = Json::Object();
                    jq["jobId"] = jobId;
                    Json job = CompanionCall(url, token, "GetJob", jq, err);
                    std::string st = job["state"].S("");
                    if (st == "Completed") {
                        answer = JobOutputs(job)["text"].S("");
                        break;
                    }
                    if (st == "Failed" || st == "Cancelled") { err = job["error"]["message"].S(st); break; }
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                }
                return [this, answer, err, cursor] {
                    std::string streamed;
                    {
                        std::lock_guard lock(assistant.streamMutex);
                        streamed.swap(assistant.streaming);
                    }
                    assistant.eventCursor = cursor;
                    assistant.busy = false;
                    if (!err.empty()) assistant.log.push_back({false, "Erreur : " + err});
                    else assistant.log.push_back({false, answer.empty() ? streamed : answer});
                };
            });
        }
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

// ------------------------------------------------------------------ image generation

void App::StartImageGeneration() {
    if (genBusy || genPrompt.empty()) return;
    const int provider = aiSettings.imageProvider == "gemini" ? 1 : aiSettings.imageProvider == "comfyui" ? 2 : 0;
    std::string* endpoint = provider == 1 ? &aiSettings.geminiEndpoint : provider == 2 ? &aiSettings.comfyEndpoint : &aiSettings.openaiEndpoint;
    std::string* model = provider == 1 ? &aiSettings.geminiModel : &aiSettings.openaiModel;
    std::string* keyEnv = provider == 1 ? &aiSettings.geminiKeyEnv : &aiSettings.openaiKeyEnv;
    genBusy = true;
    std::string prov = aiSettings.imageProvider, ep = *endpoint, mdl = *model, key = EnvSet(*keyEnv), prompt = genPrompt;
    static const char* sizes[] = {"1024x1024", "1536x1024", "1024x1536"};
    std::string size = sizes[std::clamp(genSize, 0, 2)];
    // Optional reference image (Gemini): export the composite synchronously via readback first.
    std::string refPng;
    if ((prov == "gemini" || prov == "comfyui") && genUseImage && Active() && Active()->lastImage) refPng = "pending";
    // ComfyUI job (workflow file read now, on the main thread).
    comfy::Job cj;
    if (prov == "comfyui") {
        cj.endpoint = ep;
        cj.checkpoint = aiSettings.comfyCheckpoint;
        cj.negative = aiSettings.comfyNegative;
        cj.steps = aiSettings.comfySteps;
        cj.denoise = aiSettings.comfyDenoise;
        cj.prompt = prompt;
        cj.width = std::atoi(size.c_str());
        cj.height = std::atoi(size.c_str() + size.find('x') + 1);
        if (!aiSettings.comfyWorkflow.empty() && !ReadTextFile(aiSettings.comfyWorkflow, cj.workflow)) {
            genBusy = false;
            Status("ComfyUI : workflow introuvable (" + aiSettings.comfyWorkflow + ")");
            return;
        }
    }
    auto launch = [this, prov, ep, mdl, key, prompt, size, cj](std::string ref) {
        AiAsync([this, prov, ep, mdl, key, prompt, size, ref, cj]() -> std::function<void()> {
            std::vector<u8> png;
            std::string err;
            if (prov == "comfyui") {
                comfy::Job job = cj;
                if (!ref.empty()) job.inputPng = net::Base64Decode(ref);
                comfy::Run(job, png, err);
            } else if (key.empty()) err = "clé API absente (variable d'environnement)";
            else if (prov == "openai") {
                Json b = Json::Object();
                b["model"] = mdl;
                b["prompt"] = prompt;
                b["n"] = 1;
                b["size"] = size;
                auto r = net::Http("POST", ep + "/images/generations", {{"Authorization", "Bearer " + key}, {"Content-Type", "application/json"}}, b.Dump(0), 180000);
                Json j;
                if (!r.error.empty()) err = r.error;
                else if (!Json::Parse(r.body, j)) err = "réponse invalide (HTTP " + std::to_string(r.status) + ")";
                else if (j.Has("error")) err = j["error"]["message"].S("erreur");
                else if (j["data"].Size() > 0) png = net::Base64Decode(j["data"].At(0)["b64_json"].S(""));
            } else {
                Json parts = Json::Array();
                Json t = Json::Object();
                t["text"] = prompt;
                parts.Push(t);
                if (!ref.empty()) {
                    Json inl = Json::Object();
                    inl["mimeType"] = "image/png";
                    inl["data"] = ref;
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
                auto r = net::Http("POST", ep + "/models/" + mdl + ":generateContent", {{"x-goog-api-key", key}, {"Content-Type", "application/json"}}, b.Dump(0), 180000);
                Json j;
                if (!r.error.empty()) err = r.error;
                else if (!Json::Parse(r.body, j)) err = "réponse invalide (HTTP " + std::to_string(r.status) + ")";
                else if (j.Has("error")) err = j["error"]["message"].S("erreur");
                else
                    for (auto& c : j["candidates"].Elements())
                        for (auto& part : c["content"]["parts"].Elements())
                            if (part["inlineData"].IsObject() && png.empty()) png = net::Base64Decode(part["inlineData"]["data"].S(""));
            }
            if (err.empty() && png.empty()) err = "aucune image dans la réponse";
            return [this, png, err, prompt] {
                genBusy = false;
                if (!err.empty()) { Status("Génération : " + err); return; }
                // Save next to the settings and open/import through the regular path.
                std::string dir = Narrow(fs::temp_directory_path().wstring());
                std::string path = dir + "\\shaderixel_gen_" + std::to_string(GetTickCount64()) + ".png";
                std::ofstream(Utf8Path(path), std::ios::binary).write((const char*)png.data(), png.size());
                OpenImage(path, !genNewDoc && Active() != nullptr);
                if (DocView* v = Active())
                    if (Layer* l = v->doc->ActiveLayer()) l->name = "IA : " + prompt.substr(0, 40);
                Status("Image générée");
            };
        });
    };
    if (refPng == "pending") {
        auto rb = Active()->renderer.ExportRGBA8(*ctx, Active()->renderer.EvaluateComplete(*Active()->doc, *ctx));
        genRefReadback = rb;
        genPendingLaunch = launch;
    } else {
        launch("");
    }
}

// Launches a generation waiting for its reference image (every frame, panel shown or not).
void App::UpdateImageGeneration() {
    if (genRefReadback && genRefReadback->ready && genPendingLaunch) {
        auto rb = genRefReadback;
        std::string pngData;
        stbi_write_png_to_func([](void* ctxp, void* data, int size) { ((std::string*)ctxp)->append((const char*)data, size); }, &pngData,
                               rb->width, rb->height, 4, rb->data.data(), rb->width * 4);
        auto fn = genPendingLaunch;
        genRefReadback.reset();
        genPendingLaunch = nullptr;
        fn(net::Base64Encode(pngData.data(), pngData.size()));
    }
}

void App::DrawImageGenPanel() {
    if (!ImGui::Begin("Génération IA", PanelFlag("Génération IA"))) { ImGui::End(); return; }
    ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.35f);
    int provider = aiSettings.imageProvider == "gemini" ? 1 : aiSettings.imageProvider == "comfyui" ? 2 : 0;
    if (ImGui::Combo("Fournisseur", &provider, "OpenAI (GPT Image)\0Google Gemini\0ComfyUI (local)\0")) {
        aiSettings.imageProvider = provider == 1 ? "gemini" : provider == 2 ? "comfyui" : "openai";
        SaveAiSettings();
    }
    std::string* endpoint = provider == 1 ? &aiSettings.geminiEndpoint : provider == 2 ? &aiSettings.comfyEndpoint : &aiSettings.openaiEndpoint;
    std::string* model = provider == 1 ? &aiSettings.geminiModel : &aiSettings.openaiModel;
    std::string* keyEnv = provider == 1 ? &aiSettings.geminiKeyEnv : &aiSettings.openaiKeyEnv;
    if (ImGui::InputText("Point d'accès", endpoint)) SaveAiSettings();
    if (provider != 2) {
        if (ImGui::InputText("Modèle", model)) SaveAiSettings();
        if (ImGui::InputText("Variable de la clé", keyEnv)) SaveAiSettings();
        bool has = !EnvSet(*keyEnv).empty();
        ImGui::TextColored(has ? ImVec4(0.45f, 0.9f, 0.55f, 1) : ImVec4(1, 0.6f, 0.3f, 1), has ? "Clé trouvée dans l'environnement" : "Variable d'environnement absente");
        ui::HelpTooltip("La clé n'est jamais enregistrée par Shaderixel : elle est lue dans la variable d'environnement au moment de la requête.");
    } else {
        if (ImGui::InputText("Workflow (API)", &aiSettings.comfyWorkflow)) SaveAiSettings();
        ui::HelpTooltip("Fichier JSON exporté par ComfyUI avec « Save (API format) ». Vide : graphe intégré (checkpoint ci-dessous).\n"
                        "Le prompt remplace {prompt} (ou le texte des nœuds CLIPTextEncode), la graine est tirée au hasard,\n"
                        "la taille va aux latents vides et l'image actuelle aux nœuds LoadImage.");
        ImGui::SameLine();
        if (ImGui::SmallButton("...")) {
            std::string path = platform::OpenFileDialog(hwnd, L"Workflow ComfyUI (*.json)\0*.json\0");
            if (!path.empty()) { aiSettings.comfyWorkflow = path; SaveAiSettings(); }
        }
        if (aiSettings.comfyWorkflow.empty()) {
            if (ImGui::InputText("Checkpoint", &aiSettings.comfyCheckpoint)) SaveAiSettings();
            if (ImGui::SliderInt("Étapes", &aiSettings.comfySteps, 1, 80)) SaveAiSettings();
        }
        if (ImGui::InputText("Prompt négatif", &aiSettings.comfyNegative)) SaveAiSettings();
    }
    ImGui::TextUnformatted("Prompt");
    ImGui::InputTextMultiline("##prompt", &genPrompt, ImVec2(-1, 100 * dpi));
    ImGui::Combo("Format", &genSize, "1024 x 1024\0" "1536 x 1024 (paysage)\0" "1024 x 1536 (portrait)\0");
    if (provider == 1) ImGui::Checkbox("Envoyer l'image actuelle comme référence", &genUseImage);
    if (provider == 2) {
        ImGui::Checkbox("Partir de l'image actuelle (img2img)", &genUseImage);
        if (genUseImage && aiSettings.comfyWorkflow.empty() && ImGui::SliderFloat("Force", &aiSettings.comfyDenoise, 0.05f, 1.0f, "%.2f")) SaveAiSettings();
    }
    ImGui::Checkbox("Nouveau document", &genNewDoc);
    bool busy = genBusy;
    ImGui::BeginDisabled(busy || genPrompt.empty());
    if (ImGui::Button(busy ? "Génération en cours..." : "Générer", ImVec2(-1, 0))) StartImageGeneration();
    ImGui::EndDisabled();
    ImGui::SeparatorText("Remplissage génératif");
    {
        DocView* v = Active();
        bool canFill = v && v->doc->Selection() && !genPrompt.empty() && provider != 2 && !genFill.busy;
        ImGui::BeginDisabled(!canFill);
        if (ImGui::Button(genFill.busy ? "Remplissage en cours..." : "Remplir la sélection", ImVec2(-1, 0))) StartGenerativeFill(*v, genPrompt);
        ImGui::EndDisabled();
        ImGui::TextDisabled("Remplace la zone sélectionnée par le contenu décrit dans le prompt (nouveau calque masqué).");
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

}  // namespace sx
