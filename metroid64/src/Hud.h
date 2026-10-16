#pragma once

#include "MapView.h"

#include <ship/window/gui/GuiWindow.h>
#include <cstdint>
#include <functional>
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
    // The pause menu: shown while `paused`, with a button that calls `save` (enabled when `canSave`);
    // `ticksSinceSave` (-1: none) shows that it worked.
    struct PauseMenu {
        std::function<bool()> paused, canSave;
        std::function<bool()> playing; // in play (not paused): the minimap shows
        std::function<void()> save;
        std::function<int64_t()> ticksSinceSave;
    };
    void SetPauseMenu(PauseMenu menu) {
        mPause = std::move(menu);
    }

  protected:
    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }

  private:
    void DrawPauseMenu(float scale);

    bool mRunning = false;
    bool mFrozen = false;
    int mRunFrames = 0;
    std::string mRomError;
    PauseMenu mPause;
    MapView mMap;
};

} // namespace Metroid
