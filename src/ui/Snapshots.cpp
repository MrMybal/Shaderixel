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
    DocSnapshot s;
    s.state = d.S();
    s.name = "Instantané " + std::to_string(++v.snapshotCounter);
    for (auto& [id, surf] : s.state.surfaces) {
        if (!surf.tex) continue;
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
    if (fluidRun.active) FluidFinish(true);
    const DocSnapshot& s = v.docSnapshots[index];
    DocState before = d.S();
    DocState st = s.state;
    // Fresh copies: painting after the restore must not change the snapshot.
    for (auto& [id, surf] : st.surfaces) {
        if (!surf.tex) continue;
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
    v.history.PushState("Rétablir « " + s.name + " »", before, d.S());
    d.dirty = true;
    if (w != d.Width() || h != d.Height()) v.fitPending = true;
    Status("Instantané rétabli : " + s.name);
}

void App::DocSnapshotsUI(DocView& v) {
    ImGui::SeparatorText("Instantanés du document");
    if (ImGui::SmallButton("Créer un instantané")) TakeDocSnapshot(v);
    ui::HelpTooltip("Mémorise tout le document (calques, réglages, pixels) ; cliquer sur un instantané le rétablit\n"
                    "(une étape d'annulation), même quand l'historique a oublié les étapes qui y menaient.");
    u64 total = 0;
    for (auto& s : v.docSnapshots) total += s.bytes;
    if (!v.docSnapshots.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%zu, %.0f Mo", v.docSnapshots.size(), total / 1048576.0);
    }
    static size_t renaming = (size_t)-1;
    for (size_t i = 0; i < v.docSnapshots.size(); ++i) {
        DocSnapshot& s = v.docSnapshots[i];
        ImGui::PushID((int)i);
        if (renaming == i) {
            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##n", &s.name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) || ImGui::IsItemDeactivated())
                renaming = (size_t)-1;
        } else {
            if (ImGui::Selectable(s.name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) renaming = i;
                else RestoreDocSnapshot(v, i);
            }
            ImGui::SetItemTooltip("%u x %u px, %.0f Mo (clic : rétablir, double-clic : renommer)", s.state.width, s.state.height, s.bytes / 1048576.0);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Rétablir")) RestoreDocSnapshot(v, i);
                if (ImGui::MenuItem("Renommer")) renaming = i;
                if (ImGui::MenuItem("Supprimer")) {
                    v.docSnapshots.erase(v.docSnapshots.begin() + i);
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
