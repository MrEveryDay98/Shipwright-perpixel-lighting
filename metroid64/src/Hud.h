#pragma once

#include "Game.h"

#include <ship/window/gui/GuiWindow.h>
#include <string>

namespace Metroid {

// Energy/missile readout, pickup messages and the "please provide a ROM" screen, drawn on
// libultraship's ImGui layer over the 3D view.
class Hud : public Ship::GuiWindow {
  public:
    Hud();
    void Draw() override;

    void SetGame(const Game* game) {
        mGame = game;
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
    const Game* mGame = nullptr;
    std::string mRomError;
};

} // namespace Metroid
