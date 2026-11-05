#include "ui/App.h"
#include <filesystem>
#include <shellapi.h>
#include "ui/Lang.h"
#include <stb_image.h>
#include <imgui_internal.h>

namespace sx {
std::string App::SelectedDocumentationTopic() {
    auto* view=Active();
    if (!view) return "getting_started";
    auto& doc=*view->doc;
    auto effect=[](const EffectInstance& fx,std::string_view fallback) {
        // Document overrides may no longer implement the shipped effect.
        if (!fx.customCode.empty() || !fx.graph.empty()) return std::string("authoring");
        return ui::ManualReferenceTopic("effect",fx.effectId,fallback);
    };
    if (const auto* node=doc.S().graph.Find(doc.S().activeNode)) {
        if (node->layer && (node->id&0x40000000u) && (node->id&31u)==18) return "masks";
        switch (node->kind) {
        case NodeKind::Effect: return effect(node->fx,"image_graph");
        case NodeKind::Function: return ui::ManualReferenceTopic("function",node->name,"libraries");
        case NodeKind::Blend: return "blending";
        case NodeKind::Tool: return node->tool=="bake" ? "image_graph" : "ai";
        case NodeKind::Output: return "export";
        case NodeKind::Surface: return "painting";
        case NodeKind::Math: return ui::ManualReferenceTopic("math",node->op,"builtin_nodes");
        case NodeKind::Action: return ui::ManualReferenceTopic("exec",node->op,"builtin_nodes");
        case NodeKind::Split: return "node:split";
        case NodeKind::Value: return "node:value";
        case NodeKind::VarGet: return "node:varget";
        case NodeKind::VarSet: return "node:varset";
        case NodeKind::Event: return "node:event";
        case NodeKind::Reroute: return "node:reroute";
        case NodeKind::SubInput: return "node:subinput";
        case NodeKind::SubOutput: return "node:suboutput";
        default: return "libraries";
        }
    }
    if (doc.Mode()==DocMode::Nodes) return "image_graph";
    if (doc.S().editMask) return "masks";
    if (const auto* layer=doc.ActiveLayer()) {
        switch (layer->kind) {
        case LayerKind::Fill:
        case LayerKind::Adjustment: return effect(layer->effect,"layers");
        case LayerKind::Text:
        case LayerKind::Shape: return "text_shapes";
        case LayerKind::Smart: return "smart_objects";
        default: break;
        }
    }
    return "layers";
}

std::string App::DocumentationTopic() {
    auto* focused=ImGui::GetCurrentContext()->NavWindow;
    auto isFocused=[&](const char* name) {
        auto* panel=ImGui::FindWindowByName(name);
        for (auto* window=focused;window;window=window->ParentWindow)
            if (window==panel) return true;
        return false;
    };
    if (isFocused("Propriétés") || isFocused("Graphe") || isFocused("Aperçu du calque")) return SelectedDocumentationTopic();
    if (isFocused("Éditeur d'effets")) {
        bool graphVisible=false;
        if (auto* view=Active()) {
            if (efEd.target==2) graphVisible=!(tool==Tool::Eraser ? eraser : brush).tipGraph.empty();
            else {
                auto* layer=view->doc->ActiveLayer();
                const auto* effect=efEd.target==1 ? (layer ? &layer->blendFx : nullptr) : EditedEffect(*view,nullptr);
                if (effect) {
                    const auto def=effect->Def();
                    graphVisible=!effect->graph.empty() || (effect->customCode.empty() && def && !def->graph.empty());
                }
            }
        }
        if (graphVisible && !efEd.browse && !efEd.libCode)
            if (const auto* node=efEd.graph.Find(efEd.selected)) return ui::ManualReferenceTopic("shader",node->op,"effect_editor");
        return "effect_editor";
    }
    if (isFocused("Pinceaux")) {
        const auto& selected=tool==Tool::Eraser ? eraser : brush;
        if (!selected.tipCode.empty() || !selected.tipGraph.empty()) return "authoring";
        return ui::ManualReferenceTopic("brush",selected.tipId,"brushes_tablet");
    }
    if (isFocused("Éditeur d'actions")) {
        const auto& builtins=BuiltinActions();
        const Action* action=builtinSel>=0 && builtinSel<static_cast<int>(builtins.size()) ? &builtins[builtinSel] :
            actionSel>=0 && actionSel<static_cast<int>(actions.size()) ? &actions[actionSel] : nullptr;
        if (action && actionStepSel>=0 && actionStepSel<static_cast<int>(action->steps.size()))
            return ui::ManualReferenceTopic("command",action->steps[actionStepSel].cmd,"actions");
        return "actions";
    }
    if (isFocused("Canevas") || isFocused("Vue 3D") || isFocused("Outils") || isFocused("Options")) {
        if (xf.active) return "dimensions";
        return std::string(ui::ManualToolTopic(static_cast<int>(tool)));
    }
    for (auto* window=focused;window;window=window->ParentWindow)
        if (const auto topic=ui::ManualPanelTopic(window->Name);!topic.empty()) return std::string(topic);
    return {}; // Unrecognized windows resume the last article.
}

void App::DrawDocumentation() {
    if (!documentation.open) return;
    const auto path = Utf8Path(ExeDir()) / "docs/manual/index.html";
    auto image = [&](std::string_view name) -> ui::ManualImage {
        const ui::ManualAsset* asset = nullptr;
        for (const auto& entry : ui::ManualAssets()) if (entry.path == name) { asset = &entry; break; }
        if (!asset) return {};
        auto [it, first] = manualImages.try_emplace(std::string(name));
        if (first) {
            HMODULE module = GetModuleHandleW(nullptr);
            HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(asset->resource), MAKEINTRESOURCEW(10));
            HGLOBAL memory = resource ? LoadResource(module, resource) : nullptr;
            const auto* bytes = memory ? static_cast<const stbi_uc*>(LockResource(memory)) : nullptr;
            int width=0, height=0, components=0;
            auto* pixels = bytes ? stbi_load_from_memory(bytes, static_cast<int>(SizeofResource(module,resource)), &width,&height,&components,4) : nullptr;
            if (pixels) {
                it->second = gpu::Dev().CreateTexture(width,height,gpu::Format::RGBA8,"ManualIllustration");
                ctx->Upload(it->second.get(),pixels,width*4);
                stbi_image_free(pixels);
                Log("[manual] embedded image %s %dx%d", std::string(name).c_str(), width, height);
            }
        }
        if (!it->second) return {};
        return {static_cast<ImTextureID>(gpu::Dev().UiTextureId(it->second.get())), asset->width, asset->height};
    };
    auto examplePath=[&](std::string_view id) {
        // Accept only a registered target in the embedded handbook.
        bool registered=false;
        for (const auto& page:ui::ManualPages()) for (const auto& block:page.blocks)
            if (block.kind=="example" && block.target==id) registered=true;
        return registered ? path.parent_path()/"examples"/(std::string(id)+".sxl") : std::filesystem::path{};
    };
    const bool browse=ui::DocumentationWindow(documentation, dpi, std::filesystem::is_regular_file(path), CurrentLanguage(), image,
        [&](std::string_view id){const auto file=examplePath(id);return !file.empty() && std::filesystem::is_regular_file(file);});
    if (!documentation.exampleRequest.empty()) {
        const auto file=examplePath(std::exchange(documentation.exampleRequest,{}));
        if (!file.empty() && std::filesystem::is_regular_file(file) && LoadSxl(Narrow(file.wstring()))) {
            // Practice documents start as unsaved copies, so Save never overwrites the bundled original.
            if (auto* view=Active()) {view->doc->path.clear();view->doc->dirty=true;}
            Log("[manual] opened practice copy %s",Narrow(file.filename().wstring()).c_str());
        }
    }
    if (browse) {
        // File URI plus encoded fragment preserves both chapter and language.
        auto encode=[](const std::string& text,bool pathPart) {
            static const char hex[]="0123456789ABCDEF"; std::string out;
            for (unsigned char c:text) {
                if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~'||(pathPart&&(c=='/'||c==':'))) out+=c;
                else {out+='%';out+=hex[c>>4];out+=hex[c&15];}
            }
            return out;
        };
        const auto language=ui::ResolveManualLanguage(documentation.language.empty()?CurrentLanguage():documentation.language);
        const auto page=ui::ManualPages(language)[documentation.page].id;
        auto file=Narrow(path.generic_wstring());
        const auto url=Widen("file:///"+encode(file,true)+"#"+std::string(language)+"/"+encode(std::string(page),false));
        auto result = ShellExecuteW(hwnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32) Status("Could not open the offline manual in your browser.");
    }
}
} // namespace sx
