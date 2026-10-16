#include "Hud.h"

#include "Menu.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <libultraship/bridge/consolevariablebridge.h>
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
        CenteredText(dl, big, vp->Pos.y + vp->Size.y * 0.1f, white, "GAME FROZEN (F1 > Game)");
    }
    bool paused = mPause.paused && mPause.paused();
    double time = ImGui::GetTime();
    if (paused && CVarGetInteger(kCVarPauseMap, 1) != 0) {
        float w = std::min(vp->Size.x * 0.8f, vp->Size.y * 1.1f), h = vp->Size.y * 0.7f;
        ImVec2 min(vp->Pos.x + (vp->Size.x - w) * 0.5f, vp->Pos.y + vp->Size.y * 0.1f);
        mMap.DrawArea(ImGui::GetForegroundDrawList(), min, ImVec2(min.x + w, min.y + h), time);
    }
    if (!paused && mPause.playing && mPause.playing() && CVarGetInteger(kCVarMinimap, 1) != 0) {
        float cell = std::floor(14.0f * scale);
        constexpr int kCols = 5, kRows = 3;
        ImVec2 min(vp->Pos.x + vp->Size.x - cell * kCols - 12.0f * scale, vp->Pos.y + 28.0f * scale);
        mMap.DrawMinimap(ImGui::GetForegroundDrawList(), min, cell, kCols, kRows, time);
    }
    if (paused) {
        DrawPauseMenu(scale);
    } else if (mPause.playing && mPause.playing()) {
        int64_t since = mPause.ticksSinceSave ? mPause.ticksSinceSave() : -1;
        float y = vp->Pos.y + vp->Size.y * 0.82f;
        if (since >= 0 && since < 120) {
            CenteredText(dl, big, y, IM_COL32(120, 255, 140, 255), "GAME SAVED");
        } else if (mPause.onSavePoint && mPause.onSavePoint()) {
            CenteredText(dl, big, y, IM_COL32(120, 220, 255, 255), "SAVE POINT  -  UP: SAVE");
        }
    }
}

void Hud::DrawPauseMenu(float scale) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.9f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##PauseMenu", nullptr, flags)) {
        ImGui::SetWindowFontScale(std::max(1.0f, scale));
        ImGui::TextUnformatted("PAUSED");
        bool canSave = mPause.canSave && mPause.canSave();
        ImGui::BeginDisabled(!canSave);
        if (ImGui::Button("Save game (Select)") && mPause.save) {
            mPause.save();
        }
        ImGui::EndDisabled();
        int64_t since = mPause.ticksSinceSave ? mPause.ticksSinceSave() : -1;
        if (!canSave) {
            ImGui::TextDisabled("Can't save during the escape");
        } else if (since >= 0 && since < 120) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Game saved");
        } else {
            ImGui::TextDisabled("Start: resume");
        }
    }
    ImGui::End();
}

} // namespace Metroid
