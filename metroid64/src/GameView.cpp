#include "GameView.h"

#include <algorithm>

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
    mPrev2WorldX = mPrevWorldX;
    mPrev2WorldY = mPrevWorldY;
    mPrevWorldX = mWorldX;
    mPrevWorldY = mWorldY;
    mScrollCut = false;
    if (!mHaveScroll) {
        mWorldX = x;
        mWorldY = y;
        mScrollCut = true;
        mHaveScroll = true;
    } else {
        int dx = WrapDelta(x - mPlaneX, 512), dy = WrapDelta(y - mPlaneY, 480);
        mWorldX += dx;
        mWorldY += dy;
        mScrollCut = std::abs(dx) > 32 || std::abs(dy) > 32;
    }
    if (mScrollCut) {
        // A cut, not a scroll: don't interpolate across it.
        mPrevWorldX = mPrev2WorldX = mWorldX;
        mPrevWorldY = mPrev2WorldY = mWorldY;
    }
    mPlaneX = x;
    mPlaneY = y;
}

namespace {
// (p1 + p2) / 2 - (1 - alpha) * (p2 - p0) / 2, relative to p2: between the averages of (p0, p1) and (p1, p2).
double SmoothOffset(double p0, double p1, double p2, float alpha) {
    return (p1 - p2) / 2 - (1.0 - alpha) * (p2 - p0) / 2;
}
} // namespace

double GameView::SmoothScrollOffsetX(float alpha) const {
    return SmoothOffset(mPrev2WorldX, mPrevWorldX, mWorldX, alpha);
}

double GameView::SmoothScrollOffsetY(float alpha) const {
    return SmoothOffset(mPrev2WorldY, mPrevWorldY, mWorldY, alpha);
}

void GameView::Motion::Set(bool present, float nx, float ny, bool cut) {
    prev2X = prevX;
    prev2Y = prevY;
    prevX = x;
    prevY = y;
    x = nx;
    y = ny;
    // A jump (a new room, a respawn, a new enemy in the slot) isn't motion: start over from here.
    bool jump = cut || std::abs(x - prevX) > 24 || std::abs(y - prevY) > 24;
    if (!present) {
        have = 0;
    } else if (jump || have == 0) {
        have = 1;
    } else {
        have = std::min(have + 1, 3);
    }
}

float GameView::Motion::OffsetX(float alpha) const {
    if (have >= 3) {
        return (float)SmoothOffset(prev2X, prevX, x, alpha);
    }
    return have == 2 ? -(1.0f - alpha) * (x - prevX) : 0.0f;
}

float GameView::Motion::OffsetY(float alpha) const {
    if (have >= 3) {
        return (float)SmoothOffset(prev2Y, prevY, y, alpha);
    }
    return have == 2 ? -(1.0f - alpha) * (y - prevY) : 0.0f;
}

void GameView::TrackObjects() {
    // The picture of a frame (sprites and scroll, written in the vblank) comes from the game's previous pass
    // through its main loop, while RAM already holds this pass's positions: so the positions used are the
    // ones read a frame ago, placed with this frame's scroll.
    float wx = (float)mWorldX, wy = (float)mWorldY;
    mSamusMotion.Set(mPendingSamus.present, ScreenXOf(mPendingSamus.hi, mPendingSamus.x) + wx,
                     ScreenYOf(mPendingSamus.hi, mPendingSamus.y) + wy, mScrollCut);
    mPendingSamus = { true, Peek(Ram::kSamusHi), Peek(Ram::kSamusX), Peek(Ram::kSamusY) };
    for (int i = 0; i < 6; i++) {
        const PendingObject& p = mPendingEnemies[i];
        mEnemyMotion[i].Set(p.present, ScreenXOf(p.hi, p.x) + wx, ScreenYOf(p.hi, p.y) + wy, mScrollCut);
        uint8_t slot = (uint8_t)(i * 0x10);
        mPendingEnemies[i] = { Peek((uint16_t)(0x6AF4 + slot)) != 0, Peek((uint16_t)(0x6AFB + slot)),
                               Peek((uint16_t)(0x0401 + slot)), Peek((uint16_t)(0x0400 + slot)) };
    }
}

float GameView::SamusDrawX() const {
    return mSamusMotion.x - (float)mWorldX;
}

float GameView::SamusDrawY() const {
    return mSamusMotion.y - (float)mWorldY;
}

bool GameView::InGameplay() const {
    uint8_t main = Peek(Ram::kMainRoutine);
    // Playing (3), paused (5), Samus appearing (8), waiting on a timer over the play field (9: a power-up's
    // jingle, or Samus's death before the game over screen). The game engine draws its play field from
    // pattern table $1000.
    bool field = main == 3 || main == 5 || main == 8 || main == 9;
    return Peek(Ram::kTitleRoutine) == 0 && field && (PpuCtrl() & 0x10);
}

float GameView::ScreenXOf(uint8_t hi, uint8_t x) const {
    bool horizontalRoom = Mirroring() == 2;
    int px = (horizontalRoom ? (hi & 1) * 256 : 0) + x;
    return (float)WrapAround(px - mPlaneX, 128, horizontalRoom ? 512 : 256);
}

float GameView::ScreenYOf(uint8_t hi, uint8_t y) const {
    bool horizontalRoom = Mirroring() == 2;
    int py = (horizontalRoom ? 0 : (hi & 1) * 240) + y;
    return (float)WrapAround(py - mPlaneY, 120, horizontalRoom ? 240 : 480);
}

float GameView::SamusScreenX() const {
    return ScreenXOf(Peek(Ram::kSamusHi), Peek(Ram::kSamusX));
}

float GameView::SamusScreenY() const {
    return ScreenYOf(Peek(Ram::kSamusHi), Peek(Ram::kSamusY));
}

} // namespace Metroid
