// Unreal .uasset in the app: textures decoded natively (documents, layers, smart objects, image
// nodes), meshes converted by Unreal in the background (Paint 3D projects, render scenes).
#include "ui/App.h"
#include "io/UAsset.h"

#include <stb_image.h>
#include <cstdlib>

namespace sx {

stbi_uc* App::LoadImagePixels(const std::string& path, int& w, int& h) {
    std::string ext = Narrow(Utf8Path(path).extension().wstring());
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    if (ext == ".uasset") {
        std::vector<u8> rgba;
        u32 uw = 0, uh = 0;
        std::string err;
        if (!LoadUAssetImage(path, rgba, uw, uh, &err)) {
            Status("Texture Unreal : " + err);
            return nullptr;
        }
        auto* px = (stbi_uc*)malloc(rgba.size());  // freed by stbi_image_free (free)
        if (!px) return nullptr;
        memcpy(px, rgba.data(), rgba.size());
        w = (int)uw;
        h = (int)uh;
        return px;
    }
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(path).c_str(), L"rb") != 0 || !f) return nullptr;
    int comp = 0;
    stbi_uc* px = stbi_load_from_file(f, &w, &h, &comp, 4);
    fclose(f);
    return px;
}

void App::ConvertUAssetAsync(const std::string& path, std::function<void(const std::string& glb)> done) {
    if (std::string cached = UAssetMeshCached(path); !cached.empty()) {
        if (done) done(cached);
        return;
    }
    for (auto& j : uassetJobs)
        if (j->path == path) {  // already converting: one more consumer
            if (done) j->done.push_back(std::move(done));
            return;
        }
    auto job = std::make_unique<UAssetJob>();
    job->path = path;
    job->name = Narrow(Utf8Path(path).stem().wstring());
    if (done) job->done.push_back(std::move(done));
    job->result = std::async(std::launch::async, [path]() {
        std::string glb, err;
        ConvertUAssetMesh(path, glb, &err);
        return std::make_pair(glb, err);
    });
    Status("Conversion du maillage Unreal " + job->name + " par Unreal Editor (en arrière-plan, la première fois peut prendre quelques minutes)...");
    uassetJobs.push_back(std::move(job));
}

void App::UpdateUAssetJobs() {
    // Off-tab documents (SubGraph loads): kept a few frames, until the GPU no longer reads them.
    if (loadedSubDoc) {  // a document loaded for its image only (linked / replaced contents)
        auto keep = std::make_shared<DocView>();
        keep->doc = std::move(loadedSubDoc);
        deferredViews.push_back({frameCounter, keep});
    }
    std::erase_if(deferredViews, [&](const auto& e) { return frameCounter > e.first + 4; });
    for (size_t i = 0; i < uassetJobs.size();) {
        UAssetJob& j = *uassetJobs[i];
        if (j.result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++i;
            continue;
        }
        auto [glb, err] = j.result.get();
        if (glb.empty()) {
            uassetFailed.insert(j.path);
            Status("Maillage Unreal " + j.name + " : " + err);
        } else {
            Status("Maillage Unreal converti : " + j.name);
            for (auto& d : j.done) d(glb);
        }
        uassetJobs.erase(uassetJobs.begin() + i);
    }
}

bool App::OpenUAsset(const std::string& path, bool asLayer) {
    UAssetInfo info;
    if (!ReadUAssetInfo(path, info)) {
        Status("Package Unreal illisible : " + path);
        return true;
    }
    if (info.mesh) {
        // A new Paint 3D project once Unreal has exported the mesh.
        const std::string name = Narrow(Utf8Path(path).stem().wstring());
        ConvertUAssetAsync(path, [this, name](const std::string& glb) {
            p3dNew = {};
            p3dNew.mesh = 3;
            p3dNew.meshPath = glb;
            const size_t before = docs.size();
            NewPaint3DProject();
            // Renamed only when the project was created (a failed load leaves another document active).
            if (DocView* v = Active(); v && docs.size() > before && v == docs.back().get() && v->doc->kind == 1) v->doc->SetName(name + " (3D)");
        });
        return true;
    }
    if (!info.texture) {
        Status("Ce .uasset est un " + info.cls + " : seules les textures et les maillages (StaticMesh, SkeletalMesh) s'ouvrent");
        return true;
    }
    (void)asLayer;
    return false;  // a texture: decoded like an image by OpenImage (LoadImagePixels)
}

}  // namespace sx
