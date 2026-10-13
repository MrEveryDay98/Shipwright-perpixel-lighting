#include "GameData.h"

#include "game/data/Data.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <libultraship/libultraship.h>
#include <ship/resource/Resource.h>
#include <ship/resource/ResourceFactoryXML.h>
#include <ship/resource/archive/O2rArchive.h>
#include <tinyxml2.h>
#include <zip.h>

namespace Metroid {

namespace {

constexpr const char* kAppShortName = "metroid64";

// Any of the game's data resources (game/data): the piece it holds, ready to merge into the rest.
class GameDataResource : public Ship::Resource<Game::Data::Assets> {
  public:
    using Resource::Resource;
    Game::Data::Assets assets;
    Game::Data::Assets* GetPointer() override {
        return &assets;
    }
    size_t GetPointerSize() override {
        return sizeof(assets);
    }
};

class ResourceFactoryXMLGameDataV0 : public Ship::ResourceFactoryXML {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override {
        if (!FileHasValidFormatAndReader(file, initData)) {
            return nullptr;
        }
        auto res = std::make_shared<GameDataResource>(initData);
        auto root = std::get<std::shared_ptr<tinyxml2::XMLDocument>>(file->Reader)->FirstChildElement();
        std::string error;
        if (!res->assets.Read(root, error)) {
            SPDLOG_ERROR("{}: {}", initData->Path, error);
            return nullptr;
        }
        return res;
    }
};

std::shared_ptr<Ship::ArchiveManager> Archives() {
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
}

// Writes the files into a new zip in one pass.
bool WriteArchive(const std::string& path, const std::map<std::string, std::string>& files, std::string& error) {
    int zerr = 0;
    zip_t* zip = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &zerr);
    if (zip == nullptr) {
        error = "Could not create " + path;
        return false;
    }
    for (const auto& [name, text] : files) {
        zip_source_t* src = zip_source_buffer(zip, text.data(), text.size(), 0);
        if (src == nullptr || zip_file_add(zip, name.c_str(), src, ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE) < 0) {
            zip_source_free(src);
            zip_discard(zip);
            error = "Could not add " + name + " to " + path;
            return false;
        }
    }
    if (zip_close(zip) < 0) {
        error = std::string("Could not write ") + path + ": " + zip_strerror(zip);
        zip_discard(zip);
        return false;
    }
    return true;
}

// Puts metroid.o2r right after metroid64.o2r, so mods (loaded after it) override its files.
void AddArchiveBeforeMods(const std::string& path) {
    auto archives = Archives();
    auto list = std::make_shared<std::vector<std::shared_ptr<Ship::Archive>>>();
    auto current = archives->GetArchives();
    auto data = std::make_shared<Ship::O2rArchive>(path);
    data->Load();
    bool added = false;
    for (const auto& a : *current) {
        if (std::filesystem::path(a->GetPath()).filename() == "metroid.o2r") {
            continue; // replaced by the new one
        }
        list->push_back(a);
        if (!added) {
            list->push_back(data); // after the first archive (metroid64.o2r)
            added = true;
        }
    }
    if (!added) {
        list->push_back(data);
    }
    archives->SetArchives(list);
}

} // namespace

void RegisterGameDataResourceFactories() {
    auto loader = Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceLoader();
    // One resource type per kind of data, all read by game/data ('M','D' + a number).
    for (uint32_t i = 0; Game::Data::kResourceTypes[i] != nullptr; i++) {
        loader->RegisterResourceFactory(std::make_shared<ResourceFactoryXMLGameDataV0>(), RESOURCE_FORMAT_XML,
                                        Game::Data::kResourceTypes[i], 0x4D440000 + i, 0);
    }
}

std::string GameDataArchivePath() {
    return Ship::Context::GetPathRelativeToAppDirectory("metroid.o2r", kAppShortName);
}

bool GameDataReady() {
    auto file = Archives()->LoadFile("metroid/version");
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        return false;
    }
    std::string text(file->Buffer->begin() + file->BufferOffset, file->Buffer->end());
    tinyxml2::XMLDocument doc;
    if (doc.Parse(text.c_str(), text.size()) != tinyxml2::XML_SUCCESS || doc.FirstChildElement() == nullptr) {
        return false;
    }
    return doc.FirstChildElement()->IntAttribute("Data") == Game::Data::kDataVersion;
}

bool ExtractGameData(const std::string& romPath, std::string& error) {
    std::ifstream in(romPath, std::ios::binary);
    if (!in) {
        error = "Could not open " + romPath;
        return false;
    }
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::map<std::string, std::string> files;
    if (!Game::Data::Extract(rom, files, error)) {
        return false;
    }
    std::string path = GameDataArchivePath();
    std::string temp = path + ".tmp";
    if (!WriteArchive(temp, files, error)) {
        return false;
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        error = "Could not write " + path + ": " + ec.message();
        return false;
    }
    SPDLOG_INFO("Extracted the game's data from {} into {} ({} files)", romPath, path, files.size());
    AddArchiveBeforeMods(path);
    return true;
}

bool LoadGameData(std::string& error) {
    auto resources = Ship::Context::GetRawInstance()->GetResourceManager();
    auto paths = Archives()->ListFiles("metroid/*");
    Game::Data::Assets assets;
    for (const std::string& path : *paths) {
        if (path == "metroid/version") {
            continue;
        }
        auto res = std::dynamic_pointer_cast<GameDataResource>(resources->LoadResource(path));
        if (res == nullptr) {
            error = "Could not load " + path;
            return false;
        }
        assets.Merge(res->assets);
    }
    if (assets.blocks.empty()) {
        error = "No game data in the archives";
        return false;
    }
    return Game::Data::Install(assets, error);
}

} // namespace Metroid
