#pragma once

#include <functional>
#include <ship/window/gui/GuiMenuBar.h>

namespace Metroid {

class Game;

// Console variables owned by Metroid64.
#define M64_CVAR(name) "gMetroid64." name
constexpr const char* kCVarInterpolation = M64_CVAR("FrameInterpolation");
constexpr const char* kCVarTargetFps = M64_CVAR("TargetFps");
constexpr const char* kCVarMatchRefreshRate = M64_CVAR("MatchRefreshRate");

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
        std::function<Game*()> game; // nullptr until a ROM is loaded
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
    void DrawDeveloperMenu();
    void ToggleWindow(const char* name);

    Actions mActions;
};

} // namespace Metroid
