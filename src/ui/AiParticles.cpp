#include "ui/App.h"
#include "ui/AiSchema.h"

namespace sx {
namespace {

Json BrushDescription(const auto& brush) {
    Json r = Json::Object();
    r["brush"] = brush.ToJson();
    r["controls"] = Json::Array();
    r["groups"] = Json::Array();
    if (!brush.program) return r;
    const auto& controls = brush.program->Controls();
    for (size_t i = 0; i < controls.size(); ++i) {
        const auto& control = controls[i];
        Json j = Json::Object();
        j["id"] = control.id;
        j["label"] = control.label;
        j["visible"] = brush.program->IsControlVisible(i, brush.scriptValues);
        j["kind"] = control.kind == particles::ControlKind::Checkbox ? "checkbox" :
                    control.kind == particles::ControlKind::Choice ? "choice" : "slider";
        j["default"] = control.kind == particles::ControlKind::Checkbox ? Json(control.initial != 0) : Json(control.initial);
        if (control.kind == particles::ControlKind::Slider) {
            j["min"] = control.min;
            j["max"] = control.max;
        } else if (control.kind == particles::ControlKind::Choice) {
            j["choices"] = Json::Array();
            for (const auto& option : control.options) {
                Json row = Json::Object();
                row["label"] = option.label;
                row["value"] = option.value;
                j["choices"].Push(std::move(row));
            }
        }
        r["controls"].Push(std::move(j));
    }
    const auto& groups = brush.program->Groups();
    for (size_t i = 0; i < groups.size(); ++i) {
        const auto& group = groups[i];
        Json j = Json::Object();
        j["id"] = group.id;
        j["label"] = group.label;
        j["defaultOpen"] = group.defaultOpen;
        j["open"] = brush.scriptGroupOpen[i].value_or(group.defaultOpen);
        j["visible"] = brush.program->IsGroupVisible(i, brush.scriptValues);
        j["controls"] = Json::Array();
        for (auto k = group.firstControl; k < group.endControl; ++k) j["controls"].Push(controls[k].id);
        r["groups"].Push(std::move(j));
    }
    return r;
}

}  // namespace

void App::RegisterAiParticleOps() {
    using namespace ai;
    const auto noMesh = [] { return AiResult::Fail("NoMesh", "Ouvrez un document avec un maillage"); };
    bridge.Register({"shaderixel.particles_info", "Liste les brosses de particules, le script actif, les contrôles et groupes conditionnels, et l’état du geste.",
        Schema({}), false, {}, [this](const Json&, const std::string&) {
            Json r = BrushDescription(particleBrush);
            r["presets"] = Json::Array(); r["saved"] = Json::Array();
            for (int i = 0; i < SimPresetCount(); ++i) {
                Json row = Json::Object();
                row["index"] = i; row["name"] = SimPresetName(i);
                r["presets"].Push(std::move(row));
            }
            for (const auto& b : particlePresets) r["saved"].Push(b.name);
            r["playing"] = sim.playing;
            r["gesture"] = sim.view && !sim.inputs.empty();
            return AiResult::Ok(std::move(r));
        }, Schema({{"brush", T("object")}, {"controls", T("array")}, {"groups", T("array")}},
                  {"brush", "controls", "groups"})});

    bridge.Register({"shaderixel.particles_configure", "Sélectionne un preset ou une brosse sauvegardée, compile le script émission/évolution/impact, modifie les propriétés et les réglages déclarés. resetGroup ou resetAll restaurent aussi l’ouverture.",
        Schema({{"preset", Range("integer", 0, 4096)}, {"saved", T("string")}, {"script", T("string")},
                {"settings", T("object", "Propriétés de brush dans particles_info, hors particleScript")},
                {"values", T("object", "Contrôles indexés par identifiant. Un choix accepte son libellé exact ou sa valeur.")},
                {"groupOpen", T("object")}, {"resetGroup", T("string")}, {"resetAll", T("boolean")}}),
        true, {}, [this](const Json& p, const std::string&) {
            if (sim.playing || aiParticleSequence) return AiResult::Fail("Busy", "Attendez la fin de la simulation");
            auto selected = particleBrush;
            if (p.Has("preset") && p.Has("saved")) return AiResult::Fail("InvalidParameters", "Choisissez preset ou saved");
            if (p.Has("preset")) {
                const int index = p["preset"].I();
                if (index < 0 || index >= SimPresetCount()) return AiResult::Fail("InvalidParameters", "Preset de particules inconnu");
                selected = DefaultParticleBrush(index);
                if (!selected.customMaterial) { selected.flow = brush.flow; selected.opacity = brush.opacity; }
            }
            if (p.Has("saved")) {
                const auto found = std::ranges::find(particlePresets, p["saved"].S(), &ParticleBrush::name);
                if (found == particlePresets.end()) return AiResult::Fail("InvalidParameters", "Brosse sauvegardée inconnue");
                selected = *found;
            }
            if (p.Has("settings")) {
                auto merged = selected.ToJson();
                for (const auto& [key, value] : p["settings"].Items()) {
                    if (key == "particleScript" || !merged.Has(key) || merged[key].GetType() != value.GetType())
                        return AiResult::Fail("InvalidParameters", "Propriété de brosse incorrecte : " + key);
                    merged[key] = value;
                }
                selected = ParticleBrush::FromJson(merged);
            }
            if (p.Has("script")) {
                const auto source = p["script"].S();
                if (source.empty()) {
                    selected.script.clear(); selected.program.reset();
                    selected.scriptValues = {}; selected.scriptGroupOpen = {};
                } else {
                    particles::Diagnostic error;
                    if (!selected.SetScript(source, error))
                        return AiResult::Fail("ScriptError", "Ligne " + std::to_string(error.line + 1) + " : " + error.message);
                }
            }
            if (p.Has("values") || p.Has("groupOpen") || p.Has("resetGroup") || p["resetAll"].B()) {
                if (!selected.program) return AiResult::Fail("InvalidParameters", "La brosse n’a aucun script valide");
                const auto& controls = selected.program->Controls();
                const auto& groups = selected.program->Groups();
                if (p["resetAll"].B()) {
                    for (size_t i = 0; i < controls.size(); ++i) selected.scriptValues[i] = controls[i].initial;
                    selected.scriptGroupOpen = {};
                }
                if (p.Has("resetGroup")) {
                    const auto group = std::ranges::find(groups, p["resetGroup"].S(), &particles::ControlGroup::id);
                    if (group == groups.end()) return AiResult::Fail("InvalidParameters", "Groupe de script inconnu");
                    for (auto i = group->firstControl; i < group->endControl; ++i) selected.scriptValues[i] = controls[i].initial;
                    selected.scriptGroupOpen[group - groups.begin()].reset();
                }
                for (const auto& [key, value] : p["values"].Items()) {
                    const auto control = std::ranges::find(controls, key, &particles::Control::id);
                    if (control == controls.end()) return AiResult::Fail("InvalidParameters", "Contrôle de script inconnu : " + key);
                    float result = 0;
                    if (control->kind == particles::ControlKind::Checkbox) {
                        if (value.GetType() != Json::Type::Bool) return AiResult::Fail("InvalidParameters", key + " attend un booléen");
                        result = value.B() ? 1.f : 0.f;
                    } else if (control->kind == particles::ControlKind::Choice) {
                        const auto option = std::ranges::find_if(control->options, [&](const auto& item) {
                            return (value.GetType() == Json::Type::String && value.S() == item.label) ||
                                   (value.GetType() == Json::Type::Number && value.F() == item.value);
                        });
                        if (option == control->options.end()) return AiResult::Fail("InvalidParameters", key + " attend un choix déclaré");
                        result = option->value;
                    } else {
                        if (value.GetType() != Json::Type::Number) return AiResult::Fail("InvalidParameters", key + " attend un nombre");
                        result = control->Restore(value.Num());
                    }
                    selected.scriptValues[control - controls.begin()] = result;
                }
                for (const auto& [key, value] : p["groupOpen"].Items()) {
                    const auto group = std::ranges::find(groups, key, &particles::ControlGroup::id);
                    if (group == groups.end() || value.GetType() != Json::Type::Bool)
                        return AiResult::Fail("InvalidParameters", "État de groupe invalide : " + key);
                    selected.scriptGroupOpen[group - groups.begin()] = value.B();
                }
            }
            particleBrush = std::move(selected);
            return AiResult::Ok(BrushDescription(particleBrush));
        }});

    bridge.Register({"shaderixel.particles_save", "Enregistre la brosse active avec son script, sa matière, ses valeurs et l’ouverture des groupes. overwrite confirme le remplacement d’un nom existant.",
        Schema({{"name", T("string")}, {"overwrite", T("boolean")}}, {"name"}), true, {},
        [this](const Json& p, const std::string&) {
            const auto name = p["name"].S();
            if (name.empty() || name.size() > 160) return AiResult::Fail("InvalidParameters", "Nom de brosse incorrect");
            const auto before = particlePresets;
            auto saved = particleBrush;
            saved.name = name;
            auto existing = std::ranges::find(particlePresets, name, &ParticleBrush::name);
            if (existing != particlePresets.end()) {
                if (!p["overwrite"].B()) return AiResult::Fail("ConfirmationRequired", "Une brosse porte déjà ce nom");
                *existing = saved;
            } else particlePresets.push_back(saved);
            if (!SaveAiSettings()) {
                particlePresets = before;
                return AiResult::Fail("WriteFailed", "Impossible d’enregistrer la brosse");
            }
            particleBrush.name = name;
            return AiResult::Ok(BrushDescription(saved));
        }, {}, true});

    bridge.Register({"shaderixel.particles_gesture", "Lit l’aperçu du dernier geste animé sous forme de JSON rejouable sur le même modèle. Il comprend le script et les réglages du geste.",
        Schema({}), false, {}, [this, noMesh](const Json&, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh) return noMesh();
            if (sim.view != v || sim.inputs.empty()) return AiResult::Fail("NoGesture", "Aucun aperçu de particules à lire");
            if (sim.playing) return AiResult::Fail("Busy", "Attendez la fin du calcul");
            return AiResult::Ok(ParticleGestureJson(v->mesh->data));
        }});

    bridge.Register({"shaderixel.particles_play", "Émet, rejoue, applique ou annule le geste de particules. emit utilise des points normalisés, une sélection UV ou un tracé. replay réutilise les paramètres actuels, sauf si un geste JSON a été fourni.",
        Schema({{"action", Enum({"emit", "replay", "commit", "cancel"})}, {"points", T("array", "[[x,y], ...] dans la vue normalisée")},
                {"ticksPerPoint", Range("integer", 1, 120)}, {"viewWidth", Range("integer", 128, 4096)},
                {"viewHeight", Range("integer", 128, 4096)}, {"pathLayer", Range("integer", 1, 4294967295.0)},
                {"gesture", T("object", "Résultat de particles_gesture")}}, {"action"}), true, {},
        [this, noMesh](const Json& p, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh) return noMesh();
            const auto action = p["action"].S();
            if (action == "commit" || action == "cancel") {
                if (sim.view != v) return AiResult::Fail("NoGesture", "Aucun aperçu de particules actif");
                if (action == "commit" && sim.playing) return AiResult::Fail("Busy", "Attendez la fin du calcul");
                if (aiParticleSequence) {
                    aiParticleSequence->failed = true;
                    aiParticleSequence->finished = true;
                    aiParticleSequence.reset();
                }
                FinishParticles(action == "commit");
                Json r = Json::Object(); r["applied"] = action == "commit";
                return AiResult::Ok(std::move(r));
            }
            if (aiParticleSequence || sim.playing || meshStroking || fluidRun.active)
                return AiResult::Fail("Busy", "Terminez le geste en cours");
            auto command = std::make_shared<AiParticleSequence>();
            command->view = v;
            command->ticksPerPoint = std::clamp(p["ticksPerPoint"].I(8), 1, 120);
            if (action == "replay") {
                if (p.Has("gesture")) {
                    const auto& inputs = p["gesture"]["inputs"];
                    if (inputs.Size() == 0 || inputs.Size() > 3600 || inputs.At(inputs.Size() - 1).At(0).Num() > 3600)
                        return AiResult::Fail("InvalidParameters", "Geste enregistré absent ou trop long");
                    if (!LoadParticleGesture(*v, p["gesture"]))
                        return AiResult::Fail("InvalidGesture", "Le geste ne correspond pas au maillage ou son script est invalide");
                } else {
                    if (sim.view != v || sim.inputs.empty()) return AiResult::Fail("NoGesture", "Aucun geste à rejouer");
                    if (!BeginParticleStroke(*v, true)) return AiResult::Fail("NotReady", statusMsg);
                }
            } else if (action == "emit") {
                const auto w = static_cast<u32>(std::clamp(p["viewWidth"].Num(1024), 128.0, 4096.0));
                const auto h = static_cast<u32>(std::clamp(p["viewHeight"].Num(1024), 128.0, 4096.0));
                if (particleBrush.source == 0) {
                    if (!p["points"].IsArray() || p["points"].Size() == 0 || p["points"].Size() > 128 ||
                        p["points"].Size() * command->ticksPerPoint > 3600)
                        return AiResult::Fail("InvalidParameters", "Fournissez des points normalisés pour un geste de 30 secondes maximum");
                    v->mesh->UpdateCamera(static_cast<float>(w) / h);
                    for (const auto& point : p["points"].Elements()) {
                        if (!point.IsArray() || point.Size() != 2 ||
                            point.At(0).GetType() != Json::Type::Number || point.At(1).GetType() != Json::Type::Number ||
                            point.At(0).Num() < 0 || point.At(0).Num() > 1 || point.At(1).Num() < 0 || point.At(1).Num() > 1)
                            return AiResult::Fail("InvalidParameters", "Point de particules incorrect");
                        auto origin = ParticleOriginAt(*v->mesh, {point.At(0).F() * w, point.At(1).F() * h}, w, h);
                        if (!origin.valid) return AiResult::Fail("NoSurface", "Un point ne touche pas le maillage");
                        command->origins.push_back(origin);
                    }
                }
                if (particleBrush.source == 1 && !v->doc->Selection())
                    return AiResult::Fail("InvalidParameters", "Créez une sélection UV");
                if (particleBrush.source == 2 && p.Has("pathLayer")) {
                    const auto* layer = v->doc->FindLayer(p["pathLayer"].U());
                    if (!layer || layer->surfacePath.empty()) return AiResult::Fail("InvalidParameters", "Tracé de surface invalide");
                    v->particlePathSource = layer->id;
                }
                if (sim.view) FinishParticles(true);
                if (!BeginParticleStroke(*v)) return AiResult::Fail("NotReady", statusMsg);
                if (particleBrush.source == 1) ParticleSelectionOrigins(*v);
                else if (particleBrush.source == 2) sim.queued = ParticlePathOrigins(*v, sim.origins);
                if (particleBrush.source != 0 && !sim.sourceMask && !sim.queued) {
                    FinishParticles(false);
                    return AiResult::Fail("InvalidParameters", "La sélection ou le tracé n’a pas de surface d’émission");
                }
            } else return AiResult::Fail("InvalidParameters", "Action de particules inconnue");
            tool = viewTool[1] = Tool::Particles;
            workView = 1;
            aiParticleSequence = command;
            AiResult result;
            result.done = false;
            result.poll = [this, command](AiResult& out) {
                if (!command->finished) return false;
                if (command->failed) out = AiResult::Fail("SimulationInterrupted", "La simulation a été interrompue");
                else {
                    out.result["preview"] = sim.view == command->view && !sim.inputs.empty();
                    out.result["ticks"] = sim.view == command->view ? sim.tick : 0;
                }
                return true;
            };
            return result;
        }});
}

}  // namespace sx
