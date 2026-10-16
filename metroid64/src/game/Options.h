#pragma once

// Optional changes to the original (not part of it), set by the app each frame from the menu. All are
// off by default, which is the original game; the lockstep tool runs with them off.

namespace Game::Options {

// No sprite flicker: what the original only draws every other frame (the elevator and Samus riding it, a
// statue blinking before it rises, Samus blinking in at the start) is drawn every frame, and the picture
// shows more than 8 sprites a line. Only the drawing changes: the same sprite frame is drawn again, so
// animations and timers keep their pace. Samus still blinks while she can't be hurt (it tells the player
// something).
extern bool gNoFlicker;

// Shots past the screen edge: the original removes Samus's shots as soon as they leave its 256x240 screen,
// which the wider 3D view shows. With this they fly on up to kPastEdge pixels further, as long as the room
// there is the one the game has loaded on that name table (so they hit its walls), and are drawn there
// (Present::gExtraSprites).
extern bool gShotsPastEdge;
constexpr int kPastEdge = 160;

} // namespace Game::Options
