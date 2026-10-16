#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace Metroid {

// Reads a JSON file from the loaded archives (the game's data and mods; the last one loaded wins). False if
// there is no such file or it isn't valid JSON (logged).
bool LoadArchiveJson(const std::string& path, nlohmann::json& out);

// The area bank (1 Brinstar ... 5 Ridley) for "brinstar" ... "ridley" or a number 1-5; 0 if neither.
int AreaBankFromJson(const nlohmann::json& v);

} // namespace Metroid
