#include "ui/App.h"
#include "ui/AiSchema.h"
#include "ui/Search.h"
#include "ui/Lang.h"

#include <imgui_internal.h>
#include <stb_image_write.h>
#include <filesystem>
#include <fstream>

namespace sx {
namespace {

ImGuiKey InputKey(const std::string& name) {
    static const std::pair<const char*, ImGuiKey> keys[] = {
        {"enter", ImGuiKey_Enter}, {"escape", ImGuiKey_Escape}, {"tab", ImGuiKey_Tab},
        {"space", ImGuiKey_Space}, {"backspace", ImGuiKey_Backspace}, {"delete", ImGuiKey_Delete},
        {"left", ImGuiKey_LeftArrow}, {"right", ImGuiKey_RightArrow}, {"up", ImGuiKey_UpArrow},
        {"down", ImGuiKey_DownArrow}, {"home", ImGuiKey_Home}, {"end", ImGuiKey_End},
        {"page_up", ImGuiKey_PageUp}, {"page_down", ImGuiKey_PageDown}
    };
    for (const auto& [label, key] : keys) if (name == label) return key;
    if (name.size() != 1) return ImGuiKey_None;
    if (name[0] >= 'a' && name[0] <= 'z') return static_cast<ImGuiKey>(ImGuiKey_A + name[0] - 'a');
    if (name[0] >= '0' && name[0] <= '9') return static_cast<ImGuiKey>(ImGuiKey_0 + name[0] - '0');
    return ImGuiKey_None;
}

Json Bounds(float left, float top, float right, float bottom, ImVec2 origin) {
    Json bounds = Json::Array();
    for (float x : {left - origin.x, top - origin.y, right - origin.x, bottom - origin.y}) bounds.Push(x);
    return bounds;
}

const char* SearchKind(int kind) {
    return kind == 0 ? "panel" : kind == 1 ? "menu" : "control";
}

Json DescribeSearchEntry(const SearchEntry& item) {
    Json j = Json::Object();
    j["label"] = item.label;
    j["translated"] = Tr(item.label);
    j["kind"] = SearchKind(item.kind);
    j["path"] = Json::Array();
    for (const auto& entry : item.path) j["path"].Push(entry);
    if (!item.window.empty()) j["window"] = item.window;
    return j;
}

bool ActionWithoutFiles(const std::string& command) {
    if (command == "export" || command == "op") return false;
    for (int i = 0; i < ActionCmdCount(); ++i) if (command == ActionCmdId(i)) return true;
    return false;
}

Json ActionCommands() {
    Json result = Json::Array();
    for (int i = 0; i < ActionCmdCount(); ++i) {
        const std::string id = ActionCmdId(i);
        Json item = Json::Object();
        item["command"] = id;
        item["label"] = ActionCmdLabel(id);
        item["defaults"] = ActionDefaultParams(id);
        item["runnable"] = ActionWithoutFiles(id);
        result.Push(std::move(item));
    }
    return result;
}

}  // namespace

void App::PumpAiInterface() {
    if (!aiUiSequence) return;
    auto& io = ImGui::GetIO();
    if (!bridge.allowWrites || !bridge.allowFiles) {
        std::unordered_map<ImGuiKey, bool> keys;
        bool buttons[ImGuiMouseButton_COUNT]{};
        for (size_t i = 0; i < aiUiSequence->next; ++i) {
            const auto& event = aiUiSequence->events[i];
            if (event.key != ImGuiKey_None) keys[event.key] = event.down;
            if (event.button >= 0) buttons[event.button] = event.down;
        }
        for (const auto& [key, down] : keys) if (down) io.AddKeyEvent(key, false);
        for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
            if (buttons[button]) io.AddMouseButtonEvent(button, false);
        aiUiSequence.reset();
        return;
    }
    if (aiUiSequence->next >= aiUiSequence->events.size()) {
        aiUiSequence.reset();
        return;
    }
    const auto& event = aiUiSequence->events[aiUiSequence->next++];
    if (event.move) {
        const auto* viewport = ImGui::GetMainViewport();
        io.AddMousePosEvent(viewport->Pos.x + event.point.x, viewport->Pos.y + event.point.y);
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) io.AddMouseViewportEvent(viewport->ID);
    }
    if (event.button >= 0) io.AddMouseButtonEvent(event.button, event.down);
    if (event.key != ImGuiKey_None) io.AddKeyEvent(event.key, event.down);
    if (event.wheel != 0) io.AddMouseWheelEvent(0, event.wheel);
    if (!event.text.empty()) io.AddInputCharactersUTF8(event.text.c_str());
}

void App::RegisterAiInterface() {
    using namespace ai;
    bridge.Register({"shaderixel.ui_state", "Observe les panneaux et contrôles visibles dans la fenêtre principale de Shaderixel. Les rectangles sont en pixels relatifs à la capture. query recherche aussi dans le catalogue des menus et panneaux.",
        Schema({{"query", T("string")}, {"limit", Range("integer", 1, 200)}, {"offset", Range("integer", 0, 2048)}}), false, {},
        [](const Json& args, const std::string&) {
            auto& search = GlobalSearch::Get();
            const unsigned before = search.ObservedFrame();
            search.ObserveControls();
            AiResult response;
            response.done = false;
            response.poll = [before, args](AiResult& out) {
                auto& search = GlobalSearch::Get();
                if (before == search.ObservedFrame()) return false;
                const auto* viewport = ImGui::GetMainViewport();
                Json r = Json::Object();
                r["frame"] = search.ObservedFrame();
                r["width"] = viewport->Size.x;
                r["height"] = viewport->Size.y;
                r["indexing"] = search.Crawling();
                r["panels"] = Json::Array();
                for (auto* window : ImGui::GetCurrentContext()->Windows) {
                    if (!window->WasActive || window->Hidden || window->Viewport != viewport ||
                        (window->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Tooltip))) continue;
                    const auto name = std::string(window->Name);
                    if (name.starts_with("###") || name.find("DockSpace") != std::string::npos) continue;
                    Json row = Json::Object();
                    row["name"] = name.substr(0, name.find("##"));
                    row["bounds"] = Bounds(window->Pos.x, window->Pos.y, window->Pos.x + window->Size.x,
                                           window->Pos.y + window->Size.y, viewport->Pos);
                    r["panels"].Push(std::move(row));
                }
                const auto query = GlobalSearch::Fold(args["query"].S());
                const size_t limit = static_cast<size_t>(std::clamp(args["limit"].Num(80), 1.0, 200.0));
                const size_t offset = static_cast<size_t>(std::clamp(args["offset"].Num(), 0.0, 2048.0));
                size_t matches = 0;
                r["controls"] = Json::Array();
                for (const auto& control : search.Controls()) {
                    if (!query.empty() && GlobalSearch::Fold(control.label + " " + Tr(control.label) + " " + control.window).find(query) == std::string::npos) continue;
                    if (matches++ < offset || r["controls"].Size() >= limit) continue;
                    Json row = Json::Object();
                    row["id"] = control.id;
                    row["label"] = control.label;
                    row["window"] = control.window;
                    row["disabled"] = control.disabled;
                    row["bounds"] = Bounds(control.bounds[0], control.bounds[1], control.bounds[2], control.bounds[3], viewport->Pos);
                    r["controls"].Push(std::move(row));
                }
                if (offset + r["controls"].Size() < matches) r["nextOffset"] = static_cast<u32>(offset + r["controls"].Size());
                if (!args["query"].S().empty()) {
                    r["catalog"] = Json::Array();
                    for (const auto* item : search.Query(args["query"].S(), limit)) r["catalog"].Push(DescribeSearchEntry(*item));
                }
                out = AiResult::Ok(std::move(r));
                return true;
            };
            return response;
        }, Schema({{"frame", T("integer")}, {"controls", T("array")}, {"panels", T("array")}, {"catalog", T("array")}},
                  {"frame", "controls", "panels"})});

    bridge.Register({"shaderixel.ui_activate", "Active une commande de menu ou révèle un panneau/contrôle indexé par ui_state. Fournissez le label exact et son chemin si ambigu. Les dialogues et confirmations restent visibles.",
        Schema({{"label", T("string")}, {"kind", Enum({"panel", "menu", "control"})}, {"path", T("array")}}, {"label", "kind"}),
        true, {}, [this](const Json& args, const std::string&) {
            auto& search = GlobalSearch::Get();
            if (aiUiSequence || search.Crawling() || search.Navigating())
                return AiResult::Fail("Busy", "Une navigation de l’interface est déjà en cours");
            const auto label = args["label"].S();
            const auto kind = args["kind"].S();
            if (label.empty()) return AiResult::Fail("InvalidParameters", "Libellé obligatoire");
            const SearchEntry* target = nullptr;
            for (const auto* item : search.Query(label, 2048)) {
                if (kind != SearchKind(item->kind) || (item->label != label && Tr(item->label) != label)) continue;
                if (args.Has("path")) {
                    if (args["path"].Size() != item->path.size()) continue;
                    bool same = true;
                    for (size_t i = 0; i < item->path.size(); ++i) same &= args["path"].At(i).S() == item->path[i];
                    if (!same) continue;
                }
                if (target) return AiResult::Fail("AmbiguousControl", "Utilisez le chemin exact retourné par ui_state");
                target = item;
            }
            if (!target) return AiResult::Fail("UnknownControl", "Aucune commande ou propriété indexée ne correspond");
            if (target->kind == 1 && !bridge.allowFiles)
                return AiResult::Fail("AccessDenied", "L’activation générique des menus nécessite l’accès aux fichiers");
            Json result = DescribeSearchEntry(*target);
            result["dispatched"] = true;
            ActivateSearchResult(*target);
            AiResult pending;
            pending.done = false;
            pending.poll = [result = std::move(result)](AiResult& out) {
                if (GlobalSearch::Get().Navigating()) return false;
                out = AiResult::Ok(result);
                return true;
            };
            return pending;
        }, Schema({{"dispatched", T("boolean")}}, {"dispatched"})});

    bridge.Register({"shaderixel.ui_input", "Pilote uniquement l’interface interne de Shaderixel. Actions click, drag, scroll, key ou text. Les coordonnées pixel proviennent de ui_state ou ui_capture. Un clic peut cibler id + frame de ui_state.",
        Schema({{"actions", T("array", "Suite de {action,x,y,button?}, {action:'drag',x,y,toX,toY,steps?}, {action:'scroll',x,y,delta}, {action:'key',key,ctrl?,shift?,alt?}, {action:'text',text}")}}, {"actions"}),
        true, {}, [this](const Json& args, const std::string&) {
            if (aiUiSequence || GlobalSearch::Get().Crawling() || GlobalSearch::Get().Navigating())
                return AiResult::Fail("Busy", "Une interaction de l’interface est déjà en cours");
            const auto& input = args["actions"];
            if (!input.IsArray() || !input.Size() || input.Size() > 32)
                return AiResult::Fail("InvalidParameters", "Fournissez entre 1 et 32 actions");
            auto sequence = std::make_shared<AiUiSequence>();
            using Event = AiUiSequence::Event;
            const auto* viewport = ImGui::GetMainViewport();
            auto inView = [viewport](ImVec2 p) {
                return p.x >= 0 && p.y >= 0 && p.x < viewport->Size.x && p.y < viewport->Size.y;
            };
            for (const auto& step : input.Elements()) {
                const auto action = step["action"].S();
                Event event;
                if (action == "text") {
                    event.text = step["text"].S();
                    if (event.text.empty() || event.text.size() > 8192 || event.text.find('\0') != std::string::npos)
                        return AiResult::Fail("InvalidParameters", "Texte de saisie absent ou trop long");
                    sequence->events.push_back(std::move(event));
                    continue;
                }
                if (action == "key") {
                    event.key = InputKey(step["key"].S());
                    if (event.key == ImGuiKey_None) return AiResult::Fail("InvalidParameters", "Touche inconnue");
                    for (auto [label, modifier] : {std::pair{"ctrl", ImGuiMod_Ctrl}, {"shift", ImGuiMod_Shift}, {"alt", ImGuiMod_Alt}})
                        if (step[label].B()) sequence->events.push_back(Event{.key = modifier, .down = true});
                    event.down = true;
                    sequence->events.push_back(event);
                    event.down = false;
                    sequence->events.push_back(event);
                    for (auto [label, modifier] : {std::pair{"ctrl", ImGuiMod_Ctrl}, {"shift", ImGuiMod_Shift}, {"alt", ImGuiMod_Alt}})
                        if (step[label].B()) sequence->events.push_back(Event{.key = modifier});
                    continue;
                }
                ImVec2 point(step["x"].F(-1), step["y"].F(-1));
                if (step.Has("id")) {
                    auto& search = GlobalSearch::Get();
                    if (action != "click" || step["frame"].Num() != search.ObservedFrame())
                        return AiResult::Fail("StaleControl", "Actualisez ui_state avant de cliquer sur ce contrôle");
                    const auto& controls = search.Controls();
                    auto item = std::ranges::find(controls, step["id"].Num(), &VisibleControl::id);
                    if (item == controls.end() || item->disabled)
                        return AiResult::Fail("UnknownControl", "Contrôle absent ou désactivé");
                    point = {(item->bounds[0] + item->bounds[2]) / 2 - viewport->Pos.x,
                             (item->bounds[1] + item->bounds[3]) / 2 - viewport->Pos.y};
                }
                if (!inView(point)) return AiResult::Fail("InvalidParameters", "Point en dehors de la fenêtre Shaderixel");
                event.move = true;
                event.point = point;
                if (action == "scroll") {
                    event.wheel = std::clamp(step["delta"].F(), -10.0f, 10.0f);
                    sequence->events.push_back(event);
                    continue;
                }
                if (action != "click" && action != "drag") return AiResult::Fail("InvalidParameters", "Action d’interface inconnue");
                const auto button = step["button"].S("left");
                if (button != "left" && button != "right" && button != "middle")
                    return AiResult::Fail("InvalidParameters", "Bouton attendu : left, right, middle");
                sequence->events.push_back(event); // hover first so the next frame routes the click
                event.button = button == "left" ? 0 : button == "right" ? 1 : 2;
                event.down = true;
                sequence->events.push_back(event);
                if (action == "drag") {
                    const ImVec2 end(step["toX"].F(-1), step["toY"].F(-1));
                    if (!inView(end)) return AiResult::Fail("InvalidParameters", "Point de fin en dehors de Shaderixel");
                    const int count = static_cast<int>(std::clamp(step["steps"].Num(10), 1.0, 60.0));
                    for (int i = 1; i <= count; ++i) {
                        const float t = static_cast<float>(i) / count;
                        event.point = {point.x + (end.x - point.x) * t, point.y + (end.y - point.y) * t};
                        auto motion = event;
                        motion.button = -1;
                        sequence->events.push_back(std::move(motion));
                    }
                }
                event.down = false;
                sequence->events.push_back(event);
            }
            if (sequence->events.size() > 280) return AiResult::Fail("InvalidParameters", "Séquence trop longue");
            aiUiSequence = sequence;
            AiResult pending;
            pending.done = false;
            pending.poll = [this, sequence](AiResult& out) {
                if (aiUiSequence == sequence) return false;
                if (sequence->next != sequence->events.size()) out = AiResult::Fail("AccessDenied", "Autorisation d’interface retirée");
                else out.result["events"] = static_cast<u32>(sequence->events.size());
                return true;
            };
            return pending;
        }, Schema({{"events", T("integer")}}, {"events"}), true});

    bridge.Register({"shaderixel.ui_capture", "Capture la fenêtre principale de Shaderixel en PNG privé. La capture ne couvre ni le bureau ni les autres applications.",
        Schema({}), false, {}, [this](const Json&, const std::string& workspace) {
            if (workspace.empty()) return AiResult::Fail("FileError", "Espace privé de capture indisponible");
            if (!capturePath.empty()) return AiResult::Fail("Busy", "Une capture est déjà programmée");
            auto pixels = aiUiCapture.lock();
            if (!pixels || pixels->ready) pixels = gpu::Dev().CaptureNextFrame();
            if (!pixels) return AiResult::Fail("NotReady", "Capture indisponible");
            aiUiCapture = pixels;
            const auto path = Utf8Path(workspace) / L"render.png";
            AiResult pending;
            pending.done = false;
            pending.poll = [pixels, path](AiResult& out) {
                if (!pixels->ready) return false;
                auto& data = pixels->data;
                if (!pixels->width || !pixels->height || data.size() != static_cast<size_t>(pixels->width) * pixels->height * 4) {
                    out = AiResult::Fail("CaptureFailed", "Capture GPU incomplète");
                    return true;
                }
                for (size_t i = 3; i < data.size(); i += 4) data[i] = 255;
                std::ofstream png(path, std::ios::binary | std::ios::trunc);
                auto write = [](void* stream, void* bytes, int count) {
                    static_cast<std::ofstream*>(stream)->write(static_cast<char*>(bytes), count);
                };
                if (!png || !stbi_write_png_to_func(write, &png, pixels->width, pixels->height, 4, data.data(), pixels->width * 4) || !png.flush()) {
                    out = AiResult::Fail("WriteFailed", "Impossible d’encoder cette capture");
                } else {
                    out.result["width"] = pixels->width;
                    out.result["height"] = pixels->height;
                    out.result["file"] = Narrow(path.wstring());
                }
                return true;
            };
            return pending;
        }});

    bridge.Register({"shaderixel.documents", "Liste les onglets ouverts, leurs dimensions, leur état modifié et l’index de l’onglet actif.",
        Schema({}), false, {}, [this](const Json&, const std::string&) {
            Json j = Json::Object();
            j["active"] = activeDoc;
            j["documents"] = Json::Array();
            for (size_t i = 0; i < docs.size(); ++i) {
                const auto& d = *docs[i]->doc;
                Json row = Json::Object();
                row["index"] = static_cast<u32>(i);
                row["name"] = d.Name();
                row["width"] = d.Width(); row["height"] = d.Height();
                row["paint3d"] = d.kind == 1;
                row["dirty"] = static_cast<bool>(d.dirty);
                j["documents"].Push(std::move(row));
            }
            return AiResult::Ok(std::move(j));
        }, Schema({{"documents", T("array")}, {"active", T("integer")}}, {"documents", "active"})});

    bridge.Register({"shaderixel.document_tab", "Active ou ferme l’onglet précisé par documents. La fermeture conserve la demande de sauvegarde pour un document modifié.",
        Schema({{"index", Range("integer", 0, 4096)}, {"action", Enum({"activate", "close"})}}, {"index", "action"}), true, {},
        [this](const Json& args, const std::string&) {
            const auto index = static_cast<size_t>(args["index"].Num(4096));
            if (index >= docs.size()) return AiResult::Fail("NoDocument", "Onglet introuvable");
            Json response = Json::Object();
            if (args["action"].S() == "close") {
                const auto count = docs.size();
                CloseDocument(static_cast<int>(index));
                response["closed"] = docs.size() < count;
                response["confirmationPending"] = docs.size() == count;
            } else {
                activeDoc = static_cast<int>(index);
                focusDocRequest = activeDoc;
                response["active"] = activeDoc;
            }
            return AiResult::Ok(std::move(response));
        }});

    bridge.Register({"shaderixel.document_open", "Ouvre un projet SXL/SX3D ou une image dans un onglet. Attend l’ouverture des projets natifs chargés en arrière-plan.",
        Schema({{"path", T("string")}}, {"path"}), true, {"path"}, [this](const Json& args, const std::string&) {
            const auto path = args["path"].S();
            const auto file = Utf8Path(path);
            if (!file.is_absolute() || !std::filesystem::is_regular_file(file))
                return AiResult::Fail("FileError", "Chemin absolu d’un fichier existant requis");
            if (projectLoad) return AiResult::Fail("Busy", "Un projet est en cours d’ouverture");
            auto extension = file.extension().wstring();
            std::ranges::transform(extension, extension.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
            if (extension == L".uasset") return AiResult::Fail("InvalidParameters", "Utilisez la conversion Unreal depuis l’interface");
            const auto count = docs.size();
            OpenImage(path, false);
            const auto status = [this, path, count](AiResult& out) {
                if (projectLoad) return false;
                if (docs.size() <= count) out = AiResult::Fail("OpenFailed", statusMsg);
                else {
                    const auto found = std::ranges::find_if(docs, [&](const auto& view) { return view->doc->path == path; });
                    out.result["index"] = found == docs.end() ? activeDoc : static_cast<int>(found - docs.begin());
                    out.result["path"] = path;
                }
                return true;
            };
            AiResult result;
            result.done = false;
            result.poll = status;
            return result;
        }, Schema({{"index", T("integer")}, {"path", T("string")}}, {"index", "path"})});

    bridge.Register({"shaderixel.actions_list", "Liste les commandes d’automatisation de l’application et les actions prédéfinies ou enregistrées.",
        Schema({}), false, {}, [this](const Json&, const std::string&) {
            Json r = Json::Object();
            r["commands"] = ActionCommands();
            auto list = [](const std::vector<Action>& source) {
                Json rows = Json::Array();
                for (size_t i = 0; i < source.size(); ++i) {
                    Json row = Json::Object();
                    row["index"] = static_cast<u32>(i);
                    row["name"] = source[i].name;
                    row["steps"] = static_cast<u32>(source[i].steps.size());
                    row["runnable"] = std::ranges::all_of(source[i].steps, [](const auto& step) {
                        return !step.enabled || ActionWithoutFiles(step.cmd);
                    });
                    rows.Push(std::move(row));
                }
                return rows;
            };
            r["builtin"] = list(BuiltinActions());
            r["saved"] = list(actions);
            return AiResult::Ok(std::move(r));
        }});

    bridge.Register({"shaderixel.action_run", "Exécute sur le document actif une commande de actions_list (redimensionnement, rotation, fusion, filtre, etc.) ou une action enregistrée. Utilise le moteur d’actions de Shaderixel.",
        Schema({{"command", T("string")}, {"arguments", T("object")}, {"index", Range("integer", 0, 65535)}, {"scope", Enum({"builtin", "saved"})}}), true, {},
        [this](const Json& args, const std::string&) {
            auto* view = Active();
            if (!view) return AiResult::Fail("NoDocument", "Aucun document ouvert");
            std::vector<ActionStep> steps;
            std::string name;
            if (args.Has("command")) {
                name = args["command"].S();
                Json params = ActionDefaultParams(name);
                for (const auto& [key, value] : args["arguments"].Items()) params[key] = value;
                steps.push_back({name, std::move(params), true});
            } else {
                const auto& source = args["scope"].S("saved") == "builtin" ? BuiltinActions() : actions;
                const auto index = static_cast<size_t>(args["index"].Num(65536));
                if (index >= source.size()) return AiResult::Fail("InvalidParameters", "Action introuvable");
                steps = source[index].steps;
                name = source[index].name;
            }
            std::erase_if(steps, [](const auto& step) { return !step.enabled; });
            for (const auto& step : steps)
                if (!ActionWithoutFiles(step.cmd))
                    return AiResult::Fail("InvalidParameters", "Commande absente ou nécessitant un appel séparé : " + step.cmd);
            if (steps.empty()) return AiResult::Fail("InvalidParameters", "Action sans étape");
            auto done = std::make_shared<std::optional<bool>>();
            if (!StartActionSteps(*view, name, steps, 0, static_cast<int>(steps.size()) - 1, {}, [done](bool ok) { *done = ok; }))
                return AiResult::Fail("Busy", "Une action est déjà en cours");
            AiResult result;
            result.done = false;
            result.poll = [done, name](AiResult& out) {
                if (!*done) return false;
                if (!done->value()) out = AiResult::Fail("ActionFailed", "Une étape de l’action a échoué");
                else out.result["action"] = name;
                return true;
            };
            return result;
        }});
}

}  // namespace sx
