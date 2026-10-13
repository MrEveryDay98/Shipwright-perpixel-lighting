#include "Menu.h"
#include "Game.h"

#include <fast/Fast3dWindow.h>
#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/config/Config.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>

#include <algorithm>

namespace Metroid {

namespace {

std::shared_ptr<Fast::Fast3dWindow> GetWindow() {
    return std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
}

void Save() {
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

bool CVarCheckbox(const char* label, const char* cvar, bool defaultValue) {
    bool v = CVarGetInteger(cvar, defaultValue) != 0;
    if (ImGui::Checkbox(label, &v)) {
        CVarSetInteger(cvar, v);
        Save();
        return true;
    }
    return false;
}

bool CVarSliderFloat(const char* label, const char* cvar, float min, float max, float defaultValue,
                     const char* fmt = "%.2f") {
    float v = CVarGetFloat(cvar, defaultValue);
    if (ImGui::SliderFloat(label, &v, min, max, fmt)) {
        CVarSetFloat(cvar, v);
        Save();
        return true;
    }
    return false;
}

const char* BackendName(int32_t id) {
    switch (id) {
        case Fast::FAST3D_DXGI_DX11:
            return "DirectX 11";
        case Fast::FAST3D_SDL_OPENGL:
            return "OpenGL";
        case Fast::FAST3D_SDL_METAL:
            return "Metal";
        default:
            return "Unknown";
    }
}

} // namespace

Menu::Menu(Actions actions) : Ship::GuiMenuBar(M64_CVAR("MenuBarOpen")), mActions(std::move(actions)) {
}

void Menu::InitElement() {
}

void Menu::ToggleWindow(const char* name) {
    auto window = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow(name);
    if (window) {
        window->ToggleVisibility();
    }
}

void Menu::DrawElement() {
    if (!ImGui::BeginMenuBar()) {
        return;
    }
    DrawGameMenu();
    DrawGraphicsMenu();
    DrawControlsMenu();
    DrawDeveloperMenu();
    ImGui::TextDisabled("  F1: hide menu");
    ImGui::EndMenuBar();
}

void Menu::DrawGameMenu() {
    if (!ImGui::BeginMenu("Game")) {
        return;
    }
    if (ImGui::MenuItem("Open ROM...")) {
        mActions.openRom();
    }
    Game* game = mActions.game();
    if (ImGui::MenuItem("Reset", nullptr, false, game != nullptr)) {
        mActions.reset();
    }
    if (game != nullptr) {
        bool paused = game->Paused();
        if (ImGui::MenuItem("Pause", nullptr, &paused)) {
            game->SetPaused(paused);
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Quit")) {
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    ImGui::EndMenu();
}

void Menu::DrawGraphicsMenu() {
    if (!ImGui::BeginMenu("Graphics")) {
        return;
    }
    auto wnd = GetWindow();

    bool fullscreen = wnd->IsFullscreen();
    if (ImGui::MenuItem("Fullscreen", "F11", &fullscreen)) {
        wnd->SetFullscreen(fullscreen);
    }
    CVarCheckbox("VSync", LusCVar::kVsync, true);

    ImGui::SeparatorText("Frame rate");
    CVarCheckbox("Frame interpolation", kCVarInterpolation, true);
    ImGui::SetItemTooltip("Game logic runs at 60 Hz like the original; rendering in between is interpolated.");
    if (CVarGetInteger(kCVarInterpolation, 1)) {
        CVarCheckbox("Match refresh rate", kCVarMatchRefreshRate, true);
        if (!CVarGetInteger(kCVarMatchRefreshRate, 1)) {
            int fps = CVarGetInteger(kCVarTargetFps, 60);
            if (ImGui::SliderInt("Target FPS", &fps, 30, 360)) {
                CVarSetInteger(kCVarTargetFps, fps);
                Save();
            }
        } else {
            ImGui::TextDisabled("Rendering at %u FPS", wnd->GetCurrentRefreshRate());
        }
    }

    ImGui::SeparatorText("Resolution");
    if (CVarSliderFloat("Internal resolution", LusCVar::kInternalResolution, 0.5f, 4.0f, 1.0f, "%.2fx")) {
        wnd->SetResolutionMultiplier(CVarGetFloat(LusCVar::kInternalResolution, 1.0f));
    }
    int msaa = CVarGetInteger(LusCVar::kMsaa, 1);
    if (ImGui::SliderInt("Anti-aliasing (MSAA)", &msaa, 1, 8)) {
        CVarSetInteger(LusCVar::kMsaa, msaa);
        wnd->SetMsaaLevel(msaa);
        Save();
    }
    const char* lowRes[] = { "Off", "N64 (320x240)", "240p widescreen" };
    int lowResMode = std::clamp(CVarGetInteger(LusCVar::kLowResMode, 0), 0, 2);
    if (ImGui::Combo("Low resolution mode", &lowResMode, lowRes, 3)) {
        CVarSetInteger(LusCVar::kLowResMode, lowResMode);
        Save();
    }
    CVarCheckbox("Sharp upscaling (nearest neighbour)", LusCVar::kNearestUpscale, false);

    ImGui::SeparatorText("Textures");
    const char* filters[] = { "Three-point (N64)", "Linear", "None (sharp pixels)" };
    int filter = std::clamp(CVarGetInteger(LusCVar::kTextureFilter, Fast::FILTER_NONE), 0, 2);
    if (ImGui::Combo("Texture filter", &filter, filters, 3)) {
        CVarSetInteger(LusCVar::kTextureFilter, filter);
        wnd->SetTextureFilter((Fast::FilteringMode)filter);
        Save();
    }

    ImGui::SeparatorText("Camera");
    CVarSliderFloat("Field of view", M64_CVAR("Camera.Fov"), 30.0f, 90.0f, 52.0f, "%.0f deg");
    CVarSliderFloat("Distance", M64_CVAR("Camera.Distance"), 150.0f, 600.0f, 300.0f, "%.0f");
    CVarSliderFloat("Height", M64_CVAR("Camera.Height"), -60.0f, 160.0f, 36.0f, "%.0f");
    if (ImGui::Button("Reset camera")) {
        CVarClear(M64_CVAR("Camera.Fov"));
        CVarClear(M64_CVAR("Camera.Distance"));
        CVarClear(M64_CVAR("Camera.Height"));
        Save();
    }

    ImGui::SeparatorText("Renderer");
    auto backends = wnd->GetAvailableWindowBackends();
    int32_t current = wnd->GetWindowBackend();
    if (backends && ImGui::BeginCombo("Backend", BackendName(current))) {
        for (int32_t b : *backends) {
            if (ImGui::Selectable(BackendName(b), b == current)) {
                // Picked up by libultraship on the next start.
                auto config = Ship::Context::GetRawInstance()->GetConfig();
                config->SetInt("Window.Backend.Id", b);
                config->SetString("Window.Backend.Name", BackendName(b));
                config->Save();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("Backend changes apply after a restart.");
    ImGui::EndMenu();
}

void Menu::DrawControlsMenu() {
    if (!ImGui::BeginMenu("Controls")) {
        return;
    }
    if (ImGui::MenuItem("Controller configuration...")) {
        ToggleWindow("Input Editor");
    }
    CVarCheckbox("Navigate menus with controller", LusCVar::kControllerNav, false);
    ImGui::SeparatorText("Defaults");
    ImGui::TextUnformatted("Move / aim up / morph: stick, D-pad or WASD\n"
                           "Jump: A (X key)    Shoot / bomb: B (C key)\n"
                           "Toggle missiles: R or Z    Pause: Start (Space)");
    ImGui::EndMenu();
}

void Menu::DrawDeveloperMenu() {
    if (!ImGui::BeginMenu("Developer")) {
        return;
    }
    if (ImGui::MenuItem("Stats")) {
        ToggleWindow("Stats");
    }
    if (ImGui::MenuItem("Console")) {
        ToggleWindow("Console");
    }
    Game* game = mActions.game();
    if (game != nullptr) {
        ImGui::SeparatorText("Cheats");
        Cheats& cheats = game->GetCheats();
        ImGui::Checkbox("Infinite energy", &cheats.infiniteEnergy);
        ImGui::Checkbox("Infinite missiles", &cheats.infiniteMissiles);
        if (ImGui::MenuItem("Give all items")) {
            game->GiveAllItems();
        }
    }
    ImGui::EndMenu();
}

} // namespace Metroid
