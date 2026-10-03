// Blend modes panel: built-in modes, library blends (HLSL or graph), previews
// on the active layer, new blends in code or in the visual graph editor.
#include "ui/App.h"
#include "ui/Theme.h"

namespace sx {

void App::DrawBlendPanel() {
    if (!ImGui::Begin("Fusions")) { ImGui::End(); return; }
    ImGui::TextDisabled("Modes de fusion");
    ImGui::End();
}

}  // namespace sx
