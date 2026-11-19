#include "ui/App.h"
#include "render/Dispatch.h"

namespace sx {

void App::EndSurfaceTool(DocView& v) {
    auto& t = v.surfaceTools;
    if (t.stroke == Tool::SurfaceMask && t.coverage)
        v.renderer.SelectFrom(*v.doc, *ctx, v.history, t.coverage.get(), SelSource::Mask, SelMode::Replace, {}, 0, "Masque rapide 3D");
    else if (t.stroke == Tool::Clone || t.stroke == Tool::Smudge)
        v.renderer.EndStrokeMulti(*v.doc, *ctx, v.history, t.stroke == Tool::Clone ? "Tampon de matière" : "Doigt 3D");
    t.stroke = Tool::Count;
    t.coverage.reset();
    t.merged.clear();
    t.targets.clear();
}

void App::SurfaceToolInput(DocView& v, float2 pos, u32 w, u32 h, bool hovered) {
    auto& t = v.surfaceTools;
    const bool usingTool = tool == Tool::Clone || tool == Tool::Smudge || tool == Tool::SurfaceMask;
    t.lastFrame = ImGui::GetFrameCount();
    if (t.stroke != Tool::Count && t.stroke != tool) EndSurfaceTool(v);
    if (!usingTool || !v.mesh) return;
    Document& d = *v.doc;
    MeshScene& s = *v.mesh;
    const auto& io = ImGui::GetIO();
    const bool start = hovered && !io.KeyAlt && ImGui::IsMouseClicked(0);
    if (start || t.stroke != Tool::Count) {
        gpu::Texture* view = v.renderer.SurfaceUvView(*ctx, s, w, h);
        if (!view) return;
        if (start) {
            u32 tri;
            float3 bary;
            if (!PickMeshHit(s, pos, w, h, tri, bary)) return;
            if (t.stroke != Tool::Count) EndSurfaceTool(v);
            if (tool == Tool::Clone && io.KeyCtrl) {
                const auto& m = s.data;
                t.source = m.vertices[m.indices[tri * 3]].pos * bary.x + m.vertices[m.indices[tri * 3 + 1]].pos * bary.y +
                           m.vertices[m.indices[tri * 3 + 2]].pos * bary.z;
                t.sourceRevision = s.revision;
                t.hasSource = true;
                Status("Source du tampon placée");
                return;
            }
            if (tool == Tool::SurfaceMask) {
                t.uncover = io.KeyCtrl || (platform::PenActive() && platform::LastPenSample().eraser);
                auto* sel = d.Selection();
                if (!sel && t.uncover) return;
                t.coverage = gpu::Dev().CreateTexture(d.Width(), d.Height(), gpu::Format::R8, "QuickMaskStroke");
                if (!t.coverage) return;
                if (sel)
                    ctx->CopyTexture(t.coverage.get(), sel->tex.get());
                else {
                    gpu::DispatchDesc clear;
                    clear.pipeline = Kernel("clear_r.slang");
                    if (!clear.pipeline) {
                        t.coverage.reset();
                        return;
                    }
                    struct {
                        u32 w, h;
                        float value;
                    } root{d.Width(), d.Height(), 1};
                    SetConstants(clear, root);
                    clear.uav[0] = t.coverage.get();
                    Dispatch2D(*ctx, clear, d.Width(), d.Height());
                }
            } else {
                std::vector<float4> colors;
                t.targets = Paint3DTargets(v, colors);
                if (t.targets.empty()) {
                    Status("Choisissez un canal et un calque de peinture");
                    return;
                }
                if (tool == Tool::Clone) {
                    if (!t.hasSource || t.sourceRevision != s.revision) {
                        Status("Ctrl+clic sur le modèle pour placer la source du tampon");
                        return;
                    }
                    const float4 c = s.viewProj.Transform({t.source.x, t.source.y, t.source.z, 1});
                    if (c.w <= 0) return;
                    const float2 source{(c.x / c.w * 0.5f + 0.5f) * w, (0.5f - c.y / c.w * 0.5f) * h};
                    if (source.x < 0 || source.y < 0 || source.x >= w || source.y >= h) {
                        Status("Replacez la source dans la vue");
                        return;
                    }
                    u32 sourceTri;
                    float3 weights;
                    if (!PickMeshHit(s, source, w, h, sourceTri, weights)) return;
                    const auto& m = s.data;
                    float3 visible = m.vertices[m.indices[sourceTri * 3]].pos * weights.x +
                                     m.vertices[m.indices[sourceTri * 3 + 1]].pos * weights.y +
                                     m.vertices[m.indices[sourceTri * 3 + 2]].pos * weights.z;
                    if (Length(visible - t.source) > 0.01f) {
                        Status("La source du tampon est masquée par le modèle");
                        return;
                    }
                    t.cloneOffset = source - pos;
                    t.merged.clear();
                    if (cloneVisible3D && !d.S().editMask) {
                        auto* documentComposite = v.renderer.EvaluateComplete(d, *ctx);
                        for (Id id : t.targets) {
                            gpu::Texture* visibleTex = nullptr;
                            if (auto* active = d.ActiveLayer(); active && active->material) {
                                for (int i = 0; i < (int)PbrChannel::Count; ++i) {
                                    auto ch = (PbrChannel)i;
                                    auto* layer = MaterialChannelLayer(d, active->material, ch);
                                    if (layer && layer->surface == id) {
                                        visibleTex = ChannelTexture(v, ch);
                                        break;
                                    }
                                }
                            } else
                                visibleTex = documentComposite;
                            gpu::TexturePtr copy;
                            if (visibleTex) {
                                copy = gpu::Dev().CreateTexture(d.Width(), d.Height(), visibleTex->GetFormat(), "CloneVisibleChannel");
                                if (!copy) {
                                    t.merged.clear();
                                    return;
                                }
                                ctx->CopyTexture(copy.get(), visibleTex);
                            }
                            t.merged.push_back(copy);
                        }
                    }
                }
                if (!v.renderer.BeginStrokeMulti(d, *ctx, t.targets)) return;
            }
            t.stroke = tool;
            t.last = pos;
        }
        if (t.stroke != Tool::Count) {
            const float2 delta = pos - t.last;
            BrushSettings b = (t.stroke == Tool::SurfaceMask ? brush : retouch);
            const float pressure = platform::PenActive() ? PenPressure(platform::LastPressure()) : 1;
            if (b.pressureSize) b.size *= std::max(pressure, 0.05f);
            if (b.pressureOpacity) b.flow *= pressure;
            if (t.stroke == Tool::SurfaceMask && (start || Length(delta) > 0))
                v.renderer.BrushSurfaceMask(d, *ctx, s, view, t.coverage.get(), t.last, pos, b, t.uncover);
            if (t.stroke == Tool::Clone && (start || Length(delta) > 0))
                v.renderer.RetouchSurface(d, *ctx, s, view, t.last, pos, t.cloneOffset, b, 1, false, t.merged);
            if (t.stroke == Tool::Smudge && Length(delta) > 0) {
                // Separate sweeps move the freshly blended material through faster pointer motion.
                const int n = std::clamp((int)std::ceil(Length(delta) / std::max(b.size * 0.45f, 2.0f)), 1, 8);
                float2 step = delta / (float)n;
                for (int i = 0; i < n; ++i)
                    v.renderer.RetouchSurface(d, *ctx, s, view, t.last + step * (float)i, t.last + step * (float)(i + 1), step * -1, b,
                                              retouchStrength, true, {});
            }
            t.last = pos;
            if (!ImGui::IsMouseDown(0)) EndSurfaceTool(v);
        }
    }
}
} // namespace sx
