#pragma once

#include <functional>
#include <ship/window/gui/GuiMenuBar.h>

namespace Metroid {

// Console variables owned by Metroid64.
#define M64_CVAR(name) "gMetroid64." name
constexpr const char* kCVarInterpolation = M64_CVAR("FrameInterpolation");
constexpr const char* kCVarTargetFps = M64_CVAR("TargetFps");
constexpr const char* kCVarMatchRefreshRate = M64_CVAR("MatchRefreshRate");
constexpr const char* kCVarDisplay3D = M64_CVAR("Display3D");
// What the 3D view shows outside the original screen: 0 fog over it, 1 everything without fog, 2 everything
// without fog and the section's enemies all running (game/AllActive.h).
constexpr const char* kCVarOutsideView = M64_CVAR("OutsideView");
// No sprite flicker (game/Options.h). On by default.
constexpr const char* kCVarNoFlicker = M64_CVAR("NoFlicker");
constexpr const char* kCVarVolume = M64_CVAR("Volume");
constexpr const char* kCVarMute = M64_CVAR("Mute");

// libultraship's own console variables (it defines them only for its own target).
namespace LusCVar {
constexpr const char* kVsync = "gVsyncEnabled";
constexpr const char* kInternalResolution = "gInternalResolution";
constexpr const char* kMsaa = "gMSAAValue";
constexpr const char* kTextureFilter = "gTextureFilter";
constexpr const char* kNearestUpscale = "gNearestFramebufferFilter";
constexpr const char* kLowResMode = "gLowResMode";
constexpr const char* kControllerNav = "gControlNav";
constexpr const char* kInputEditorOpen = "gControllerConfigurationEnabled";
} // namespace LusCVar

// The F1 menu bar.
class Menu : public Ship::GuiMenuBar {
  public:
    struct Actions {
        std::function<void()> openRom;
        std::function<void()> reset;
        std::function<bool()> loaded; // a ROM is running
        bool* frozen = nullptr;       // game stopped from the menu
    };

    Menu(Actions actions);

  protected:
    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override {
    }

  private:
    void DrawGameMenu();
    void DrawGraphicsMenu();
    void DrawControlsMenu();
    void DrawAudioMenu();
    void DrawDeveloperMenu();
    void ToggleWindow(const char* name);

    Actions mActions;
};

} // namespace Metroid
