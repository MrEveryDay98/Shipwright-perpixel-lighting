#include "Hud.h"

#include <cstdio>
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

    if (mGame == nullptr) {
        float y = vp->Pos.y + vp->Size.y * 0.3f;
        CenteredText(dl, big * 1.4f, y, IM_COL32(255, 160, 40, 255), "METROID64");
        CenteredText(dl, small, y + big * 2.5f, white, "Drop your Metroid (USA) .nes ROM onto this window,");
        CenteredText(dl, small, y + big * 3.4f, white, "or place it next to the executable as metroid.nes");
        if (!mRomError.empty()) {
            CenteredText(dl, small, y + big * 5.0f, IM_COL32(255, 90, 90, 255), mRomError.c_str());
        }
        return;
    }

    const Samus& s = mGame->GetSamus();
    char buf[128];
    float x = vp->Pos.x + 24 * scale, y = vp->Pos.y + 20 * scale;

    int tanks = (s.maxEnergy - 99) / 100;
    int full = s.energy / 100;
    std::string tankStr;
    for (int i = 0; i < tanks; i++) {
        tankStr += i < full ? "[#]" : "[ ]";
    }
    if (!tankStr.empty()) {
        ShadowText(dl, small, ImVec2(x, y), IM_COL32(120, 200, 255, 255), tankStr.c_str());
        y += small * 1.1f;
    }
    snprintf(buf, sizeof(buf), "EN..%02d", s.energy % 100);
    ShadowText(dl, big, ImVec2(x, y), white, buf);
    if (s.maxMissiles > 0) {
        snprintf(buf, sizeof(buf), "%sMISSILES %03d", s.missileMode ? "> " : "  ", s.missiles);
        ShadowText(dl, small, ImVec2(x, y + big * 1.2f), s.missileMode ? IM_COL32(255, 220, 80, 255) : white, buf);
    }

    if (mGame->MessageTimer() > 0) {
        CenteredText(dl, big, vp->Pos.y + vp->Size.y * 0.18f, IM_COL32(255, 230, 120, 255), mGame->Message().c_str());
    }
    if (mGame->Paused()) {
        CenteredText(dl, big * 1.5f, vp->Pos.y + vp->Size.y * 0.42f, white, "PAUSE");
        CenteredText(dl, small, vp->Pos.y + vp->Size.y * 0.42f + big * 2.0f, white, mGame->AreaName());
    }
}

} // namespace Metroid
