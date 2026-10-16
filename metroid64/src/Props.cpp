#include "Props.h"

#include "ArchiveJson.h"

#include <spdlog/spdlog.h>

namespace Metroid {

namespace {

constexpr const char* kPath = "metroid64/props.json";
std::vector<Prop> sProps;

} // namespace

void LoadProps() {
    sProps.clear();
    nlohmann::json json;
    if (!LoadArchiveJson(kPath, json)) {
        return;
    }
    if (!json.is_object() || !json.contains("props") || !json["props"].is_array()) {
        SPDLOG_ERROR("{} needs a \"props\" array", kPath);
        return;
    }
    for (const auto& p : json["props"]) {
        if (!p.is_object()) {
            continue;
        }
        Prop prop;
        prop.bank = AreaBankFromJson(p.value("area", nlohmann::json()));
        int mx = p.value("map_x", -1), my = p.value("map_y", -1);
        prop.displayList = p.value("display_list", std::string());
        if (prop.bank == 0 || mx < 0 || mx > 31 || my < 0 || my > 31 || prop.displayList.empty()) {
            SPDLOG_ERROR("{}: a prop needs an area, map_x / map_y 0-31 and a display_list", kPath);
            continue;
        }
        prop.mapX = (uint8_t)mx;
        prop.mapY = (uint8_t)my;
        prop.x = p.value("x", 128.0f);
        prop.y = p.value("y", 120.0f);
        prop.z = p.value("z", 0.0f);
        prop.scale = p.value("scale", 1.0f);
        prop.yaw = p.value("yaw", 0.0f);
        sProps.push_back(prop);
    }
    SPDLOG_INFO("{} props from {}", sProps.size(), kPath);
}

const std::vector<Prop>& Props() {
    return sProps;
}

} // namespace Metroid
