#include "ArchiveJson.h"

#include <algorithm>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <spdlog/spdlog.h>

namespace Metroid {

bool LoadArchiveJson(const std::string& path, nlohmann::json& out) {
    auto ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return false;
    }
    auto archives = ctx->GetResourceManager()->GetArchiveManager();
    if (!archives->HasFile(path)) {
        return false;
    }
    auto file = archives->LoadFile(path);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        return false;
    }
    try {
        const char* begin = file->Buffer->data() + file->BufferOffset;
        const char* end = file->Buffer->data() + file->Buffer->size();
        end = std::find(begin, end, '\0'); // archives may hand back a trailing NUL
        out = nlohmann::json::parse(begin, end);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("{} is not valid JSON: {}", path, e.what());
        return false;
    }
    return true;
}

int AreaBankFromJson(const nlohmann::json& v) {
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

} // namespace Metroid
