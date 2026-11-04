#include "ui/App.h"
#include "ui/Theme.h"
#include "ui/Lang.h"
#include <cyui/Locale.h>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>

namespace sx {
void App::InitUpdates() {
    cyui::registerTranslations("fr", {
        {"Check for updates...", "Rechercher des mises à jour..."},
        {"Software updates", "Mises à jour du logiciel"},
        {"Check at startup", "Vérifier au démarrage"},
        {"Include alpha and prerelease versions", "Inclure les versions alpha et préliminaires"},
        {"Check now", "Vérifier maintenant"}, {"Checking GitHub...", "Vérification sur GitHub..."},
        {"You are up to date.", "Vous utilisez la dernière version."},
        {"Download update", "Télécharger la mise à jour"}, {"Downloading...", "Téléchargement..."},
        {"Cancel download", "Annuler le téléchargement"},
        {"Verified installer ready.", "Installateur vérifié et prêt."},
        {"Install and restart", "Installer et redémarrer"},
        {"You will be asked to save unsaved documents before the installer opens.", "Les documents modifiés devront être enregistrés ou fermés avant l'ouverture de l'installateur."},
        {"View releases on GitHub", "Voir les versions sur GitHub"},
        {"A new Shaderixel version is available.", "Une nouvelle version de Shaderixel est disponible."}
    });
    if (updateCheckRequested || (updateCheckAtStartup && !Automated())) {
        Log("[updates] %s check", updateCheckRequested ? "Manual" : "Startup");
        updateClient.CheckNow(updateIncludePrereleases);
    }
}
void App::InstallReadyUpdate() {
    auto state = updateClient.Get();
    if (state.phase != updates::Phase::Ready || !updates::VerifyFile(state.installer, state.release)) {
        updateInstallError = "Installer verification failed. Download the update again."; return;
    }
    updateInstallError.clear();
    pendingUpdateInstaller = state.installer;
    PostMessageW(hwnd, WM_CLOSE, 0, 0); // use the normal document save/discard/cancel path
}
void App::DrawUpdates() {
    auto state = updateClient.Get();
    if (state.phase == updates::Phase::Checking) updateNotified = false;
    if (state.phase == updates::Phase::Available && !updateNotified) {
        updateNotified = true; showUpdates = true;
        if (!Automated()) cyui::notify(cyui::tr("A new Shaderixel version is available.").c_str());
    }
    if (!showUpdates) return;
    auto label = [](const char* text) { return cyui::trLabel(text); };
    ImGui::SetNextWindowSize(ImVec2(610 * dpi, 490 * dpi), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(label("Software updates").c_str(), &showUpdates)) { ImGui::End(); return; }
    ImGui::Text("Shaderixel %s", updates::Version);
    if (ImGui::Checkbox(label("Check at startup").c_str(), &updateCheckAtStartup)) SaveAiSettings();
    if (ImGui::Checkbox(label("Include alpha and prerelease versions").c_str(), &updateIncludePrereleases)) SaveAiSettings();
    bool busy = state.phase == updates::Phase::Checking || state.phase == updates::Phase::Downloading;
    ImGui::BeginDisabled(busy);
    if (ImGui::Button(label("Check now").c_str())) { updateInstallError.clear(); updateClient.CheckNow(updateIncludePrereleases); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(label("View releases on GitHub").c_str()))
        ShellExecuteW(hwnd, L"open", Widen(updates::ReleasesUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    ImGui::Separator();
    auto text = [](const char* value) { ImGui::TextWrapped("%s", cyui::tr(value).c_str()); };
    if (state.phase == updates::Phase::Checking) text("Checking GitHub...");
    if (state.phase == updates::Phase::Current) text("You are up to date.");
    if (state.phase == updates::Phase::Error) ImGui::TextWrapped("%s", state.error.c_str());
    if (!updateInstallError.empty()) ImGui::TextWrapped("%s", updateInstallError.c_str());
    if (!state.release.version.empty()) ImGui::Text("%s   (%.1f MB)", state.release.version.c_str(), state.release.size / 1000000.0);
    if (state.phase == updates::Phase::Available && ImGui::Button(label("Download update").c_str())) {
        PWSTR local = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local))) {
            auto cache = std::filesystem::path(local) / L"Shaderixel/Updates";
            CoTaskMemFree(local); updateClient.DownloadNow(Narrow(cache.wstring()));
        } else updateInstallError = "Cannot locate the update download folder.";
    }
    if (state.phase == updates::Phase::Downloading) {
        text("Downloading...");
        ImGui::ProgressBar(state.release.size ? (float)((double)state.received / state.release.size) : 0.f);
        if (ImGui::Button(label("Cancel download").c_str())) updateClient.Cancel();
    }
    if (state.phase == updates::Phase::Ready) {
        text("Verified installer ready.");
        text("You will be asked to save unsaved documents before the installer opens.");
        if (ImGui::Button(label("Install and restart").c_str())) InstallReadyUpdate();
    }
    if (!state.release.notes.empty()) {
        ImGui::Separator();
        ImGui::BeginChild("##release_notes", ImVec2(0, 0));
        ImGui::TextWrapped("%s", state.release.notes.c_str());
        ImGui::EndChild();
    }
    ImGui::End();
}
}
