#include "ui/App.h"
#include "render/Dispatch.h"
#include <TextEditor.h>
#include <imgui_internal.h>
#include <bit>
#include <fstream>

namespace sx {
namespace demo {
extern void (*observeItem)(ImGuiID, const ImRect&);
extern void (*observeCodeEditor)(const std::string*, TextEditor&);
}

namespace {
constexpr const char* kParticlePanel = "Matériau du pinceau";
constexpr const char* kParticlePopup = "Éditeur de particules##particle-code";
constexpr const char* kImpactEdit = " impact_radius = radius * 4; strength = 0.08;";
constexpr float kSavedRadius = 2.6f, kSavedForce = 0.55f, kSavedBounce = 0.64f;

struct ParticleEditorInput {
    ImGuiID id = 0;
    ImGuiID impactSlider = 0, bounceSlider = 0, bounceGroup = 0;
    ImRect rect{};
    int observedFrame = -1;
    int impactFrame = -1, bounceFrame = -1, bounceGroupFrame = -1;
    int mouseStep = 0;
    int stage = 0;
    u64 entered = 0;
    std::string expected;
    std::string groupPreset;
    const std::string* source = nullptr;
    size_t editLine = 0;
    bool caretReady = false;
    float bounceStrength = 0;

    void Next(u64 frame) {
        ++stage;
        entered = frame;
    }

    bool Click(ImVec2 point, float dragX) {
        ImGuiIO& io = ImGui::GetIO();
        switch (mouseStep++) {
            case 0: io.AddMousePosEvent(point.x, point.y); return false;
            case 1: io.AddMouseButtonEvent(ImGuiMouseButton_Left, true); return false;
            case 2:
                if (dragX != 0) {
                    io.AddMousePosEvent(point.x + dragX, point.y);
                    return false;
                }
                [[fallthrough]];
            default:
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
                mouseStep = 0;
                return true;
        }
    }

    bool Click(ImGuiWindow* window, ImGuiID target, float dragFraction = 0) {
        if (!window) return false;
        if (target != id) {
            id = target;
            observedFrame = -1;
            return false;
        }
        // This demo runs before these panels are drawn. Use the previous frame's item position.
        if (observedFrame != ImGui::GetFrameCount() - 1) return false;
        ImRect visible = rect;
        visible.ClipWith(window->InnerClipRect);
        if (visible.GetHeight() < rect.GetHeight() * 0.8f) {
            ImGui::SetScrollY(window, window->Scroll.y + rect.GetCenter().y - window->InnerRect.GetCenter().y);
            return false;
        }
        return Click(visible.GetCenter(),visible.GetWidth()*dragFraction);
    }
};

ParticleEditorInput g_editorInput;

MeshData RivetPanel() {
    MeshData mesh;
    const auto quad = [&](float bottom, float top, float v0, float v1) {
        const u32 start = (u32)mesh.vertices.size();
        mesh.vertices.push_back({{-0.5f, top, 0}, {0,0,-1}, {0.07f,v0}});
        mesh.vertices.push_back({{0.5f, top, 0}, {0,0,-1}, {0.93f,v0}});
        mesh.vertices.push_back({{-0.5f, bottom, 0}, {0,0,-1}, {0.07f,v1}});
        mesh.vertices.push_back({{0.5f, bottom, 0}, {0,0,-1}, {0.93f,v1}});
        for (u32 i : {0u,1u,2u,2u,1u,3u}) mesh.indices.push_back(start+i);
    };
    quad(0, 0.9f, 0.07f, 0.42f);
    quad(-0.9f, 0, 0.58f, 0.93f);
    mesh.boundsMin = {-0.5f,-0.9f,0};
    mesh.boundsMax = {0.5f,0.9f,0};
    mesh.name = "Rivet panel with a UV seam";
    return mesh;
}

// A curved mesh with four independent UV charts and matching positions along their seams.
MeshData DenseParticlePanel() {
    constexpr int subdivisions = 256;
    MeshData mesh;
    mesh.name = "Dense four-UDIM particle panel";
    for (int tileY = 0; tileY < 2; ++tileY) for (int tileX = 0; tileX < 2; ++tileX) {
        const u32 start = (u32)mesh.vertices.size();
        for (int y = 0; y <= subdivisions; ++y) for (int x = 0; x <= subdivisions; ++x) {
            const float u = (float)x / subdivisions, v = (float)y / subdivisions;
            const float wx = (tileX + u - 1) * 0.55f, wy = (1 - tileY - v) * 0.9f;
            const float waveX = wx * 28, waveY = wy * 21;
            const float dzdx = 0.004f * 28 * std::cos(waveX) * std::sin(waveY);
            const float dzdy = 1 + 0.004f * 21 * std::sin(waveX) * std::cos(waveY);
            const float z = wy + 0.004f * std::sin(waveX) * std::sin(waveY);
            mesh.vertices.push_back({{wx,wy,z},Normalize({dzdx,dzdy,-1}),{tileX + 0.03f + u*0.94f,0.03f + v*0.94f - tileY}});
        }
        for (int y = 0; y < subdivisions; ++y) for (int x = 0; x < subdivisions; ++x) {
            const u32 a = start + y*(subdivisions+1) + x, b = a + subdivisions+1;
            mesh.indices.insert(mesh.indices.end(),{a,a+1,b,b,a+1,b+1});
        }
    }
    mesh.boundsMin = {-0.55f,-0.9f,-0.904f};
    mesh.boundsMax = {0.55f,0.9f,0.904f};
    return mesh;
}

bool ParticleReferencePixels(const std::string& name, const std::vector<u8>& pixels, bool write) {
    const auto path = Utf8Path(name);
    if (write) {
        std::ofstream file(path,std::ios::binary);
        file.write((const char*)pixels.data(),pixels.size());
        file.close();
        return !!file;
    }
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file || file.tellg() != (std::streamoff)pixels.size()) return false;
    file.seekg(0);
    std::vector<char> chunk(1u << 20);
    for (size_t offset = 0; offset < pixels.size(); offset += chunk.size()) {
        const size_t bytes = std::min(chunk.size(),pixels.size()-offset);
        if (!file.read(chunk.data(),bytes) || memcmp(chunk.data(),pixels.data()+offset,bytes)) return false;
    }
    return true;
}
}

void App::DemoPaintParticles() {
    if (const char* preset = getenv("SX_PARTICLE_4K")) {
        DemoParticlePerformance(atoi(preset));
        return;
    }
    constexpr int size = 256;
    static const bool scripted = getenv("SX_PARTICLE_SCRIPT_TEST") != nullptr;
    static int phase = 0, failures = 0;
    static int frame = 0;
    static std::vector<Id> channels;
    static std::vector<gpu::ReadbackPtr> before, at30, at120, afterReplay, replay120, afterSliders, afterChoice, afterChange, undone, redone;
    static std::vector<gpu::ReadbackPtr> bounce30, bounce120;
    static Json gestureWithSliders;
    static bool replayingAt120 = false;
    static std::vector<gpu::ReadbackPtr> fromSelection, fromWetPath;
    static std::vector<ParticleInput> input30;
    static gpu::ReadbackPtr cachedBounds;
    static const ParticleOrigin rivet{{0,0.68f,0},{0,0,-1},{0,0.68f,-2},{0,0,1},0.018f,true};
    const auto check = [&](bool result, const char* label) {
        Log("[paint-particles] %s %s", result ? "PASS" : "FAIL", label);
        if (!result) ++failures;
    };
    const auto finish = [&] {
        demo::observeItem = nullptr;
        demo::observeCodeEditor = nullptr;
        Log("[paint-particles] COMPLETE %d failures", failures);
        uiErrorCount += failures;
        autoQuit = true;
    };
    if (frameCounter < 4 || autoQuit) return;
    if (frameCounter >= (scripted ? 1600u : 660u)) {
        if (scripted) Log("[paint-particles] timed out in phase %d, editor step %d", phase, g_editorInput.stage);
        check(false,"fixture timed out"); finish(); return;
    }
    if (!phase) {
        NewDocument(size,size,Precision::Half16,DocMode::Layers,0,"Rivet particle regression");
        auto& v = *Active();
        auto& d = *v.doc;
        d.kind = 1;
        for (auto c : {PbrChannel::BaseColor,PbrChannel::Roughness,PbrChannel::Height}) d.pbrChannels |= 1u << (int)c;
        AttachMesh(v, RivetPanel());
        CreatePbrChannels(v, true);
        const Id layer = AddPaintLayer3D(v, "Rivet");
        for (auto c : {PbrChannel::BaseColor,PbrChannel::Roughness,PbrChannel::Height}) {
            const auto* material = MaterialChannelLayer(d, layer, c);
            if (!material) { check(false,"fixture channel"); finish(); return; }
            channels.push_back(material->surface);
        }
        v.renderer.Prepare(d, *ctx);
        if (!v.renderer.MeshPrepare(d, *ctx, *v.mesh)) { check(false,"fixture geometry"); finish(); return; }
        cachedBounds = std::move(v.mesh->brushTileBounds); // reference: dispatch every UV tile
        const auto mask = gpu::Dev().CreateTexture(size,size,gpu::Format::R8,"RivetProtection");
        std::vector<u8> pixels(size*size,255);
        for (int y = 166; y < 178; ++y) std::fill_n(pixels.begin() + y * size, size, 0);
        ctx->Upload(mask.get(), pixels.data(), size);
        v.renderer.SelectFrom(d,*ctx,v.history,mask.get(),SelSource::Mask,SelMode::Replace,{},0,"Protect a strip");
        for (Id channel : channels) before.push_back(ctx->ReadbackTexture(d.GetSurface(channel)->tex.get()));
        v.history.Clear();
        SetParticleBrush(scripted ? SimPresetCount()-1 : 6);
        if (scripted) {
            using namespace particles;
            if (!particleBrush.program) { check(false,"example script compiles"); finish(); return; }
            Diagnostic diagnostic;
            const auto code = particleBrush.program;
            check(!particleBrush.SetScript("emit {\n  age = 3;\n}",diagnostic) && diagnostic.line == 1 && particleBrush.program == code,
                  "invalid edit marks the code line and preserves the applied program");
            check(!Program::Compile("update {\n  ax = 1 + ;\n}",diagnostic) && diagnostic.line == 1,
                  "syntax errors point to the edited line");
            const auto& groups = code->Groups();
            check(groups.size() == 2 && groups[0].id == "impact" && groups[0].firstControl == 0 && groups[0].endControl == 4 &&
                  groups[1].id == "bounce" && groups[1].firstControl == 4 && groups[1].endControl == 5 &&
                  groups[0].defaultOpen && !groups[1].defaultOpen &&
                  code->IsGroupVisible(0,particleBrush.scriptValues) && !code->IsGroupVisible(1,particleBrush.scriptValues) &&
                  code->Controls().size() == 5 && particleBrush.scriptValues[0] == 2.1f && particleBrush.scriptValues[1] == 1 &&
                  particleBrush.scriptValues[2] == 1 && particleBrush.scriptValues[3] == (float)Slide &&
                  particleBrush.scriptValues[4] == 0.35f,
                  "Impact defaults to open and Rebond defaults to collapsed and is hidden for Slide");
            check(!Program::Compile("slider gain \"Gain\" = 2 [0, 1];\nimpact {}",diagnostic) && diagnostic.line == 0 &&
                  !Program::Compile("slider radius \"Radius\" = 1 [0, 1];\nimpact {}",diagnostic) &&
                  !Program::Compile("slider gain \"Gain\" = 1 [0, 1];\nimpact { gain = 0; }",diagnostic),
                  "slider declarations reject invalid defaults, conflicting names and writes");
            check(!!Program::Compile("slider choice \"Choice\" = 1 [0, 2];\nimpact { strength = choice; }",diagnostic),
                  "existing slider identifiers can still use the newly introduced declaration names");
            check(!Program::Compile("checkbox enabled \"Enabled\" = 2;\nimpact {}",diagnostic) && diagnostic.line == 0 &&
                  !Program::Compile("checkbox enabled \"Enabled\" = false;\nimpact { enabled = true; }",diagnostic) &&
                  !Program::Compile("choice mode \"Mode\" = SLIDE [STOP \"Stop\"];\nimpact {}",diagnostic) &&
                  !Program::Compile("choice mode \"Mode\" = STOP [STOP \"A\", STOP \"B\"];\nimpact {}",diagnostic),
                  "checkboxes and lists reject invalid defaults, duplicate option values and assignments");
            const auto conditional = Program::Compile("checkbox enabled \"Enabled\" = false;\n"
                "slider gain \"Gain\" = 2 [0, 3];\n"
                "choice mode \"Mode\" = STOP [STOP \"Stop\"] when enabled && gain > 1;\nimpact {}",diagnostic);
            const std::array conditionValues{1.f,2.f};
            check(conditional && !conditional->IsControlVisible(2,{}) && conditional->IsControlVisible(2,conditionValues) &&
                  !Program::Compile("slider gain \"Gain\" = 1 [0, 2] when speed > 1;\nimpact {}",diagnostic) &&
                  !Program::Compile("checkbox show \"Show\" = true when random();\nimpact {}",diagnostic) &&
                  !Program::Compile("checkbox show \"Show\" = true when later;\ncheckbox later \"Later\" = true;\nimpact {}",diagnostic),
                  "visibility expressions read earlier controls and reject particle fields, randomness and forward references");
            const auto grouped = Program::Compile("checkbox enabled \"Enabled\" = false;\n"
                "group custom \"Custom\" when enabled { slider gain \"Gain\" = 2 [0, 3]; }\n"
                "slider last \"Last\" = 1 [0, 2];\nimpact { strength = gain * last; }",diagnostic);
            check(grouped && grouped->Controls().size() == 3 && grouped->Groups().size() == 1 &&
                  grouped->Groups()[0].firstControl == 1 && grouped->Groups()[0].endControl == 2 &&
                  grouped->Groups()[0].defaultOpen &&
                  !grouped->IsGroupVisible(0,{}) && grouped->IsGroupVisible(0,conditionValues),
                  "legacy groups default to open and can use the same conditional expressions alongside ungrouped controls");
            check(!Program::Compile("group empty \"Empty\" {}\nimpact {}",diagnostic) &&
                  !Program::Compile("group a \"A\" { checkbox x1 \"X1\" = true; }\n"
                                    "group a \"B\" { checkbox x2 \"X2\" = false; }\nimpact {}",diagnostic) &&
                  !Program::Compile("group a \"A\" { group b \"B\" { checkbox x1 \"X1\" = true; } }\nimpact {}",diagnostic),
                  "groups reject empty bodies, duplicate IDs and nested group declarations");
            State params{};
            params[Radius] = 0.025f; params[Life] = 2; params[Age] = 0.2f; params[NY] = 1; params[Speed] = 2;
            u32 seed = 92131;
            code->Run(Stage::Emit,params,seed);
            code->Run(Stage::Update,params,seed);
            code->Run(Stage::Impact,params,seed);
            check(seed != 92131 && params[Radius] != 0.025f && std::abs(params[AX]) > 0.2f &&
                  params[ImpactRadius] > params[Radius] && params[Response] == (float)Slide,
                  "example executes seeded emission, evolution and the default slide response");
            particleBrush.scriptValues[0] = kSavedRadius;
            particleBrush.scriptValues[1] = kSavedForce;
            particleBrush.scriptValues[4] = kSavedBounce;
            const float previousRadius = params[ImpactRadius], previousStrength = params[Strength];
            code->Run(Stage::Impact,params,seed,particleBrush.scriptValues);
            check(params[ImpactRadius] > previousRadius && params[Strength] < previousStrength,
                  "the impact reads the brush's radius and force slider values");
            auto presetWithoutValues = particleBrush.ToJson();
            presetWithoutValues["particleScript"].Erase("values");
            check(ParticleBrush::FromJson(presetWithoutValues).scriptValues[0] == 2.1f,
                  "missing saved slider values use the script defaults");
            auto groupPreset = particleBrush;
            groupPreset.scriptGroupOpen[0] = false;
            groupPreset.scriptGroupOpen[1] = true;
            auto savedGroups = groupPreset.ToJson();
            check(ParticleBrush::FromJson(savedGroups).scriptGroupOpen == groupPreset.scriptGroupOpen,
                  "a brush saves each group's chosen presentation state by ID");
            savedGroups["particleScript"]["groupOpen"]["bounce"] = 1;
            check(!ParticleBrush::FromJson(savedGroups).scriptGroupOpen[1].has_value(),
                  "invalid saved presentation states fall back to script defaults");
            check(groupPreset.SetScript("group bounce \"Bounce\" collapsed { checkbox extra \"Extra\" = true; }\n"
                                        "group impact \"Renamed Impact\" open { checkbox deposit \"Deposit\" = true; }\nimpact {}",diagnostic) &&
                  groupPreset.scriptGroupOpen[0] == true && groupPreset.scriptGroupOpen[1] == false,
                  "group state follows its ID when script groups are reordered or renamed");
            auto edited = particleBrush;
            check(edited.SetScript("slider impact_force \"Force\" = 1 [0, 2];\n"
                                   "slider impact_radius_scale \"Radius\" = 1 [-1, 2];\nimpact {}",diagnostic) &&
                  edited.scriptValues[0] == kSavedForce && edited.scriptValues[1] == 2,
                  "applying a reordered script keeps values by ID and clamps to new bounds");
            edited = particleBrush;
            edited.scriptValues[2] = 0;
            edited.scriptValues[3] = Bounce;
            const auto custom = edited.ToJson();
            check(custom["particleScript"]["values"]["paint_impacts"].GetType() == Json::Type::Bool &&
                  ParticleBrush::FromJson(custom).scriptValues == edited.scriptValues,
                  "a saved preset restores an unchecked box and a nondefault choice");
            check(edited.SetScript("choice impact_mode \"Reaction\" = STOP [BOUNCE \"Bounce\", STOP \"Stop\"];\n"
                                   "checkbox paint_impacts \"Deposit\" = true;\nimpact {}",diagnostic) &&
                  edited.scriptValues[0] == (float)Bounce && edited.scriptValues[1] == 0 &&
                  edited.SetScript("choice impact_mode \"Reaction\" = STOP [STOP \"Stop\"];\nimpact {}",diagnostic) &&
                  edited.scriptValues[0] == (float)Stop,
                  "choices survive reordering by value and fall back when removed");

            sim.settings = particleBrush;
            bool behaved = true;
            for (const auto mode : {Slide,Bounce,Stop}) {
                sim.settings.scriptValues[3] = (float)mode;
                SimParticle p;
                p.p = rivet.point; p.n = rivet.normal; p.onSurface = p.justPlaced = true;
                sim.parts = {p};
                std::vector<float4> dabs, extra;
                SimStep(*v.mesh,dabs,extra);
                behaved &= dabs.size() == 1;
                if (mode == Stop) behaved &= sim.parts.empty();
                else if (sim.parts.empty()) behaved = false;
                else if (mode == Slide) behaved &= sim.parts.front().onSurface;
                else behaved &= !sim.parts.front().onSurface && std::abs(Dot(sim.parts.front().v,p.n) - kSavedBounce * 0.1f) < 1e-6f;
            }
            check(behaved,"the selected option controls contact physics and bounce strength changes rebound velocity");
            sim = {};
        }
        tool = viewTool[0] = viewTool[1] = Tool::Particles;
        Json preset;
        const auto presetText = particleBrush.ToJson().Dump(0);
        const bool parsed = Json::Parse(presetText,preset);
        const auto saved = ParticleBrush::FromJson(preset);
        check(parsed && saved.ToJson().Dump(0) == presetText && (!scripted || (saved.program && saved.scriptValues == particleBrush.scriptValues)),
              "preset round-trips its material, script and slider values");
        if (!BeginParticleStroke(v)) { check(false,"start 30 FPS rust brush"); finish(); return; }
        sim.previous = rivet;
        sim.held = true;
        phase = 1;
        return;
    }
    auto& v = *Active();
    auto& d = *v.doc;
    const auto capture = [&](auto& snapshots) {
        for (Id id : channels) snapshots.push_back(ctx->ReadbackTexture(d.GetSurface(id)->tex.get()));
    };
    const auto ready = [](const auto& snapshots) {
        return std::all_of(snapshots.begin(), snapshots.end(), [](const auto& r) { return r && r->ready; });
    };
    const auto same = [](const auto& a, const auto& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) if (a[i]->data != b[i]->data) return false;
        return true;
    };
    if (phase == 1 || phase == 2) {
        const int fps = phase == 1 ? 30 : 120;
        ++frame;
        AdvanceParticles(1.0 / fps, rivet, frame < fps / 2);
        if (sim.playing) return;
        if (phase == 1) {
            input30 = sim.inputs;
            capture(at30);
            FinishParticles(false);
            v.mesh->brushTileBounds = std::move(cachedBounds);
            check(v.mesh->brushTileBounds && v.mesh->brushTileBounds->ready, "world-space UV tile bounds are ready");
            if (!BeginParticleStroke(v)) { check(false,"start 120 FPS rust brush"); finish(); return; }
            sim.previous = rivet;
            sim.held = true;
            frame = 0;
            phase = 2;
        } else {
            capture(at120);
            check(!input30.empty() && sim.inputs.size() == input30.size(),"both frame cadences recorded the same number of source ticks");
            v.mesh->yaw = 2.3f;
            v.mesh->UpdateCamera(1);
            Json saved;
            if (scripted) {
                particleBrush.scriptValues[0] = 0;
                particleBrush.scriptValues[4] = 0;
            }
            const bool parsed = Json::Parse(ParticleGestureJson(v.mesh->data).Dump(0), saved);
            auto wrongMesh = saved;
            wrongMesh["mesh"] = "different geometry";
            check(parsed && !LoadParticleGesture(v,wrongMesh), "a saved gesture checks its mesh signature");
            if (!parsed || !LoadParticleGesture(v,saved)) { check(false,"replay saved rust brush"); finish(); return; }
            if (scripted) {
                const auto& values = saved["brush"]["particleScript"]["values"];
                check(values["impact_radius_scale"].F() == kSavedRadius && values["impact_force"].F() == kSavedForce &&
                      values["bounce_strength"].F() == kSavedBounce && particleBrush.scriptValues[4] == kSavedBounce &&
                      particleBrush.scriptValues[0] == kSavedRadius && particleBrush.scriptValues[1] == kSavedForce &&
                      sim.settings.scriptValues == particleBrush.scriptValues,
                      "the saved gesture restores the slider values used for painting, including hidden bounce strength");
            }
            phase = 3;
        }
        return;
    }
    if (phase == 3 || phase == 4) {
        AdvanceParticles(1.0 / (phase == 3 && replayingAt120 ? 120 : 30), {}, false);
        if (sim.playing) return;
        if (phase == 3) {
            capture(replayingAt120 ? replay120 : afterReplay);
            if (scripted && !replayingAt120) {
                replayingAt120 = true;
                if (!BeginParticleStroke(v,true)) { check(false,"repeat scripted replay at 120 FPS"); finish(); }
                return;
            }
            if (scripted) {
                *PanelFlag(kParticlePanel) = true;
                ShowSideTab(4);
                FocusPanel(kParticlePanel);
                demo::observeItem = [](ImGuiID id, const ImRect& rect) {
                    if (g_editorInput.impactSlider && id == g_editorInput.impactSlider)
                        g_editorInput.impactFrame = ImGui::GetFrameCount();
                    if (g_editorInput.bounceGroup && id == g_editorInput.bounceGroup)
                        g_editorInput.bounceGroupFrame = ImGui::GetFrameCount();
                    if (g_editorInput.bounceSlider && id == g_editorInput.bounceSlider)
                        g_editorInput.bounceFrame = ImGui::GetFrameCount();
                    if (id != g_editorInput.id) return;
                    g_editorInput.rect = rect;
                    g_editorInput.observedFrame = ImGui::GetFrameCount();
                };
                g_editorInput.entered = frameCounter;
                g_editorInput.stage = -5;
                phase = 9;
                return;
            }
            particleBrush.gravity = 0.20f;
            if (!BeginParticleStroke(v,true)) { check(false,"replay adjusted rust brush"); finish(); return; }
            phase = 4;
        } else {
            capture(afterChange);
            FinishParticles(true);
            v.history.Undo(d,*ctx);
            capture(undone);
            v.history.Redo(d,*ctx);
            capture(redone);
            phase = 5;
        }
        return;
    }
    if (phase == 10) {
        if (frameCounter - g_editorInput.entered < 3) return;
        if (!sim.replay || sim.settings.scriptValues != particleBrush.scriptValues) {
            check(false,"Replay uses the new slider values"); finish(); return;
        }
        AdvanceParticles(1.0/30,{},false);
        if (sim.playing) return;
        capture(afterSliders);
        gestureWithSliders = ParticleGestureJson(v.mesh->data);
        g_editorInput.entered = frameCounter;
        phase = 9;
        return;
    }
    if (phase == 11) {
        if (frameCounter - g_editorInput.entered < 3) return;
        if (!sim.replay || sim.settings.scriptValues[2] != 0 || sim.settings.scriptValues[3] != (float)particles::Bounce) {
            check(false,"Replay uses the unchecked deposit control and the bounce option"); finish(); return;
        }
        AdvanceParticles(1.0/30,{},false);
        if (sim.playing) return;
        capture(afterChoice);
        Json gesture;
        const bool parsed = Json::Parse(ParticleGestureJson(v.mesh->data).Dump(0),gesture);
        particleBrush.scriptValues[2] = 1;
        particleBrush.scriptValues[3] = particles::Slide;
        particleBrush.scriptValues[4] = 0;
        const bool restored = parsed && LoadParticleGesture(v,gesture) &&
            particleBrush.scriptValues[2] == 0 && particleBrush.scriptValues[3] == (float)particles::Bounce &&
            particleBrush.scriptValues[4] == g_editorInput.bounceStrength;
        check(restored,"the saved gesture restores the unchecked box, Bounce and the adjusted bounce strength");
        if (!restored) { finish(); return; }
        particleBrush.scriptValues[2] = 1;
        if (!BeginParticleStroke(v,true)) { check(false,"start the bounce gesture with impact deposits"); finish(); return; }
        phase = 12;
        return;
    }
    if (phase == 12) {
        const bool at120FPS = !bounce30.empty();
        AdvanceParticles(at120FPS ? 1.0/120 : 1.0/30,{},false);
        if (sim.playing) return;
        if (!at120FPS) {
            capture(bounce30);
            Json savedBounce;
            const bool parsed = Json::Parse(ParticleGestureJson(v.mesh->data).Dump(0),savedBounce);
            if (!parsed || !LoadParticleGesture(v,savedBounce) || sim.settings.scriptValues[4] != g_editorInput.bounceStrength) {
                check(false,"reload the painted bounce gesture and its adjusted strength"); finish();
            }
            return;
        }
        capture(bounce120);
        if (!LoadParticleGesture(v,gestureWithSliders)) {
            check(false,"restore the slide gesture for the script editing test"); finish(); return;
        }
        g_editorInput.entered = frameCounter;
        phase = 9;
        return;
    }
    if (phase == 9) {
        auto& input = g_editorInput;
        auto* panel = ImGui::FindWindowByName(kParticlePanel);
        auto* popup = ImGui::FindWindowByName(kParticlePopup);
        const ImGuiID simulationTab = panel ? ImHashStr("Simulation", 0, panel->GetID("##paint3dtabs")) : 0;
        const ImGuiID impactGroup = ImHashStr("###script-group-impact",0,simulationTab);
        input.impactSlider = ImHashStr("##script-slider",0,ImHashStr("impact_radius_scale",0,simulationTab));
        input.bounceGroup = ImHashStr("###script-group-bounce",0,simulationTab);
        input.bounceSlider = ImHashStr("##script-slider",0,ImHashStr("bounce_strength",0,simulationTab));
        ImGuiIO& io = ImGui::GetIO();
        if (frameCounter - input.entered > 120) {
            Log("[paint-particles] editor control timed out at step %d", input.stage);
            check(false,"operate the particle editor from the Simulation panel"); finish(); return;
        }
        switch (input.stage) {
            case -5:
                if (input.Click(panel, simulationTab)) input.Next(frameCounter);
                break;
            case -4:
                if (frameCounter - input.entered < 3) break;
                if (input.impactFrame != ImGui::GetFrameCount() - 1) {
                    check(false,"Impact is open on first display"); finish(); return;
                }
                if (input.Click(panel,impactGroup)) input.Next(frameCounter);
                break;
            case -3:
                if (frameCounter - input.entered < 3) break;
                if (input.impactFrame < 0 || input.impactFrame >= ImGui::GetFrameCount() - 1 ||
                    particleBrush.scriptGroupOpen[0] != false) {
                    check(false,"collapsing Impact records its state and hides its controls"); finish(); return;
                }
                input.groupPreset = particleBrush.ToJson().Dump(0);
                particleBrush.scriptGroupOpen = {};
                input.Next(frameCounter);
                break;
            case -2: {
                if (frameCounter - input.entered < 3) break;
                if (input.impactFrame != ImGui::GetFrameCount() - 1) {
                    check(false,"a brush without saved state opens Impact using the script default"); finish(); return;
                }
                Json preset;
                if (!Json::Parse(input.groupPreset,preset)) {
                    check(false,"parse the saved presentation state"); finish(); return;
                }
                particleBrush = ParticleBrush::FromJson(preset);
                input.Next(frameCounter);
                break;
            }
            case -1:
                if (frameCounter - input.entered < 3) break;
                if (input.impactFrame >= ImGui::GetFrameCount() - 1 || particleBrush.scriptGroupOpen[0] != false) {
                    check(false,"reloading the saved brush restores the collapsed Impact header"); finish(); return;
                }
                if (input.Click(panel,impactGroup)) {
                    check(true,"Impact opened by default and restored its collapsed state after brush reload");
                    input.Next(frameCounter);
                }
                break;
            case 0:
                if (frameCounter - input.entered < 3) break;
                if (input.impactFrame != ImGui::GetFrameCount() - 1 || particleBrush.scriptValues[0] != kSavedRadius) {
                    check(false,"expanding Impact restores its visible controls with their previous values"); finish(); return;
                }
                check(true,"Impact collapses and expands in Simulation with its values preserved");
                input.Next(frameCounter);
                break;
            case 1:
                if (input.Click(panel,input.impactSlider,0.3f))
                    input.Next(frameCounter);
                break;
            case 2:
                if (input.Click(panel, ImHashStr("##script-slider",0,ImHashStr("impact_force",0,simulationTab)),0.3f))
                    input.Next(frameCounter);
                break;
            case 3:
                if (particleBrush.scriptValues[0] <= kSavedRadius || particleBrush.scriptValues[1] <= kSavedForce) {
                    Log("[paint-particles] slider values after dragging: radius %.6g, force %.6g",
                        particleBrush.scriptValues[0],particleBrush.scriptValues[1]);
                    check(false,"both impact sliders can be adjusted from Simulation"); finish(); return;
                }
                if (input.Click(panel, ImHashStr("Rejouer le geste",0,simulationTab))) {
                    check(true,"adjusted both sliders in Simulation and clicked Replay");
                    input.Next(frameCounter);
                    phase = 10;
                }
                break;
            case 4:
                if (input.Click(panel,ImHashStr("###script-checkbox",0,ImHashStr("paint_impacts",0,simulationTab))))
                    input.Next(frameCounter);
                break;
            case 5:
                if (input.bounceGroupFrame >= ImGui::GetFrameCount() - 1 || input.bounceFrame >= ImGui::GetFrameCount() - 1) {
                    check(false,"the Rebond group and its slider are hidden while Slide is selected"); finish(); return;
                }
                if (input.Click(panel,ImHashStr("##script-choice",0,ImHashStr("impact_mode",0,simulationTab))))
                    input.Next(frameCounter);
                break;
            case 6:
                if (!GImGui->OpenPopupStack.empty()) {
                    auto* combo = GImGui->OpenPopupStack.back().Window;
                    if (combo && input.Click(combo,ImHashStr("###script-option",0,combo->GetID(1))))
                        input.Next(frameCounter);
                }
                break;
            case 7:
                if (frameCounter - input.entered < 3) break;
                if (particleBrush.scriptValues[2] != 0 || particleBrush.scriptValues[3] != (float)particles::Bounce) {
                    check(false,"the checkbox and dropdown can be edited in Simulation"); finish(); return;
                }
                if (!particleBrush.scriptGroupOpen[1].value_or(false)) {
                    if (input.bounceGroupFrame != ImGui::GetFrameCount() - 1 || input.bounceFrame >= ImGui::GetFrameCount() - 1) {
                        check(false,"Rebond appears collapsed by default after selecting Bounce"); finish(); return;
                    }
                    if (input.Click(panel,input.bounceGroup)) input.entered = frameCounter;
                    break;
                }
                if (input.Click(panel,input.bounceSlider,0.35f)) input.Next(frameCounter);
                break;
            case 8:
                if (frameCounter - input.entered < 3) break;
                input.bounceStrength = particleBrush.scriptValues[4];
                if (input.bounceStrength <= kSavedBounce) {
                    check(false,"the visible bounce slider can be adjusted in Simulation"); finish(); return;
                }
                check(true,"Rebond starts collapsed and opens when clicked");
                particleBrush.scriptValues[3] = particles::Slide;
                input.Next(frameCounter);
                break;
            case 9: {
                if (frameCounter - input.entered < 3) break;
                Json preset;
                const bool parsed = Json::Parse(particleBrush.ToJson().Dump(0),preset);
                const auto saved = ParticleBrush::FromJson(preset);
                check(parsed && input.bounceGroupFrame < ImGui::GetFrameCount() - 1 && input.bounceFrame < ImGui::GetFrameCount() - 1 &&
                      particleBrush.scriptValues[4] == input.bounceStrength && saved.scriptValues[4] == input.bounceStrength &&
                      saved.scriptGroupOpen[1] == true,
                      "saving while Rebond is hidden preserves both its adjusted value and its open state");
                particleBrush = saved;
                particleBrush.scriptValues[3] = particles::Bounce;
                input.Next(frameCounter);
                break;
            }
            case 10:
                if (frameCounter - input.entered < 3) break;
                if (input.bounceGroupFrame != ImGui::GetFrameCount() - 1 || input.bounceFrame != ImGui::GetFrameCount() - 1 ||
                    particleBrush.scriptValues[4] != input.bounceStrength) {
                    check(false,"selecting Bounce again shows the group and its previous slider value"); finish(); return;
                }
                if (input.Click(panel,ImHashStr("Rejouer le geste",0,simulationTab))) {
                    check(true,"Bounce shows the Rebond group again and Simulation replays its adjusted value");
                    input.Next(frameCounter);
                    phase = 11;
                }
                break;
            case 11:
                if (input.Click(panel, ImHashStr("Éditer le script…", 0, simulationTab))) input.Next(frameCounter);
                break;
            case 12: {
                if (input.expected.empty()) {
                    input.expected = particleBrush.script;
                    const size_t impact = input.expected.find("    strength = clamp(");
                    const size_t endOfLine = input.expected.find('\n', impact);
                    if (endOfLine == std::string::npos) {
                        check(false,"example has a strength assignment to edit"); finish(); return;
                    }
                    input.editLine = std::count(input.expected.begin(), input.expected.begin() + endOfLine, '\n');
                    input.expected.insert(endOfLine, kImpactEdit);
                    input.source = &particleCode.text;
                    // Position the real editor caret without depending on theme, scale or font metrics.
                    demo::observeCodeEditor = [](const std::string* owner, TextEditor& editor) {
                        auto& edit = g_editorInput;
                        if (owner != edit.source || edit.caretReady) return;
                        editor.SetCursor({edit.editLine, editor.GetLineText(edit.editLine).size()});
                        edit.caretReady = true;
                    };
                }
                if (input.caretReady && popup && GImGui->NavWindow && GImGui->NavWindow->ParentWindow == popup) {
                    demo::observeCodeEditor = nullptr;
                    input.Next(frameCounter);
                }
                break;
            }
            case 13:
                if (frameCounter - input.entered >= 2) {
                    io.AddInputCharactersUTF8(kImpactEdit);
                    input.Next(frameCounter);
                }
                break;
            case 14:
                if (frameCounter - input.entered < 3) break;
                if (particleCode.text != input.expected) {
                    Log("[paint-particles] unexpected edited source (%zu bytes), focus %s: %s", particleCode.text.size(),
                        GImGui->NavWindow ? GImGui->NavWindow->Name : "-", particleCode.text.c_str());
                    check(false,"code editor changed the impact hook of the example"); finish(); return;
                }
                if (popup && input.Click(popup, popup->GetID("Appliquer le script"))) input.Next(frameCounter);
                break;
            case 15:
                if (frameCounter - input.entered < 3) break;
                if (!particleBrush.program || particleBrush.script != input.expected) {
                    check(false,"Apply installs the edited script in the running application"); finish(); return;
                }
                if (input.Click(panel, ImHashStr("Rejouer le geste", 0, simulationTab))) {
                    check(true,"edited the example and applied it in the Shaderixel particle editor");
                    input.Next(frameCounter);
                }
                break;
            case 16:
                if (frameCounter - input.entered < 3) break;
                if (!sim.replay || !sim.playing || sim.settings.program != particleBrush.program) {
                    check(false,"Replay uses the script just applied in the editor"); finish(); return;
                }
                check(true,"clicked Replay and used the edited script without restarting");
                phase = 4;
                break;
        }
        return;
    }
    if (phase == 13) {
        auto& input = g_editorInput;
        auto* panel = ImGui::FindWindowByName(kParticlePanel);
        const ImGuiID simulationTab = panel ? ImHashStr("Simulation",0,panel->GetID("##paint3dtabs")) : 0;
        if (frameCounter - input.entered < 3) return;
        if (frameCounter - input.entered > 120) {
            check(false,"use the group reset buttons in Simulation"); finish(); return;
        }
        const int lastFrame = ImGui::GetFrameCount() - 1;
        switch (input.stage) {
            case 0:
                if (input.Click(panel,ImHashStr("###script-group-reset-bounce",0,simulationTab))) input.Next(frameCounter);
                break;
            case 1:
                check(particleBrush.scriptValues[4] == 0.35f && !particleBrush.scriptGroupOpen[1].has_value() &&
                      input.bounceGroupFrame == lastFrame && input.bounceFrame < lastFrame,
                      "Reset on Rebond restores bounce strength and collapses the group");
                check(particleBrush.scriptValues[0] == kSavedRadius && particleBrush.scriptValues[1] == kSavedForce &&
                      particleBrush.scriptValues[2] == 0 && particleBrush.scriptValues[3] == (float)particles::Bounce &&
                      particleBrush.scriptGroupOpen[0] == false && input.impactFrame < lastFrame,
                      "Reset on Rebond preserves Impact values and its collapsed state");
                particleBrush.scriptValues[4] = kSavedBounce;
                particleBrush.scriptGroupOpen[1] = true;
                input.Next(frameCounter);
                break;
            case 2:
                if (input.Click(panel,ImHashStr("###script-group-reset-impact",0,simulationTab))) input.Next(frameCounter);
                break;
            case 3:
                check(particleBrush.scriptValues[0] == 2.1f && particleBrush.scriptValues[1] == 1.f &&
                      particleBrush.scriptValues[2] == 1.f && particleBrush.scriptValues[3] == (float)particles::Slide &&
                      !particleBrush.scriptGroupOpen[0].has_value() && input.impactFrame == lastFrame,
                      "Reset on collapsed Impact restores all four controls and opens the group");
                check(particleBrush.scriptValues[4] == kSavedBounce && particleBrush.scriptGroupOpen[1] == true &&
                      input.bounceGroupFrame < lastFrame,
                      "Reset on Impact hides Rebond through the default reaction and preserves its value and open state");
                particleBrush.scriptValues = {kSavedRadius,kSavedForce,0.f,(float)particles::Stop,kSavedBounce};
                particleBrush.scriptGroupOpen[0] = false;
                input.Next(frameCounter);
                break;
            case 4:
                if (input.impactFrame >= lastFrame || input.bounceGroupFrame >= lastFrame) {
                    check(false,"Reset all starts with Impact collapsed and Rebond hidden"); finish(); return;
                }
                if (input.Click(panel,ImHashStr("###script-reset-all",0,simulationTab))) input.Next(frameCounter);
                break;
            case 5:
                check(particleBrush.scriptValues[0] == 2.1f && particleBrush.scriptValues[1] == 1.f &&
                      particleBrush.scriptValues[2] == 1.f && particleBrush.scriptValues[3] == (float)particles::Slide &&
                      particleBrush.scriptValues[4] == 0.35f && !particleBrush.scriptGroupOpen[0].has_value() &&
                      !particleBrush.scriptGroupOpen[1].has_value() && input.impactFrame == lastFrame &&
                      input.bounceGroupFrame < lastFrame,
                      "Reset all restores every value and group default, including hidden Rebond");
                particleBrush.scriptValues[3] = particles::Bounce;
                input.Next(frameCounter);
                break;
            case 6:
                check(input.bounceGroupFrame == lastFrame && input.bounceFrame < lastFrame &&
                      particleBrush.scriptValues[4] == 0.35f,
                      "Rebond reappears collapsed with its default strength after Reset all");
                finish();
                break;
        }
        return;
    }
    if (phase == 5 && ready(before) && ready(at30) && ready(at120) && ready(afterReplay) && ready(replay120) &&
        ready(afterSliders) && ready(afterChoice) && ready(bounce30) && ready(bounce120) && ready(afterChange) &&
        ready(undone) && ready(redone)) {
        check(same(at30,at120),"all three RGBA16F textures match at 30 and 120 FPS, with and without tile culling");
        check(same(at30,afterReplay),"orbiting the model and replaying preserves every pixel");
        if (scripted) {
            check(same(afterReplay,replay120),"saved slider settings replay byte-for-byte at 30 and 120 FPS");
            check(!same(at30,afterSliders),"changing only the two sliders changes the replayed paint");
            check(!same(afterSliders,afterChoice) && same(before,afterChoice),
                  "bouncing with impact deposits unchecked leaves the model's textures untouched");
            check(!same(before,bounce30) && same(bounce30,bounce120),
                  "the saved Bounce gesture with adjusted strength paints identical pixels at 30 and 120 FPS");
            size_t changedPixels = 0;
            for (size_t c = 0; c < channels.size(); ++c)
                for (size_t pixel = 0; pixel < at30[c]->data.size(); pixel += 8)
                    changedPixels += memcmp(at30[c]->data.data() + pixel, afterChange[c]->data.data() + pixel, 8) != 0;
            Log("[paint-particles] %zu RGBA16F pixels changed across the three channels after editing and replaying", changedPixels);
            check(changedPixels > 0,"editing the script changes the saved gesture's paint");
        } else check(!same(at30,afterChange),"changing gravity reroutes the saved gesture");
        check(same(before,undone) && same(afterChange,redone),"freezing yields one undo/redo operation for all channels");
        bool protectedPixels = true, acrossSeam = true;
        for (size_t c = 0; c < channels.size(); ++c) {
            bool top = false, bottom = false;
            for (int y = 0; y < size; ++y) {
                const size_t offset = (size_t)y*size*8;
                if (memcmp(before[c]->data.data()+offset,at30[c]->data.data()+offset,size*8) == 0) continue;
                if (y >= 166 && y < 178) protectedPixels = false;
                if (y < 110) top = true;
                if (y > 178) bottom = true;
            }
            acrossSeam &= top && bottom;
        }
        check(acrossSeam,"all three channels mark both sides of the UV seam");
        check(protectedPixels,"all pixels of the protected strip are untouched");
        if (scripted) {
            SetParticleBrush(SimPresetCount()-1);
            particleBrush.scriptValues = {kSavedRadius,kSavedForce,0.f,(float)particles::Bounce,kSavedBounce};
            particleBrush.scriptGroupOpen[0] = false;
            particleBrush.scriptGroupOpen[1] = true;
            g_editorInput.stage = 0;
            g_editorInput.entered = frameCounter;
            phase = 13;
            return;
        }
        v.history.Undo(d,*ctx);
        SetParticleBrush(6);
        particleBrush.source = 1;
        const auto selection = gpu::Dev().CreateTexture(size,size,gpu::Format::R8,"EmitterSelection");
        std::vector<u8> coverage(size*size);
        for (int y = 34; y < 54; ++y) std::fill_n(coverage.begin()+y*size+120,16,255);
        ctx->Upload(selection.get(),coverage.data(),size);
        v.renderer.SelectFrom(d,*ctx,v.history,selection.get(),SelSource::Mask,SelMode::Replace,{},0,"Point source");
        if (!BeginParticleStroke(v)) { check(false,"start selection source"); finish(); return; }
        ParticleSelectionOrigins(v);
        if (!sim.sourceMask) { check(false,"selection source scheduled its GPU readback"); finish(); return; }
        phase = 6;
        return;
    }
    if (phase == 6) {
        if (sim.sourceMask) UpdateSimulation(v,false,false,{},0,0);
        else AdvanceParticles(1.0/30, {}, false);
        if (sim.playing || sim.sourceMask) return;
        check(!sim.origins.empty() && std::all_of(sim.origins.begin(),sim.origins.end(),[](const ParticleOrigin& o) {
            return std::abs(o.point.x) < 0.045f && o.point.y > 0.50f && o.point.y < 0.76f;
        }), "UV selection yields emitter positions in the selected patch on the mesh");
        capture(fromSelection);
        FinishParticles(false);
        v.renderer.Deselect(d,v.history);
        const Id layer = d.S().activeLayer;
        v.particlePathSource = d.AddPixelLayer("Emitter path").id;
        d.S().activeLayer = layer;
        Json path = Json::Object();
        for (float weight : {0.2f,0.7f}) {
            Json source = Json::Array();
            source.Push(0);
            source.Push(0.8f-weight);
            source.Push(weight);
            source.Push(0.2f);
            path["points"].Push(std::move(source));
        }
        d.FindLayer(v.particlePathSource)->surfacePath = path.Dump(0);
        particleBrush.source = 2;
        particleBrush.wet = true;
        fluidSet.simRes = 3;
        if (!BeginParticleStroke(v) || !ParticlePathOrigins(v,sim.origins)) {
            check(false,"start wet particles from a surface path"); finish(); return;
        }
        check(sim.origins.size()>2 && Length(sim.origins.front().point-sim.origins.back().point)>0.4f,
              "particle sources sample the surface between path anchors");
        AdvanceParticles(1.0/30, sim.origins.front(), false);
        phase = 7;
    } else if (phase == 7) {
        if (sim.playing) {
            AdvanceParticles(1.0/30, {}, false);
            return;
        }
        check(fluidRun.active && tool == Tool::Fluid, "path impacts hand off to the wet paint tool");
        FluidFinish(true);
        capture(fromWetPath);
        v.history.Undo(d,*ctx);
        before.clear();
        capture(before);
        phase = 8;
    } else if (phase == 8 && ready(before) && ready(fromSelection) && ready(fromWetPath)) {
        check(!same(before,fromSelection),"particles emitted from the selection produce visible paint");
        check(same(before,undone),"undo of the liquid handoff restores the original paint");
        check(!same(before,fromWetPath),"fixing the liquid handoff commits the impact deposits");
        finish();
    }
}

// Run one warm-up stroke followed by three measured strokes at a controlled 60 Hz simulation cadence.
// Frame intervals are wall-clock measurements, including real GPU submission and viewport rendering.
void App::DemoParticlePerformance(int preset) {
    using Clock = std::chrono::steady_clock;
    constexpr int resolution = 4096, inputFrames = 60, runs = 4;
    constexpr PbrChannel channels[] = {PbrChannel::BaseColor,PbrChannel::Roughness,PbrChannel::Height};
    static const bool multipleTiles = getenv("SX_PARTICLE_UDIM4K") != nullptr;
    static const char* reference = getenv("SX_PARTICLE_REFERENCE");
    static int atlasSize = resolution;
    static int phase = 0, run = 0, frame = 0;
    static u64 nextFrame = 0;
    static Id material = 0, roughness = 0;
    static u64 peak = 0, idleMemory = 0;
    static Clock::time_point previous, started, frozen;
    static std::vector<double> frameTimes, paintingTimes, updateTimes;
    static bool previousPainted = false;
    static gpu::ReadbackPtr completed;
    static gpu::ReadbackPtr painted;
    static std::vector<std::pair<std::string,Rect>> parts;
    static size_t readIndex = 0;
    const auto finish = [&](bool pass, const char* message) {
        Log("[particle-4k] %s %s", pass ? "PASS" : "FAIL", message);
        Log("[particle-4k] COMPLETE %d failures", pass ? 0 : 1);
        if (!pass) ++uiErrorCount;
        autoQuit = true;
    };
    if (frameCounter < 4 || autoQuit) return;
    if (frameCounter >= 2600) { finish(false,"4K benchmark timed out"); return; }
    if (!phase) {
        if (preset < 6 || preset >= SimPresetCount()) { finish(false,"unknown material preset"); return; }
        for (const char* name : {"mesh_brush.slang","mesh_stroke_tiles.slang","clear_r.slang"}) Kernel(name);
        auto mesh = multipleTiles ? DenseParticlePanel() : RivetPanel();
        if (multipleTiles) {
            std::string error;
            if (!PackUdims(mesh) || !SizeTextureTiles(mesh,resolution,"",atlasSize,error)) {
                finish(false,"cannot pack the native UDIMs");
                return;
            }
        } else {
            const float3 normal = Normalize({0,1,-1});
            for (auto& vertex : mesh.vertices) {
                vertex.pos.z = vertex.pos.y;
                vertex.normal = normal;
            }
            mesh.boundsMin.z = mesh.boundsMin.y;
            mesh.boundsMax.z = mesh.boundsMax.y;
            mesh.name = "Sloped panel with two UV islands";
        }
        NewDocument(atlasSize,atlasSize,Precision::Half16,DocMode::Layers,0,"Particle paint 4K");
        auto& v = *Active();
        auto& d = *v.doc;
        d.kind = 1;
        d.pbrChannels = 0;
        for (const auto channel : channels) d.pbrChannels |= 1u << (int)channel;
        AttachMesh(v,std::move(mesh));
        if (multipleTiles) {
            parts = ExportParts(v);
            if (parts.size() != 4 || !std::all_of(parts.begin(),parts.end(),[](const auto& part) {
                return part.second.Width() == resolution && part.second.Height() == resolution;
            })) { finish(false,"the four UDIMs are not native 4K"); return; }
        } else {
            parts.push_back({"", {resolution/4,0,3*resolution/4,resolution}});
        }
        CreatePbrChannels(v,true);
        material = AddPaintLayer3D(v,"Particle benchmark");
        v.renderer.Prepare(d,*ctx);
        const auto* layer = MaterialChannelLayer(d,material,PbrChannel::Roughness);
        if (!material || !layer || !v.renderer.MeshPrepare(d,*ctx,*v.mesh)) { finish(false,"cannot set up the 4K panel"); return; }
        roughness = layer->surface;
        SetParticleBrush(preset);
        const auto recipe = particleBrush.ToJson();
        if (ParticleBrush::FromJson(recipe).ToJson().Dump(0) != recipe.Dump(0)) {
            finish(false,"particle material settings did not round-trip");
            return;
        }
        tool = viewTool[0] = viewTool[1] = Tool::Particles;
        Log("[particle-4k] preset=%d, %zu native 4K set(s), %ux%u RGBA16F atlas, %zu triangles, 60 input frames, 3 timings after warm-up",
            preset,parts.size(),d.Width(),d.Height(),v.mesh->data.indices.size()/3);
        phase = 1;
        nextFrame = frameCounter + 12;
        return;
    }
    auto& v = *Active();
    auto& d = *v.doc;
    const auto origin = [](int i) {
        const float t = std::clamp((float)i / inputFrames, 0.0f, 1.0f);
        float3 p{-0.19f + 0.38f*t,0.65f,0.65f};
        float3 n = Normalize({0,1,-1});
        if (multipleTiles) {
            // A continuous sweep that touches all four UDIMs, even if particles stick at impact.
            p.x = 0.42f - 0.84f*(1-std::clamp(3*t,0.f,1.f) + std::clamp(3*t-2,0.f,1.f));
            p.y = 0.65f - 1.25f*std::clamp(3*t-1,0.f,1.f);
            const float wx = p.x * 28, wy = p.y * 21;
            p.z = p.y + 0.004f * std::sin(wx) * std::sin(wy);
            n = Normalize({0.004f*28*std::cos(wx)*std::sin(wy),1+0.004f*21*std::sin(wx)*std::cos(wy),-1});
        }
        return ParticleOrigin{p,n,p+n*2,n*-1,0.04f,true};
    };
    const auto milliseconds = [](auto interval) { return std::chrono::duration<double,std::milli>(interval).count(); };
    if (phase == 1 && frameCounter >= nextFrame && v.mesh->brushTileBounds && v.mesh->brushTileBounds->ready) {
        frame = 0;
        previousPainted = false;
        idleMemory = peak = gpu::Dev().VideoMemoryUsage();
        frameTimes.clear();
        paintingTimes.clear();
        updateTimes.clear();
        if (!BeginParticleStroke(v)) { finish(false,"preset could not start on a 4K layer"); return; }
        const auto expectedChannels = (size_t)std::popcount(particleBrush.channels);
        if (sim.targets.size() != expectedChannels) { finish(false,"preset did not select the expected 4K channels"); return; }
        sim.previous = origin(0);
        sim.held = true;
        started = previous = Clock::now();
        phase = 2;
        return;
    }
    if (phase == 2) {
        const auto now = Clock::now();
        const auto interval = milliseconds(now-previous);
        if (frame > 0 && frame <= inputFrames) frameTimes.push_back(interval);
        if (previousPainted) paintingTimes.push_back(interval);
        previous = now;
        ++frame;
        const auto revision = d.GetSurface(roughness)->version;
        const auto startUpdate = Clock::now();
        AdvanceParticles(1.0/60,origin(frame),frame<inputFrames);
        previousPainted = d.GetSurface(roughness)->version != revision;
        if (previousPainted) updateTimes.push_back(milliseconds(Clock::now()-startUpdate));
        peak = std::max(peak, gpu::Dev().VideoMemoryUsage());
        if (sim.playing) return;
        frozen = Clock::now();
        FinishParticles(true);
        completed = v.renderer.ReadPixel(*ctx,d.GetSurface(roughness)->tex.get(),atlasSize/2,atlasSize/2);
        if (!completed) { finish(false,"cannot fence the 4K stroke"); return; }
        phase = 3;
        return;
    }
    if (phase == 3 && completed->ready) {
        const auto elapsed = milliseconds(Clock::now()-started);
        const auto freeze = milliseconds(Clock::now()-frozen);
        v.history.Maintain(*ctx);
        peak = std::max(peak, gpu::Dev().VideoMemoryUsage());
        const auto percentile = [](auto& values, double p) {
            std::sort(values.begin(),values.end());
            return values.empty() ? 0.0 : values[(size_t)std::ceil(p*values.size())-1];
        };
        if (run) Log("[particle-4k] run=%d frame_p50_ms=%.3f frame_p95_ms=%.3f frame_max_ms=%.3f paint_frames=%zu paint_p95_ms=%.3f paint_max_ms=%.3f cpu_p95_ms=%.3f elapsed_ms=%.1f freeze_ms=%.1f peak_mib=%.1f idle_mib=%.1f undo_mib=%.2f",
                     run,percentile(frameTimes,0.50),percentile(frameTimes,0.95),percentile(frameTimes,1),
                     paintingTimes.size(),percentile(paintingTimes,0.95),percentile(paintingTimes,1),percentile(updateTimes,0.95),
                     elapsed,freeze,peak/1048576.,idleMemory/1048576.,v.history.MemoryBytes()/1048576.);
        if (!v.history.CanUndo() || paintingTimes.empty()) { finish(false,"4K stroke created no recorded paint"); return; }
        if (++run < runs) {
            v.history.Undo(d,*ctx);
            v.history.Clear();
            nextFrame = frameCounter + 5;
            phase = 1;
            return;
        }
        phase = 4;
        return;
    }
    if (phase == 4) {
        const auto channel = channels[readIndex % std::size(channels)];
        const auto& [suffix,rect] = parts[readIndex / std::size(channels)];
        if (!painted) {
            const auto* layer = MaterialChannelLayer(d,material,channel);
            const auto* surface = layer ? d.GetSurface(layer->surface) : nullptr;
            if (surface && surface->tex && surface->tex->Width() == atlasSize && surface->tex->Height() == atlasSize)
                painted = ctx->ReadbackTexture(surface->tex.get(),&rect);
            if (!painted) finish(false,"4K channel readback failed");
            return;
        }
        if (painted->ready) {
            const auto& rb = *painted;
            if (rb.format != gpu::Format::RGBA16F || rb.data.size() != (size_t)rb.width*rb.height*8) {
                finish(false,"unexpected 4K readback layout");
                return;
            }
            size_t marked = 0, materialMatches = 0;
            const auto c = (size_t)channel;
            const bool expected = (particleBrush.channels & (1u << c)) != 0;
            const float4 trail = LinearColor(particleBrush.colors[c]), impact = LinearColor(particleBrush.impactColors[c]);
            for (size_t at = 0; at < rb.data.size(); at += 8) {
                if ((rb.data[at+6] | rb.data[at+7]) == 0) continue;
                ++marked;
                u16 rgba[4];
                memcpy(rgba,&rb.data[at],sizeof(rgba));
                const float alpha = HalfToFloat(rgba[3]);
                if (alpha < 0.03f) continue;
                bool matches = true;
                for (int k = 0; k < 3; ++k) {
                    const float component = HalfToFloat(rgba[k]) / alpha;
                    matches &= component >= std::min(trail[k],impact[k])-0.004f && component <= std::max(trail[k],impact[k])+0.004f;
                }
                materialMatches += matches;
            }
            Log("[particle-4k] tile=%s channel=%s painted_texels=%zu material_matches=%zu expected=%d",
                suffix.c_str(),PbrChannelSuffix(channel),marked,materialMatches,(int)expected);
            if (expected ? materialMatches <= 100 : marked != 0) { finish(false,"unexpected painted channel result"); return; }
            if (reference) {
                const std::string file = std::string(PbrChannelSuffix(channel)) + suffix + ".rgba16f";
                if (!ParticleReferencePixels(file,rb.data,std::string_view(reference) == "write")) {
                    finish(false,"pixel reference mismatch or I/O error");
                    return;
                }
            }
            painted.reset();
            if (++readIndex == parts.size()*std::size(channels))
                finish(true,"RGBA16F pixels match the enabled channels and any requested reference");
        }
    }
}
} // namespace sx
