#pragma once

// The sound engine at $B3B4. Every bank carries an identical copy of its code; the music and sound
// effect data it plays come from whichever bank is mapped.

namespace Game {

void SoundEngine(); // $B3B4, called every NMI

} // namespace Game
