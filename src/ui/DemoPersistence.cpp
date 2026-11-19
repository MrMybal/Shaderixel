#include "ui/App.h"

namespace sx {

// --demo project-io exercises the application's asynchronous save and close paths.
// It uses only its own project in a private temporary directory.
void App::DemoProjectPersistence() {
    if (frameCounter < 4) return;
    static int phase = 0, failures = 0;
    static DocView* parent = nullptr;
    static std::filesystem::path folder;
    static std::string project, recovery;
    static Id outer = 0, inner = 0, pixels = 0;
    static gpu::ReadbackPtr original, restored;
    static HANDLE lock = INVALID_HANDLE_VALUE;
    auto check = [&](bool ok, const char* msg) {
        Log("[project-io] %s %s", ok ? "PASS" : "FAIL", msg);
        if (!ok) ++failures;
    };
    auto indexOfParent = [&]() {
        for (int i = 0; i < (int)docs.size(); ++i) if (docs[i].get() == parent) return i;
        return -1;
    };
    auto finish = [&] {
        if (lock != INVALID_HANDLE_VALUE) { CloseHandle(lock); lock = INVALID_HANDLE_VALUE; }
        std::error_code ec;
        if (!project.empty()) std::filesystem::remove(Utf8Path(project), ec);
        if (!recovery.empty()) std::filesystem::remove(Utf8Path(recovery), ec);
        if (!folder.empty()) std::filesystem::remove(folder, ec);
        Log("[project-io] COMPLETE %d failures", failures);
        uiErrorCount += failures;
        phase = 5;
        autoQuit = true;
    };
    if (phase == 0) {
        folder = std::filesystem::temp_directory_path() / ("shaderixel-io-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        if (!std::filesystem::create_directory(folder)) { check(false, "private temporary directory"); finish(); return; }
        project = Narrow((folder / "nested.sxl").wstring());
        recovery = Narrow((folder / "recovered.sxl").wstring());
        NewDocument(32, 32, Precision::Half16, DocMode::Layers, 3, "Nested save fixture");
        parent = Active();
        parent->doc->AddPixelLayer("Outer");
        ConvertToSmartObject(*parent);
        outer = parent->doc->ActiveLayer()->smart;
        EditSmartContents(*parent, *parent->doc->ActiveLayer());
        DocView& child = *Active();
        child.doc->AddPixelLayer("Inner");
        ConvertToSmartObject(child);
        inner = child.doc->ActiveLayer()->smart;
        EditSmartContents(child, *child.doc->ActiveLayer());
        DocView& leaf = *Active();
        TextStyle caption;
        caption.content = "Nested editable text";
        leaf.doc->AddTextLayer(caption);
        Layer& layer = leaf.doc->AddPixelLayer("Editable detail");
        pixels = layer.surface;
        leaf.doc->CancelPendingClear(pixels);
        Prepare(leaf);
        std::vector<u16> red(32 * 32 * 4);
        for (size_t i = 0; i < red.size(); i += 4) red[i] = red[i + 3] = 0x3c00;
        ctx->Upload(leaf.doc->GetSurface(pixels)->tex.get(), red.data(), 32 * 8);
        leaf.doc->TouchSurface(pixels);
        original = ctx->ReadbackTexture(leaf.doc->GetSurface(pixels)->tex.get());
        phase = 1;
        return;
    }
    if (phase == 1) {
        SaveDocument(*parent, project);
        const int index = indexOfParent();
        CloseDocument(index);
        check(indexOfParent() == index && !saves.empty(), "close waits for the write");
        parent->doc->AddPixelLayer("Edited while writing");
        Prepare(*parent);
        phase = 2;
        return;
    }
    if (phase == 2 && saves.empty()) {
        check(indexOfParent() >= 0, "a newer edit kept the original tab open");
        if (indexOfParent() < 0) { finish(); return; }
        check(parent->doc->dirty, "a newer edit remained dirty");
        // Close editable children after their open-tab state was captured.
        for (int i = (int)docs.size() - 1; i >= 0; --i) if (docs[i]->smartLink.parent) CloseDocument(i, true);
        const bool opened = LoadSxl(project);
        check(opened, "saved project reopened");
        if (!opened) { finish(); return; }
        auto& doc = *Active()->doc;
        auto* outerObject = doc.GetSmart(outer);
        auto* innerObject = outerObject && outerObject->child ? outerObject->child->GetSmart(inner) : nullptr;
        Document* leaf = innerObject ? innerObject->child.get() : nullptr;
        const Surface* surface = leaf ? leaf->GetSurface(pixels) : nullptr;
        check(leaf && leaf->S().layers.size() == 3 && leaf->S().layers.back().name == "Editable detail", "nested editable layers survived");
        check(leaf && std::any_of(leaf->S().layers.begin(), leaf->S().layers.end(), [](const Layer& layer) {
            return layer.kind == LayerKind::Text && layer.text.content == "Nested editable text";
        }), "nested text reopened as an editable text layer");
        if (surface) restored = ctx->ReadbackTexture(surface->tex.get());
        check(restored != nullptr, "nested pixel surface reopened");
        const bool stale = std::any_of(doc.S().layers.begin(), doc.S().layers.end(), [](const Layer& l) { return l.name == "Edited while writing"; });
        check(!stale, "snapshot excludes later edits");
        lock = CreateFileW(Utf8Path(project).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        check(lock != INVALID_HANDLE_VALUE, "destination locked for failure case");
        if (lock == INVALID_HANDLE_VALUE) { finish(); return; }
        SaveAndClose(indexOfParent());
        phase = 3;
        return;
    }
    if (phase == 3 && saves.empty() && (!restored || restored->ready) && (!original || original->ready)) {
        const bool red = original && original->data.size() == 32 * 32 * 8 && original->data[0] == 0 &&
            original->data[1] == 0x3c && original->data[6] == 0 && original->data[7] == 0x3c;
        check(red, "source pixels contain the painted color");
        check(red && restored && restored->data == original->data, "all nested pixels are intact");
        check(indexOfParent() >= 0 && parent->doc->dirty, "failed Save and Close preserved the modified document");
        if (indexOfParent() < 0) { finish(); return; }
        SaveDocument(*parent, recovery);
        check(parent->doc->path == project, "Save As kept the old path until the write succeeded");
        CloseDocument(indexOfParent());
        phase = 4;
        return;
    }
    if (phase == 4 && saves.empty()) {
        check(indexOfParent() < 0, "successful retry closed the original tab");
        bool hasEdit = false;
        if (LoadSxl(recovery)) for (const auto& l : Active()->doc->S().layers) hasEdit |= l.name == "Edited while writing";
        check(hasEdit, "latest edit reopened from Save As while the original remained locked");
        restored.reset();
        original.reset();
        finish();
    }
}
}  // namespace sx
