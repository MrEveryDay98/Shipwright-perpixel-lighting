#include "GameView.h"

namespace Metroid {

namespace {

// Wraps a plane coordinate difference into [-size/2, size/2).
int WrapDelta(int d, int size) {
    d %= size;
    if (d < -size / 2) {
        d += size;
    } else if (d >= size / 2) {
        d -= size;
    }
    return d;
}

// Wraps a plane coordinate difference into the period's copy nearest the screen centre.
int WrapAround(int d, int centre, int period) {
    int lo = centre - period / 2;
    return lo + (((d - lo) % period) + period) % period;
}

} // namespace

void GameView::ResetView() {
    mOamTags.fill({});
    mHaveScroll = false;
}

void GameView::TrackScroll(int x, int y) {
    mPrevWorldX = mWorldX;
    mPrevWorldY = mWorldY;
    if (!mHaveScroll) {
        mWorldX = mPrevWorldX = x;
        mWorldY = mPrevWorldY = y;
        mHaveScroll = true;
    } else {
        int dx = WrapDelta(x - mPlaneX, 512), dy = WrapDelta(y - mPlaneY, 480);
        mWorldX += dx;
        mWorldY += dy;
        if (std::abs(dx) > 32 || std::abs(dy) > 32) {
            mPrevWorldX = mWorldX; // a cut, not a scroll: don't interpolate across it
            mPrevWorldY = mWorldY;
        }
    }
    mPlaneX = x;
    mPlaneY = y;
}

bool GameView::InGameplay() const {
    uint8_t main = Peek(Ram::kMainRoutine);
    // The game engine draws its play field from pattern table $1000.
    return Peek(Ram::kTitleRoutine) == 0 && (main == 3 || main == 8) && (PpuCtrl() & 0x10);
}

float GameView::SamusScreenX() const {
    bool horizontalRoom = Mirroring() == 2;
    int px = (horizontalRoom ? (Peek(Ram::kSamusHi) & 1) * 256 : 0) + Peek(Ram::kSamusX);
    return (float)WrapAround(px - mPlaneX, 128, horizontalRoom ? 512 : 256);
}

float GameView::SamusScreenY() const {
    bool horizontalRoom = Mirroring() == 2;
    int py = (horizontalRoom ? 0 : (Peek(Ram::kSamusHi) & 1) * 240) + Peek(Ram::kSamusY);
    return (float)WrapAround(py - mPlaneY, 120, horizontalRoom ? 240 : 480);
}

} // namespace Metroid
