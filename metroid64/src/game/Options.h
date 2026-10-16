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

} // namespace Game::Options
