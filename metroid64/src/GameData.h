#pragma once

// The game's data in the app: metroid.o2r, made from the player's ROM on first run, holds every table,
// room, graphic and piece of music as resources (game/data). Mods override them like any other file.

#include <string>

namespace Metroid {

void RegisterGameDataResourceFactories();

// Where metroid.o2r goes (the app's folder).
std::string GameDataArchivePath();

// True when the loaded archives hold the game's data, extracted by this version of Metroid64.
bool GameDataReady();

// Makes metroid.o2r from the ROM at romPath and puts it in front of the mods. False, with a reason, if
// the ROM isn't Metroid (USA) or the archive can't be written.
bool ExtractGameData(const std::string& romPath, std::string& error);

// Builds the game's data from the resources (the extracted data plus any mods).
bool LoadGameData(std::string& error);

} // namespace Metroid
