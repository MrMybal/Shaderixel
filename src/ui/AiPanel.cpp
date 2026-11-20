// Native assistant and MCP controls, plus image generation.
#include "ui/App.h"
#include "ui/Lang.h"
#include "ui/Filter.h"
#include "ai/Comfy.h"
#include "ai/McpPaths.h"
#include "ShaderixelMcpVersion.h"
#include <cymcp/chat_ui.hpp>
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

}  // namespace

void App::LoadAiSettings() {
    std::string text;
    Json j;
    if (ReadTextFile(settingsPath, text) && Json::Parse(text, j)) {
        aiSettings.FromJson(j["ai"]);
        LoadTextureExportPresets(j["textureExportPresets"]);
        particlePresets.clear();
        for (const auto& entry : j["particlePresets"].Elements()) {
            auto saved = ParticleBrush::FromJson(entry);
            if (!saved.name.empty()) particlePresets.push_back(std::move(saved));
        }
        penPressureGamma = std::clamp(j["tablet"]["pressureGamma"].F(1.0f), 0.2f, 5.0f);
        lensfunDir = j["lensfun"]["dir"].S("");
        uiLanguage = j["ui"]["language"].S("en");
        uiTheme = j["ui"]["theme"].S("violet");
        uiComfortable = j["ui"]["comfortable"].B(true);
        tooltipDelay = std::clamp(j["ui"]["tooltipDelay"].F(0.15f), 0.0f, 2.0f);
        brushFavorites.clear();
        for (auto& f : j["brushes"]["favorites"].Elements()) if (!f.S("").empty()) brushFavorites.insert(f.S(""));
        updateCheckAtStartup = j["updates"]["checkAtStartup"].B(true);
        updateIncludePrereleases = j["updates"]["includePrereleases"].B(std::string(updates::Version).find('-') != std::string::npos);
        histGpuMB = j["history"]["gpuMB"].I(0);
        histGpuMaxMB = j["history"]["gpuMaxMB"].I(0);
        histRamMB = j["history"]["ramMB"].I(0);
        if (j["tablet"]["api"].S("ink") == "wintab" && !platform::SetTabletApi(platform::TabletApi::Wintab))
            Log("Tablet: Wintab unavailable, Windows Ink used");
    }

}

bool App::SaveAiSettings() {
    Json j = Json::Object();
    aiSettings.allowWrites = bridge.allowWrites;
    aiSettings.allowFiles = bridge.allowFiles;
    j["ai"] = aiSettings.ToJson();
    j["textureExportPresets"] = TextureExportPresetsJson();
    j["particlePresets"] = Json::Array();
    for (const auto& entry : particlePresets) j["particlePresets"].Push(entry.ToJson());
    Json t = Json::Object();
    t["api"] = platform::GetTabletApi() == platform::TabletApi::Wintab ? "wintab" : "ink";
    t["pressureGamma"] = penPressureGamma;
    j["tablet"] = t;
    Json lf = Json::Object();
    lf["dir"] = lensfunDir;
    j["lensfun"] = lf;
    Json ui = Json::Object();
    ui["language"] = uiLanguage;
    ui["theme"] = uiTheme;
    ui["comfortable"] = uiComfortable;
    ui["tooltipDelay"] = tooltipDelay;
    {
        Json fav = Json::Array();
        for (auto& f : brushFavorites) fav.Push(f);
        j["brushes"]["favorites"] = fav;
    }
    j["ui"] = ui;
    j["updates"]["checkAtStartup"] = updateCheckAtStartup;
    j["updates"]["includePrereleases"] = updateIncludePrereleases;
    Json hist = Json::Object();  // 0: automatic (from the graphics card and the system memory)
    hist["gpuMB"] = histGpuMB;
    hist["gpuMaxMB"] = histGpuMaxMB;
    hist["ramMB"] = histRamMB;
    j["history"] = hist;
    // Written to a temporary file then moved over the settings: a failed write (full disk, crash) keeps the previous ones.
    const fs::path dst = Utf8Path(settingsPath), tmp = Utf8Path(settingsPath + ".tmp");
    bool ok = false;
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        f << j.Dump(1);
        f.flush();
        ok = f.good();
    }
    std::error_code ec;
    if (ok) fs::rename(tmp, dst, ec);
    if (!ok || ec) {
        fs::remove(tmp, ec);
        Log("Settings: cannot write %s", settingsPath.c_str());
        return false;
    }
    return true;
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

struct AssistantUi {
    std::shared_ptr<cymcp::ProviderRegistry> providers = std::make_shared<cymcp::ProviderRegistry>();
    cymcp::ChatController controller{providers};
    cymcp::Cancellation cancelDiscovery;
    std::future<std::vector<cymcp::DiscoveredConnection>> discover;
    std::vector<cymcp::DiscoveredConnection> connections;
    std::vector<fs::path> attachments;
    std::string draft, error;
    uint64_t shownRevision = 0;
    enum class Probe { Open, Send, Tool, Cancel, Restore, Finished } probe = Probe::Open;
    bool observedDelta = false;
    cymcp::Clock::time_point probeDeadline = cymcp::Clock::now() + std::chrono::minutes(2);

    ~AssistantUi() { cancelDiscovery.cancel(); controller.cancel(); }

    void Discover() {
        if (discover.valid()) return;
        cancelDiscovery = {};
        auto registry = providers;
        cymcp::CallContext context;
        context.cancellation = cancelDiscovery;
        discover = std::async(std::launch::async, [registry, context] { return registry->discover({}, context); });
    }

    void Poll() {
        controller.poll();
        if (!discover.valid() || discover.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try { connections = discover.get(); }
        catch (const std::exception& e) { error = cymcp::redact_error_message(e.what()); }
    }

    void Open(AiBridge& bridge, const AiSettings& settings, bool restore) {
        cymcp::ChatConnection connection;
        connection.provider_id = settings.assistantConnector;
        connection.model = settings.assistantModel;
        connection.system_prompt =
            "You are the assistant in Shaderixel. Inspect the current document and use its operations for requested editing. "
            "Respect read-only mode, errors, and confirmations. Ask before overwriting an existing file. "
            "GPU and document changes affect the user's open editor. Use the resource tools to retrieve render results.";
        const auto found = std::find_if(connections.begin(), connections.end(), [&](const auto& candidate) {
            return candidate.provider_id == connection.provider_id && candidate.available();
        });
        if (found != connections.end()) connection.provider = found->config;
        if (!settings.assistantEndpoint.empty()) connection.provider.settings["base_url"] = settings.assistantEndpoint;
        if (!settings.assistantKeyEnv.empty()) connection.provider.api_key = EnvSet(settings.assistantKeyEnv);
        else if (connection.provider.api_key.empty()) {
            if (connection.provider_id == "openai") connection.provider.api_key = EnvSet("OPENAI_API_KEY");
            if (connection.provider_id == "anthropic") connection.provider.api_key = EnvSet("ANTHROPIC_API_KEY");
            if (connection.provider_id == "openrouter") connection.provider.api_key = EnvSet("OPENROUTER_API_KEY");
        }
        if (connection.provider_id == "claude-code") {
            std::string failure;
            if (!bridge.StartMcp(failure)) throw cymcp::Error("NetworkError", failure);
            connection.provider.settings["mcp_servers"] =
                cymcp::parse_json(bridge.McpConfiguration()).at("mcpServers");
        } else {
            connection.provider.tools = bridge.ChatTools(settings.compactTools);
        }
        const auto endpoint = connection.provider.settings.value("base_url", std::string());
        const auto state = mcplocal::StateDirectory();
        if (settings.keepChatHistory) {
            connection.history_directory = state / "conversations";
            connection.history_id = "assistant-" + cymcp::sha256(connection.provider_id + "\n" + endpoint).substr(0, 24);
            connection.restore = restore && fs::is_regular_file(connection.history_directory / (connection.history_id + ".json"));
        }
        connection.context.max_bytes = static_cast<size_t>(settings.contextKiB) * 1024;
        connection.performance = bridge.Performance();
        connection.automatic_model = connection.model.empty();
        controller.open(std::move(connection));
    }

    void DrawChat(HWND owner, float dpi) {
        const auto& view = controller.state();
        if (!view.error_message.empty()) ImGui::TextWrapped("%s", view.error_message.c_str());
        if (!view.storage_error.empty()) ImGui::TextWrapped("Historique : %s", view.storage_error.c_str());
        const float transcriptHeight = std::max(100.0f * dpi, ImGui::GetContentRegionAvail().y - 120.0f * dpi);
        if (ImGui::BeginChild("AssistantMessages", ImVec2(0, transcriptHeight), ImGuiChildFlags_Borders)) {
            const bool nearBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - ImGui::GetTextLineHeightWithSpacing();
            for (const auto& message : view.messages) {
                ImGui::TextDisabled("%s", message.role == "user" ? "Vous" : "Assistant");
                ImGui::TextWrapped("%s", message.markdown.c_str());
                for (const auto& file : message.attachments) ImGui::TextDisabled("%s", file.name.c_str());
                if (message.interrupted) ImGui::TextDisabled("Annulé");
                if (!message.error_message.empty()) ImGui::TextWrapped("%s", message.error_message.c_str());
                ImGui::Spacing();
            }
            if (view.busy && !view.activity.empty()) ImGui::TextDisabled("%s", view.activity.back().name.c_str());
            if (nearBottom && shownRevision != view.revision) ImGui::SetScrollHereY(1.0f);
            shownRevision = view.revision;
        }
        ImGui::EndChild();
        ImGui::BeginDisabled(!view.connected || view.busy);
        const auto inputSize = ImVec2(-1, 59 * dpi);
        ImGui::InputTextMultiline("##assistantDraft", &draft, inputSize);
        const bool shortcut = view.connected && !view.busy && ImGui::IsItemFocused() &&
                              ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter);
        if (ImGui::Button("Joindre un fichier")) {
            const auto path = platform::OpenFileDialog(owner, L"Fichiers\0*.*\0");
            if (!path.empty()) attachments.push_back(Utf8Path(path));
        }
        ImGui::SameLine();
        if ((ImGui::Button("Envoyer") || shortcut) && (!draft.empty() || !attachments.empty())) {
            try {
                error.clear();
                controller.send(draft, attachments);
                draft.clear();
                attachments.clear();
            } catch (const std::exception& e) { error = cymcp::redact_error_message(e.what()); }
        }
        for (size_t i = 0; i < attachments.size();) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton("Retirer")) attachments.erase(attachments.begin() + i);
            else {
                ImGui::SameLine();
                ImGui::TextUnformatted(cymcp::path_utf8(attachments[i].filename()).c_str());
                ++i;
            }
            ImGui::PopID();
        }
        ImGui::EndDisabled();
        if (view.busy && ImGui::Button("Annuler la requête")) controller.cancel();
    }

    // Isolated integration fixture, enabled together with --ai-test by the local test driver.
    void CheckIntegration(AiBridge& bridge, const AiSettings& settings, const fs::path& report) {
        if (probe == Probe::Finished) return;
        const auto& view = controller.state();
        const auto has = [&](std::string_view text) {
            return std::any_of(view.messages.begin(), view.messages.end(), [&](const auto& message) {
                return message.role == "assistant" && message.markdown.find(text) != std::string::npos;
            });
        };
        const auto reportResult = [&](const std::string& failure) {
            probe = Probe::Finished;
            std::ofstream file(report, std::ios::binary);
            file << cymcp::Json{{"ok", failure.empty()}, {"streamed", observedDelta}, {"error", failure}}.dump();
        };
        try {
            if (cymcp::Clock::now() > probeDeadline) throw std::runtime_error("Chat fixture timed out");
            switch (probe) {
            case Probe::Open:
                Open(bridge, settings, false);
                probe = Probe::Send;
                break;
            case Probe::Send:
                if (view.busy) break;
                if (!view.connected) throw std::runtime_error("Connect: " + view.error_code + " " + view.error_message);
                controller.send("fixture-tool-turn");
                probe = Probe::Tool;
                break;
            case Probe::Tool:
                if (view.busy) {
                    if (has("Contrôle") && !has("Contrôle réussi")) observedDelta = true;
                } else {
                    if (!observedDelta || !has("Contrôle réussi"))
                        throw std::runtime_error("Missing streamed or final response: " + view.error_code + " " + view.error_message);
                    controller.send("fixture-cancel-turn");
                    probe = Probe::Cancel;
                }
                break;
            case Probe::Cancel:
                if (view.busy) {
                    if (has("Interrompre")) controller.cancel();
                } else {
                    if (view.error_code != "Cancelled" || !has("Interrompre"))
                        throw std::runtime_error("Missing cancellation or partial text: " + view.error_code);
                    Open(bridge, settings, true);
                    probe = Probe::Restore;
                }
                break;
            case Probe::Restore:
                if (view.busy) break;
                if (!view.connected || !has("Contrôle réussi"))
                    throw std::runtime_error("Could not restore the completed conversation: " + view.error_code + " " + view.error_message);
                reportResult({});
                break;
            case Probe::Finished: break;
            }
        } catch (const std::exception& e) { reportResult(cymcp::redact_error_message(e.what())); }
    }
};

void App::CloseAssistant() { assistant.reset(); }

void App::DrawAiPanel() {
    if (!ImGui::Begin("Assistant IA", PanelFlag("Assistant IA"))) { ImGui::End(); return; }
    if (!assistant) assistant = std::make_shared<AssistantUi>();
    auto& chat = *assistant;
    chat.Poll();
    if (aiTest && Active()) {
        const auto report = EnvSet("SX_MCP_CHAT_RESULT");
        if (!report.empty()) chat.CheckIntegration(bridge, aiSettings, Utf8Path(report));
    }
    if (ImGui::CollapsingHeader("Serveur MCP")) {
        ImGui::TextDisabled("CyMCPSDK %s (%.*s)", cymcp::version, 12, SX_CYMCP_REVISION);
        const auto profiles = McpAccessOptions();
        const auto current = std::ranges::find(profiles, aiSettings.mcpProfile, &McpAccessOption::profile);
        if (ImGui::BeginCombo("Profil MCP", current->label)) {
            for (const auto& option : profiles) {
                if (!ImGui::Selectable(option.label, option.profile == aiSettings.mcpProfile) ||
                    option.profile == aiSettings.mcpProfile) continue;
                const auto& state = chat.controller.state();
                if (state.provider_id == "claude-code" && (state.connected || state.busy))
                    chat.controller.disconnect();
                aiSettings.mcpProfile = option.profile;
                chat.error.clear();
                bridge.SetMcpProfile(option.profile, chat.error);
                SaveAiSettings();
            }
            ImGui::EndCombo();
        }
        const auto url = bridge.McpUrl();
        if (url.empty()) {
            if (ImGui::Button("Démarrer le serveur MCP")) {
                chat.error.clear();
                bridge.StartMcp(chat.error);
            }
        } else {
            ImGui::TextUnformatted(url.c_str());
            if (ImGui::Button("Arrêter le serveur MCP")) bridge.StopMcp();
            ImGui::SameLine();
            if (ImGui::Button("Copier la configuration MCP")) ImGui::SetClipboardText(bridge.McpConfiguration().c_str());
        }
        if (bridge.ControlsOperations()) {
            bool writes = bridge.allowWrites;
            if (ImGui::Checkbox("Autoriser l'IA à modifier le document", &writes)) { bridge.allowWrites = writes; SaveAiSettings(); }
            bool files = bridge.allowFiles;
            if (ImGui::Checkbox("Autoriser l'accès aux fichiers", &files)) { bridge.allowFiles = files; SaveAiSettings(); }
        }
        if (ImGui::Checkbox("Démarrage auto du serveur", &aiSettings.autoStart)) SaveAiSettings();
        if (ImGui::TreeNode("Optimisations MCP")) {
            if (ImGui::Checkbox("Découverte à la demande dans le chat", &aiSettings.compactTools)) SaveAiSettings();
            if (ImGui::Checkbox("Stockage sur disque des gros résultats", &aiSettings.spoolResults)) SaveAiSettings();
            if (ImGui::Checkbox("Cache de découverte", &aiSettings.cacheDiscovery)) SaveAiSettings();
            if (ImGui::Checkbox("Notifications du catalogue et des ressources", &aiSettings.notifyChanges)) SaveAiSettings();
            if (ImGui::Checkbox("Mesures de performance privées", &aiSettings.measureCalls)) SaveAiSettings();
            ImGui::TextDisabled("Les options du serveur s’appliquent après redémarrage.");
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("Fournisseur de l’assistant", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(chat.controller.state().busy);
        const std::pair<const char*, const char*> profiles[] = {
            {"codex", "Codex"}, {"claude-code", "Claude Code"}, {"openai", "OpenAI"},
            {"anthropic", "Anthropic"}, {"openrouter", "OpenRouter"}, {"ollama", "Ollama"},
            {"lmstudio", "LM Studio"}, {"llama.cpp", "llama.cpp"}, {"openai-compatible", "API compatible OpenAI"}
        };
        if (ImGui::BeginCombo("Connecteur", aiSettings.assistantConnector.c_str())) {
            for (const auto& [id, name] : profiles) {
                if (ImGui::Selectable(name, aiSettings.assistantConnector == id)) {
                    aiSettings.assistantConnector = id;
                    aiSettings.assistantModel.clear();
                    aiSettings.assistantEndpoint.clear();
                    aiSettings.assistantKeyEnv.clear();
                    SaveAiSettings();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::InputText("Modèle", &aiSettings.assistantModel)) SaveAiSettings();
        const auto& models = chat.controller.state().models;
        if (models.is_array() && !models.empty() && ImGui::BeginCombo("Modèles détectés", aiSettings.assistantModel.c_str())) {
            for (const auto& model : models) {
                if (!model.is_object()) continue;
                const auto id = model.value("id", std::string());
                if (!id.empty() && ImGui::Selectable(id.c_str(), aiSettings.assistantModel == id)) {
                    aiSettings.assistantModel = id;
                    SaveAiSettings();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::TreeNode("Connexion et historique")) {
            if (ImGui::InputText("Adresse du fournisseur", &aiSettings.assistantEndpoint)) SaveAiSettings();
            if (ImGui::InputText("Variable d’environnement de la clé", &aiSettings.assistantKeyEnv)) SaveAiSettings();
            if (ImGui::Checkbox("Conserver l’historique local", &aiSettings.keepChatHistory)) SaveAiSettings();
            if (ImGui::InputInt("Fenêtre de contexte (Kio, 0 = complète)", &aiSettings.contextKiB)) {
                aiSettings.contextKiB = std::clamp(aiSettings.contextKiB, 0, 1024);
                SaveAiSettings();
            }
            ImGui::TreePop();
        }
        if (ImGui::Button("Détecter les fournisseurs")) chat.Discover();
        if (chat.discover.valid()) { ImGui::SameLine(); ImGui::TextDisabled("Détection…"); }
        for (const auto& candidate : chat.connections) {
            if (candidate.provider_id != aiSettings.assistantConnector) continue;
            ImGui::SameLine();
            ImGui::TextDisabled("%s", candidate.state.c_str());
            break;
        }
        auto open = [&](bool restore) {
            try { chat.error.clear(); chat.Open(bridge, aiSettings, restore); }
            catch (const std::exception& e) { chat.error = cymcp::redact_error_message(e.what()); }
        };
        if (ImGui::Button("Connecter ou reprendre")) open(true);
        ImGui::SameLine();
        if (ImGui::Button("Nouvelle conversation")) open(false);
        ImGui::EndDisabled();
    }
    if (!chat.error.empty()) ImGui::TextWrapped("%s", chat.error.c_str());
    chat.DrawChat(hwnd, dpi);
    ImGui::End();
}

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
                const bool asLayer = !genNewDoc && Active() != nullptr;
                DocView* before = Active();
                const size_t docsBefore = docs.size(), layersBefore = before ? before->doc->S().layers.size() : 0;
                OpenImage(path, asLayer);
                // Renamed only when the image really arrived (new document, or a new layer of the same document).
                DocView* v = Active();
                const bool opened = asLayer ? v && v == before && v->doc->S().layers.size() > layersBefore
                                            : v && docs.size() > docsBefore && v == docs.back().get();
                if (!opened) { Status("Génération : ouverture de l'image générée impossible (" + path + ")"); return; }
                if (Layer* l = v->doc->ActiveLayer()) l->name = "IA : " + prompt.substr(0, 40);
                Status("Image générée");
            };
        });
    };
    if (refPng == "pending") {
        auto rb = Active() ? Active()->renderer.ExportRGBA8(*ctx, Active()->renderer.EvaluateComplete(*Active()->doc, *ctx)) : nullptr;
        if (!rb) {
            genBusy = false;
            Status("Génération : lecture de l'image de référence impossible");
            return;
        }
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
        if (rb->width == 0 || rb->height == 0 || rb->data.size() < (size_t)rb->width * rb->height * 4) {
            // Failed readback: nothing to send as reference.
            genRefReadback.reset();
            genPendingLaunch = nullptr;
            genBusy = false;
            Status("Génération : lecture de l'image de référence impossible");
            return;
        }
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
    ui::FilterBox();
    ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.35f);
    if (ImGui::TreeNode("Scripts d’image")) {
        if (ImGui::InputText("Interpréteur Python", &aiSettings.python)) SaveAiSettings();
        ImGui::TreePop();
    }
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
