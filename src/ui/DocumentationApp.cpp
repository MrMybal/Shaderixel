#include "ui/App.h"
#include <filesystem>
#include <shellapi.h>
#include "ui/Lang.h"
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
    const auto path=Utf8Path(ExeDir())/"docs/manual/index.html";
    if (!documentation.exampleRequest.empty()) {
        const auto id=std::exchange(documentation.exampleRequest,{});
        const auto example=path.parent_path()/"examples"/(id+".sxl");
        if (ui::ManualPracticeAvailable(id) && std::filesystem::is_regular_file(example) && LoadSxl(Narrow(example.wstring()))) {
            if (auto* view=Active()) {view->doc->path.clear();view->doc->dirty=true;}
            Log("[manual] opened practice copy %s",id.c_str());
        }
    }
    if (!documentation.open || !documentation.focus) return;
    documentation.focus=false;
    if (!std::filesystem::is_regular_file(path)) {
        Status(cyui::tr("Offline documentation is missing. Repair or reinstall Shaderixel."));
        Log("[cydoc] missing local reader");return;
    }
    const auto topic=ui::ManualPages()[documentation.page].id;
    const auto language=documentation.language.empty() ? CurrentLanguage() : documentation.language;
    const auto url=ui::ManualUrl(path,language,topic,documentation.anchor,documentation.media);
    if (url.empty()) {Status("Could not construct the local documentation address.");return;}
    documentation.lastUrl=url;
    Log("[cydoc] open %s",url.c_str());
    // Capture exact native routes during automation, without opening browser windows.
    if (Automated() && getenv("SX_CYDOC_CAPTURE_LINKS")) return;
    const auto result=ShellExecuteW(hwnd,L"open",Widen(url).c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result)<=32) Status(cyui::tr("Could not open the offline manual in your browser."));
}
} // namespace sx
