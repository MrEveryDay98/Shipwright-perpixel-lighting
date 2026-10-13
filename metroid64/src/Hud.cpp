#include "Hud.h"

#include <cfloat>
#include <imgui.h>

namespace Metroid {

namespace {

void ShadowText(ImDrawList* dl, float size, ImVec2 pos, ImU32 color, const char* text) {
    dl->AddText(ImGui::GetFont(), size, ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 200), text);
    dl->AddText(ImGui::GetFont(), size, pos, color, text);
}

void CenteredText(ImDrawList* dl, float size, float y, ImU32 color, const char* text) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    ShadowText(dl, size, ImVec2(vp->Pos.x + (vp->Size.x - ext.x) * 0.5f, y), color, text);
}

} // namespace

Hud::Hud() : Ship::GuiWindow("gMetroid64.Hud", "Metroid64 HUD") {
}

void Hud::Draw() {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float scale = vp->Size.y / 480.0f;
    float big = 22.0f * scale, small = 16.0f * scale;
    const ImU32 white = IM_COL32(255, 255, 255, 255);

    if (!mRunning) {
        float y = vp->Pos.y + vp->Size.y * 0.3f;
        CenteredText(dl, big * 1.4f, y, IM_COL32(255, 160, 40, 255), "METROID64");
        CenteredText(dl, small, y + big * 2.5f, white, "Drop your Metroid (USA) .nes ROM onto this window,");
        CenteredText(dl, small, y + big * 3.4f, white, "or place it next to the executable as metroid.nes");
        if (!mRomError.empty()) {
            CenteredText(dl, small, y + big * 5.0f, IM_COL32(255, 90, 90, 255), mRomError.c_str());
        }
        return;
    }
    if (mRunFrames < 600) {
        mRunFrames++;
        CenteredText(dl, small, vp->Pos.y + vp->Size.y - small * 2.5f, IM_COL32(200, 200, 200, 255),
                     "Press F1 for the menu (graphics, controls, frame rate)");
    }
    if (mFrozen) {
        CenteredText(dl, big, vp->Pos.y + vp->Size.y * 0.1f, white, "EMULATION FROZEN (F1 > Game)");
    }
}

} // namespace Metroid
