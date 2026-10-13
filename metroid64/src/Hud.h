#pragma once

#include <ship/window/gui/GuiWindow.h>
#include <string>

namespace Metroid {

// The "please provide a ROM" screen and a few notices, drawn on libultraship's ImGui layer. The game's
// own status display is part of the game's picture.
class Hud : public Ship::GuiWindow {
  public:
    Hud();
    void Draw() override;

    void SetRunning(bool running) {
        mRunning = running;
        mRunFrames = 0;
    }
    void SetFrozen(bool frozen) {
        mFrozen = frozen;
    }
    void SetRomError(const std::string& error) {
        mRomError = error;
    }

  protected:
    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }

  private:
    bool mRunning = false;
    bool mFrozen = false;
    int mRunFrames = 0;
    std::string mRomError;
};

} // namespace Metroid
