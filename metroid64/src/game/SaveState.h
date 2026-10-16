#pragma once

// Saving the game (not part of the original, which has passwords): the whole state of the game at a moment,
// as bytes, and putting it back. The app saves while the game is paused and, on dying, can go back to the
// save the way Super Metroid does.

#include <string>

namespace Game::SaveState {

// Saving is allowed while the game is paused in play (Start), except during the escape (the original
// doesn't let the password screen be reached then either).
bool CanSave();

// The game's memory, picture memory and mapper state, the presentation's bookkeeping (Present.h) and the
// map's visited screens (Explored.h).
std::string Capture();
// Puts a Capture back. False (and nothing changed) if the data isn't one. "Everything active" builds
// the section's roster again, so the section's enemies are back where their rooms place them.
bool Restore(const std::string& data);

} // namespace Game::SaveState
