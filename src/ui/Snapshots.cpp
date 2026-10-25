// Document snapshots (History panel, like Photoshop's): the whole document (layers, settings and a copy of every
// surface's pixels) kept under a name; restoring one is an ordinary undo step, available even after the steps
// that led to it left the history.
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_stdlib.h>

namespace sx {

namespace {
u64 TexBytes(const gpu::TexturePtr& t) {
    if (!t) return 0;
    const auto f = t->GetFormat();
    const u64 bpp = f == gpu::Format::RGBA32F ? 16 : f == gpu::Format::RGBA16F || f == gpu::Format::RGBA16 ? 8 : f == gpu::Format::R8 ? 1 : 4;
    return (u64)t->Width() * t->Height() * bpp;
}
}  // namespace

void App::TakeDocSnapshot(DocView& v) {
    Document& d = *v.doc;
    if (xf.active && xf.view == &v) TransformCommit(v);
    Prepare(v);  // (queued surface copies / clears done first)
    if (fluidRun.active && fluidRun.view == &v) FluidFinish(true);  // (no half-done wet stroke)
    DocSnapshot s;
    s.state = d.S();
    s.name = "Instantané " + std::to_string(++v.snapshotCounter);
    s.icc = d.iccProfile;
    s.cmyk = d.cmykProfile;
    s.colorMode = d.colorMode;
    for (auto& [id, surf] : s.state.surfaces) {
        if (!surf.tex) continue;
        s.live[id] = {surf.tex.get(), surf.version};
        auto copy = gpu::Dev().CreateTexture(surf.tex->Width(), surf.tex->Height(), surf.tex->GetFormat(), "DocSnapshot");
        if (!copy) { Status("Instantané impossible : mémoire graphique insuffisante"); return; }
        ctx->CopyTexture(copy.get(), surf.tex.get());
        surf.tex = copy;  // (the live surfaces change in place: the snapshot keeps its own pixels)
        s.bytes += TexBytes(copy);
    }
    v.docSnapshots.push_back(std::move(s));
    Status("Instantané créé : " + v.docSnapshots.back().name);
}

void App::RestoreDocSnapshot(DocView& v, size_t index) {
    if (index >= v.docSnapshots.size()) return;
    Document& d = *v.doc;
    if (xf.active && xf.view == &v) TransformCancel(v);
    if (fluidRun.active && fluidRun.view == &v) FluidFinish(true);
    const DocSnapshot& s = v.docSnapshots[index];
    DocState before = d.S();
    DocState st = s.state;
    // Fresh copies (painting after the restore must not change the snapshot), except for the surfaces left as they
    // were since the snapshot: those keep their current pixels (no copy, no memory).
    for (auto& [id, surf] : st.surfaces) {
        if (!surf.tex) continue;
        auto cur = before.surfaces.find(id);
        auto was = s.live.find(id);
        if (cur != before.surfaces.end() && was != s.live.end() && cur->second.tex.get() == was->second.first && cur->second.version == was->second.second) {
            surf = cur->second;
            continue;
        }
        auto copy = gpu::Dev().CreateTexture(surf.tex->Width(), surf.tex->Height(), surf.tex->GetFormat(), "Surface");
        if (!copy) { Status("Rétablir l'instantané : mémoire graphique insuffisante"); return; }
        ctx->CopyTexture(copy.get(), surf.tex.get());
        surf.tex = copy;
        ++surf.version;
    }
    const u32 w = d.Width(), h = d.Height();
    d.S() = std::move(st);
    d.InvalidateSurfaces();
    d.SyncGraph();
    v.history.BeginGroup();
    v.history.PushState("Rétablir « " + s.name + " »", before, d.S());
    if (d.iccProfile != s.icc || d.cmykProfile != s.cmyk || d.colorMode != s.colorMode) {
        // The color settings live outside the state: their own step, undone together.
        History::ColorSettings cb{d.iccProfile, d.cmykProfile, d.colorMode}, ca{s.icc, s.cmyk, s.colorMode};
        d.iccProfile = s.icc;
        d.cmykProfile = s.cmyk;
        d.colorMode = s.colorMode;
        v.history.PushColorSettings("Rétablir « " + s.name + " »", cb, ca);
    }
    v.history.EndGroup();
    d.dirty = true;
    if (w != d.Width() || h != d.Height()) v.fitPending = true;
    Status("Instantané rétabli : " + s.name);
}

void App::DocSnapshotsUI(DocView& v) {
    ImGui::SeparatorText("Instantanés du document");
    if (ImGui::SmallButton("Créer un instantané")) TakeDocSnapshot(v);
    ui::HelpTooltip("Mémorise tout le document (calques, réglages, pixels, profil colorimétrique) ; cliquer sur un instantané\n"
                    "le rétablit (une étape d'annulation), même quand l'historique a oublié les étapes qui y menaient.\n"
                    "Le contenu des objets dynamiques n'en fait pas partie.");
    u64 total = 0;
    for (auto& s : v.docSnapshots) total += s.bytes;
    if (!v.docSnapshots.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%zu, %.0f Mo au plus", v.docSnapshots.size(), total / 1048576.0);
    }
    int& renaming = v.snapshotRenaming;  // (per document)
    for (size_t i = 0; i < v.docSnapshots.size(); ++i) {
        DocSnapshot& s = v.docSnapshots[i];
        ImGui::PushID((int)i);
        if (renaming == (int)i) {
            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##n", &s.name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) || ImGui::IsItemDeactivated())
                renaming = -1;
        } else {
            if (ImGui::Selectable(s.name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) renaming = (int)i;
                else RestoreDocSnapshot(v, i);
            }
            ImGui::SetItemTooltip("%u x %u px, %.0f Mo (clic : rétablir, double-clic : renommer)", s.state.width, s.state.height, s.bytes / 1048576.0);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Rétablir")) RestoreDocSnapshot(v, i);
                if (ImGui::MenuItem("Renommer")) renaming = (int)i;
                if (ImGui::MenuItem("Supprimer")) {
                    v.docSnapshots.erase(v.docSnapshots.begin() + i);
                    renaming = -1;
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break;
                }
                ImGui::EndPopup();
            }
        }
        ImGui::PopID();
    }
    ImGui::Separator();
}

}  // namespace sx
