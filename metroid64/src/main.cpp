// Metroid64: NES Metroid in 2.5D on libultraship (Shipwright's engine). The game is a native C++
// port of the original (game/), reading its data from the player's ROM; Metroid64 presents it in 3D.

// We provide our own main(); stop SDL from renaming it to SDL_main on Windows.
#define SDL_MAIN_HANDLED

#include "GameData.h"
#include "Port.h"
#include "Hud.h"
#include "Menu.h"
#include "Renderer.h"
#include "Skeleton.h"

#include <fast/Fast3dWindow.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/factory/LightFactory.h>
#include <fast/resource/factory/MatrixFactory.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/factory/VertexFactory.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/controller/controldeck/ControlDeck.h>
#include <libultraship/libultraship.h>
#include <libultraship/window/gui/InputEditorWindow.h>
#include <ship/Context.h>
#include <ship/audio/Audio.h>
#include <ship/resource/ResourceManager.h>
#include <ship/window/FileDropMgr.h>
#include <ship/window/gui/FileBrowserWindow.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr const char* kAppName = "Metroid64";
constexpr const char* kAppShortName = "metroid64";
constexpr const char* kRomPathConfigKey = "Metroid64.RomPath";
constexpr double kTickSeconds = 1.0 / 60.0;

std::unique_ptr<Metroid::Port> sGame;
Metroid::Renderer sRenderer;
std::shared_ptr<Metroid::Hud> sHud;
bool sPlaying = false;
bool sFrozen = false;

// Starts the game from the game's data in the archives.
bool StartGame() {
    std::string error;
    auto game = std::make_unique<Metroid::Port>();
    if (!game->LoadGameData(error)) {
        SPDLOG_WARN("Game data not usable: {}", error);
        sHud->SetRomError(error);
        return false;
    }
    sGame = std::move(game);
    sGame->PowerOn();
    sRenderer.Init();
    sHud->SetRunning(true);
    sPlaying = true;
    return true;
}

// First run (or new data version): makes metroid.o2r from the ROM, then starts the game. The ROM isn't
// needed after that.
bool TryLoadRom(const std::string& path) {
    std::string error;
    if (!Metroid::ExtractGameData(path, error)) {
        SPDLOG_WARN("ROM {} rejected: {}", path, error);
        sHud->SetRomError(error);
        return false;
    }
    auto config = Ship::Context::GetRawInstance()->GetConfig();
    config->SetString(kRomPathConfigKey, std::filesystem::absolute(path).string());
    config->Save();
    SPDLOG_INFO("Game data made from the ROM at {}", path);
    return StartGame();
}

bool OnFileDropped(char* path) {
    return TryLoadRom(path);
}

void OpenRomDialog() {
    Ship::FileBrowserRequest request;
    request.Title = "Open Metroid (USA) ROM";
    request.Filters = { { "NES ROMs", { "*.nes" } }, { "All files", { "*" } } };
    request.OnResult = [](std::optional<std::filesystem::path> path) {
        if (path) {
            TryLoadRom(path->string());
        }
    };
    Ship::FileBrowserWindow::Open(request);
}

void FindRom() {
    auto config = Ship::Context::GetRawInstance()->GetConfig();
    std::string saved = config->GetString(kRomPathConfigKey, "");
    if (!saved.empty() && TryLoadRom(saved)) {
        return;
    }
    std::vector<std::filesystem::path> dirs = { Ship::Context::GetAppDirectoryPath(kAppShortName),
                                                Ship::Context::GetAppBundlePath(), "." };
    for (const auto& dir : dirs) {
        std::error_code ec;
        if (dir.empty() || !std::filesystem::is_directory(dir, ec)) {
            continue;
        }
        for (const char* name : { "metroid.nes", "Metroid (USA).nes", "Metroid.nes" }) {
            if (std::filesystem::exists(dir / name, ec) && TryLoadRom((dir / name).string())) {
                return;
            }
        }
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (entry.path().extension() == ".nes" && TryLoadRom(entry.path().string())) {
                return;
            }
        }
    }
}

// metroid64.o2r, the game's data (metroid.o2r) and anything in mods/ (archives or plain folders), in
// that order: later archives override earlier ones.
std::vector<std::string> CollectArchives() {
    std::vector<std::string> archives = { Ship::Context::LocateFileAcrossAppDirs("metroid64.o2r", kAppShortName) };
    // The game's data, made from the ROM on first run.
    std::error_code dataEc;
    if (std::filesystem::exists(Metroid::GameDataArchivePath(), dataEc)) {
        archives.push_back(Metroid::GameDataArchivePath());
    }
    std::filesystem::path mods = Ship::Context::GetPathRelativeToAppDirectory("mods", kAppShortName);
    std::error_code ec;
    if (!std::filesystem::is_directory(mods, ec)) {
        std::filesystem::create_directories(mods, ec);
        return archives;
    }
    std::vector<std::string> found;
    for (const auto& entry : std::filesystem::directory_iterator(mods, ec)) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (entry.is_directory() || ext == ".o2r" || ext == ".otr" || ext == ".zip") {
            found.push_back(entry.path().string());
        }
    }
    std::sort(found.begin(), found.end());
    archives.insert(archives.end(), found.begin(), found.end());
    return archives;
}

void RegisterResourceFactories() {
    auto loader = Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceLoader();
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 1);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryVertexV0>(), RESOURCE_FORMAT_BINARY,
                                    "Vertex", static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLVertexV0>(), RESOURCE_FORMAT_XML, "Vertex",
                                    static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>(),
                                    RESOURCE_FORMAT_BINARY, "DisplayList",
                                    static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLDisplayListV0>(), RESOURCE_FORMAT_XML,
                                    "DisplayList", static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryMatrixV0>(), RESOURCE_FORMAT_BINARY,
                                    "Matrix", static_cast<uint32_t>(Fast::ResourceType::Matrix), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryLightV0>(), RESOURCE_FORMAT_BINARY,
                                    "Light", static_cast<uint32_t>(Fast::ResourceType::Light), 0);
}

// Reads the pad as NES buttons. Presses are kept until a game frame consumes them, so short
// taps aren't lost on frames where no tick runs (high refresh rates).
struct InputState {
    uint8_t held = 0;
    uint8_t pressed = 0;

    void Poll() {
        OSContPad pads[4];
        osContGetReadData(pads);
        const OSContPad& p = pads[0];
        uint8_t b = 0;
        b |= (p.stick_x < -30 || (p.button & BTN_DLEFT)) ? Metroid::BtnLeft : 0;
        b |= (p.stick_x > 30 || (p.button & BTN_DRIGHT)) ? Metroid::BtnRight : 0;
        b |= (p.stick_y > 40 || (p.button & BTN_DUP)) ? Metroid::BtnUp : 0;
        b |= (p.stick_y < -40 || (p.button & BTN_DDOWN)) ? Metroid::BtnDown : 0;
        b |= (p.button & BTN_A) ? Metroid::BtnA : 0;
        b |= (p.button & BTN_B) ? Metroid::BtnB : 0;
        b |= (p.button & (BTN_Z | BTN_R)) ? Metroid::BtnSelect : 0;
        b |= (p.button & BTN_START) ? Metroid::BtnStart : 0;
        pressed |= b & ~held;
        held = b;
    }

    uint8_t Take() {
        uint8_t b = held | pressed;
        pressed = 0;
        return b;
    }
};

// Sends the NES's audio to the output device. Audio is dropped while the device already holds plenty,
// which keeps latency bounded if the game ever runs ahead.
void PlayAudio(std::vector<int16_t>& samples) {
    auto audio = Ship::Context::GetRawInstance()->GetAudio();
    auto player = audio ? audio->GetAudioPlayer() : nullptr;
    if (player == nullptr || !player->IsInitialized() || samples.empty()) {
        samples.clear();
        return;
    }
    float volume = CVarGetInteger(Metroid::kCVarMute, 0) ? 0.0f : CVarGetFloat(Metroid::kCVarVolume, 0.8f);
    if (player->Buffered() < Nes::Apu::kSampleRate / 8) {
        std::vector<int16_t> stereo(samples.size() * 2);
        for (size_t i = 0; i < samples.size(); i++) {
            stereo[i * 2] = stereo[i * 2 + 1] = (int16_t)(samples[i] * volume);
        }
        player->Play((const uint8_t*)stereo.data(), stereo.size() * sizeof(int16_t));
    }
    samples.clear();
}

Gfx* BuildIdleFrame() {
    // Nothing to draw while waiting for a ROM; Fast3D clears the frame to black.
    static Gfx dl[2];
    Gfx* g = dl;
    gDPFullSync(g++);
    gSPEndDisplayList(g++);
    return dl;
}

int32_t DesiredFps(const std::shared_ptr<Fast::Fast3dWindow>& window) {
    if (!CVarGetInteger(Metroid::kCVarInterpolation, 1)) {
        return 60;
    }
    if (CVarGetInteger(Metroid::kCVarMatchRefreshRate, 1)) {
        return std::max<int32_t>(60, window->GetCurrentRefreshRate());
    }
    return std::clamp(CVarGetInteger(Metroid::kCVarTargetFps, 60), 30, 360);
}

} // namespace

int main(int argc, char** argv) {
    auto* context = Ship::Context::CreateUninitializedInstance(kAppName, kAppShortName, "metroid64.json");
    context->InitConfiguration();
    context->InitConsoleVariables();
    context->InitLogging(spdlog::level::debug, spdlog::level::info);

    context->InitControlDeck(std::make_shared<LUS::ControlDeck>());
    context->InitResourceManager(CollectArchives(), {}, 1, false);
    RegisterResourceFactories();
    Metroid::RegisterSkeletonResourceFactories();
    Metroid::RegisterGameDataResourceFactories();
    context->InitConsole();

    sHud = std::make_shared<Metroid::Hud>();
    auto inputEditor =
        std::make_shared<LUS::InputEditorWindow>(Metroid::LusCVar::kInputEditorOpen, "Input Editor", ImVec2(820, 630));
    auto window =
        std::make_shared<Fast::Fast3dWindow>(std::vector<std::shared_ptr<Ship::GuiWindow>>({ sHud, inputEditor }));
    context->InitWindow(window);
    Ship::AudioSettings audio;
    audio.SampleRate = Nes::Apu::kSampleRate;
    context->InitAudio(audio);
    context->InitFileDropMgr();
    context->GetFileDropMgr()->RegisterDropHandler(OnFileDropped);
    sHud->Show();

    Metroid::Menu::Actions actions;
    actions.openRom = OpenRomDialog;
    actions.reset = []() {
        if (sPlaying) {
            sGame->PowerOn();
        }
    };
    actions.loaded = []() { return sPlaying; };
    actions.frozen = &sFrozen;
    window->GetGui()->SetMenuBar(std::make_shared<Metroid::Menu>(actions));
    window->SetTextureFilter((Fast::FilteringMode)CVarGetInteger(Metroid::LusCVar::kTextureFilter, Fast::FILTER_NONE));

    OSMesgQueue queue;
    uint8_t controllerBits;
    OSContStatus status[4] = {};
    osContInit(&queue, &controllerBits, status);

    if (argc > 1) {
        TryLoadRom(argv[1]); // a ROM given on the command line remakes the game's data
    }
    if (!sPlaying && Metroid::GameDataReady()) {
        StartGame();
    }
    if (!sPlaying) {
        FindRom();
    }

    // Optional automation for headless smoke tests: quit after N rendered frames.
    int frameLimit = getenv("METROID64_FRAMES") ? atoi(getenv("METROID64_FRAMES")) : -1;

    // Optional automation for headless tests: NES buttons (hex) to hold from a given frame, e.g.
    // "120:08,125:00" presses Start at frame 120 and releases it at 125.
    std::vector<std::pair<int, uint8_t>> script;
    if (const char* s = getenv("METROID64_INPUT")) {
        for (const char* p = s; *p;) {
            int f = atoi(p);
            const char* colon = strchr(p, ':');
            if (colon == nullptr) {
                break;
            }
            script.emplace_back(f, (uint8_t)strtol(colon + 1, nullptr, 16));
            const char* comma = strchr(colon, ',');
            p = comma ? comma + 1 : colon + strlen(colon);
        }
    }
    uint8_t scripted = 0;
    int ticks = 0;
    // Stops the game at this tick (for comparing screenshots).
    int freezeAt = getenv("METROID64_FREEZE_AT") ? atoi(getenv("METROID64_FREEZE_AT")) : -1;

    // The NES runs at a fixed 60 Hz; frames in between are interpolated.
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    double accumulator = 0.0;
    int32_t appliedFps = -1;
    InputState input;

    for (int frame = 0; window->IsRunning() && frame != frameLimit; frame++) {
        window->HandleEvents();
        int32_t fps = DesiredFps(window);
        if (fps != appliedFps) {
            window->SetTargetFps(fps);
            appliedFps = fps;
        }

        auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        input.Poll();

        Gfx* dl;
        sHud->SetFrozen(sFrozen);
        if (sPlaying) {
            auto tick = [&]() {
                for (const auto& [f, b] : script) {
                    if (f == ticks) {
                        scripted = b;
                    }
                }
                if (ticks == freezeAt) {
                    sFrozen = true;
                    return;
                }
                sGame->Tick(input.Take() | scripted);
                ticks++;
                if (frameLimit >= 0 && ticks % 60 == 0) {
                    SPDLOG_INFO("tick {} 1D={:02X} 1E={:02X} gameplay={} ctrl={:02X} mirror={} scroll={},{}", ticks,
                                sGame->Peek(0x1D), sGame->Peek(0x1E), sGame->InGameplay(), sGame->PpuCtrl(),
                                sGame->Mirroring(), sGame->PlaneScrollX(), sGame->PlaneScrollY());
                    SPDLOG_INFO("  samus screen {},{} hi={:02X} x={:02X} y={:02X}", sGame->SamusScreenX(),
                                sGame->SamusScreenY(), sGame->Peek(0x30C), sGame->Peek(0x30E), sGame->Peek(0x30D));
                }
            };
            bool interpolate = CVarGetInteger(Metroid::kCVarInterpolation, 1) != 0;
            float alpha = 1.0f;
            if (sFrozen) {
                accumulator = 0;
            } else if (interpolate && frameLimit < 0) {
                accumulator += std::min(dt, 0.25);
                for (int n = 0; accumulator >= kTickSeconds && n < 5; n++) {
                    tick();
                    accumulator -= kTickSeconds;
                }
                accumulator = std::min(accumulator, kTickSeconds);
                alpha = (float)(accumulator / kTickSeconds);
            } else if (!sFrozen) {
                // One tick per frame (also used for deterministic headless runs).
                tick();
            }
            PlayAudio(sGame->AudioSamples());
            dl = sRenderer.BuildFrame(*sGame, alpha);
        } else {
            dl = BuildIdleFrame();
        }
        window->DrawAndRunGraphicsCommands(dl, {});
    }

    context->GetConfig()->Save();
    return 0;
}
