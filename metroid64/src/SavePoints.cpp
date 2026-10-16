#include "SavePoints.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <spdlog/spdlog.h>
#include <string>

namespace Metroid {

namespace {

constexpr const char* kPath = "metroid64/savepoints.json";
std::vector<SavePoint> sPoints;

int AreaBank(const nlohmann::json& v) {
    if (v.is_number_integer()) {
        int b = v.get<int>();
        return b >= 1 && b <= 5 ? b : 0;
    }
    if (!v.is_string()) {
        return 0;
    }
    static const char* const kNames[] = { "", "brinstar", "norfair", "tourian", "kraid", "ridley" };
    std::string s = v.get<std::string>();
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    for (int b = 1; b <= 5; b++) {
        if (s == kNames[b]) {
            return b;
        }
    }
    return 0;
}

} // namespace

void LoadSavePoints() {
    sPoints.clear();
    auto ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return;
    }
    auto archives = ctx->GetResourceManager()->GetArchiveManager();
    if (!archives->HasFile(kPath)) {
        return;
    }
    auto file = archives->LoadFile(kPath);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        return;
    }
    nlohmann::json json;
    try {
        const char* begin = file->Buffer->data() + file->BufferOffset;
        const char* end = file->Buffer->data() + file->Buffer->size();
        end = std::find(begin, end, '\0');
        json = nlohmann::json::parse(begin, end);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("{} is not valid JSON: {}", kPath, e.what());
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
        s.bank = AreaBank(p.value("area", nlohmann::json()));
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
