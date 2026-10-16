#include "SavePoints.h"

#include "ArchiveJson.h"

#include <spdlog/spdlog.h>

namespace Metroid {

namespace {

constexpr const char* kPath = "metroid64/savepoints.json";
std::vector<SavePoint> sPoints;

} // namespace

void LoadSavePoints() {
    sPoints.clear();
    nlohmann::json json;
    if (!LoadArchiveJson(kPath, json)) {
        return;
    }
    if (!json.is_object() || !json.contains("save_points") || !json["save_points"].is_array()) {
        SPDLOG_ERROR("{} needs a \"save_points\" array", kPath);
        return;
    }
    for (const auto& p : json["save_points"]) {
        if (!p.is_object()) {
            continue;
        }
        SavePoint s;
        s.bank = AreaBankFromJson(p.value("area", nlohmann::json()));
        int mx = p.value("map_x", -1), my = p.value("map_y", -1);
        s.x = p.value("x", 128);
        s.y = p.value("y", 176);
        if (s.bank == 0 || mx < 0 || mx > 31 || my < 0 || my > 31) {
            SPDLOG_ERROR("{}: a save point needs an area and map_x / map_y 0-31", kPath);
            continue;
        }
        s.mapX = (uint8_t)mx;
        s.mapY = (uint8_t)my;
        sPoints.push_back(s);
    }
    SPDLOG_INFO("{} save points from {}", sPoints.size(), kPath);
}

const std::vector<SavePoint>& SavePoints() {
    return sPoints;
}

} // namespace Metroid
