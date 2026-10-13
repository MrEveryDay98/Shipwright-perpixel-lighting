// Metroid64: a 2.5D remake of NES Metroid running on libultraship (Shipwright's engine).

// We provide our own main(); stop SDL from renaming it to SDL_main on Windows.
#define SDL_MAIN_HANDLED

#include "Game.h"
#include "Hud.h"
#include "Menu.h"
#include "Renderer.h"
#include "Rom.h"

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
#include <ship/resource/ResourceManager.h>
#include <ship/window/FileDropMgr.h>
#include <ship/window/gui/FileBrowserWindow.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <memory>

namespace {

constexpr const char* kAppName = "Metroid64";
constexpr const char* kAppShortName = "metroid64";
constexpr const char* kRomPathConfigKey = "Metroid64.RomPath";
constexpr double kTickSeconds = 1.0 / 60.0;

Metroid::Rom sRom;
Metroid::Game sGame;
Metroid::Renderer sRenderer;
std::shared_ptr<Metroid::Hud> sHud;
bool sPlaying = false;

bool TryLoadRom(const std::string& path) {
    std::string error;
    Metroid::Rom rom;
    if (!rom.Load(path, error)) {
        SPDLOG_WARN("ROM {} rejected: {}", path, error);
        sHud->SetRomError(error);
        return false;
    }
    sRom = std::move(rom);
    sGame.Init(&sRom);
    sRenderer.Init(&sRom);
    sHud->SetGame(&sGame);
    sPlaying = true;
    auto config = Ship::Context::GetRawInstance()->GetConfig();
    config->SetString(kRomPathConfigKey, std::filesystem::absolute(path).string());
    config->Save();
    SPDLOG_INFO("Loaded Metroid ROM from {}", path);
    return true;
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

// metroid64.o2r plus anything in mods/ (archives or plain folders): that is where models go.
std::vector<std::string> CollectArchives() {
    std::vector<std::string> archives = { Ship::Context::LocateFileAcrossAppDirs("metroid64.o2r", kAppShortName) };
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

// Reads the pad and keeps button presses until a game tick consumes them, so presses aren't lost
// on frames where no tick runs (high refresh rates).
struct InputState {
    CONTROLLERBUTTONS_T prevButtons = 0;
    Metroid::Input pending;

    void Poll() {
        OSContPad pads[4];
        osContGetReadData(pads);
        const OSContPad& p = pads[0];
        CONTROLLERBUTTONS_T pressed = p.button & ~prevButtons;
        prevButtons = p.button;

        pending.left = p.stick_x < -30 || (p.button & BTN_DLEFT);
        pending.right = p.stick_x > 30 || (p.button & BTN_DRIGHT);
        pending.up = p.stick_y > 40 || (p.button & BTN_DUP);
        pending.down = p.stick_y < -40 || (p.button & BTN_DDOWN);
        pending.jump = p.button & BTN_A;
        pending.jumpPressed |= (pressed & BTN_A) != 0;
        pending.shootPressed |= (pressed & BTN_B) != 0;
        pending.toggleMissilePressed |= (pressed & (BTN_R | BTN_Z)) != 0;
        pending.startPressed |= (pressed & BTN_START) != 0;
    }

    Metroid::Input Take() {
        Metroid::Input in = pending;
        pending.jumpPressed = pending.shootPressed = pending.toggleMissilePressed = pending.startPressed = false;
        return in;
    }
};

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
    context->InitConsole();

    sHud = std::make_shared<Metroid::Hud>();
    auto inputEditor =
        std::make_shared<LUS::InputEditorWindow>(Metroid::LusCVar::kInputEditorOpen, "Input Editor", ImVec2(820, 630));
    auto window =
        std::make_shared<Fast::Fast3dWindow>(std::vector<std::shared_ptr<Ship::GuiWindow>>({ sHud, inputEditor }));
    context->InitWindow(window);
    context->InitFileDropMgr();
    context->GetFileDropMgr()->RegisterDropHandler(OnFileDropped);
    sHud->Show();

    Metroid::Menu::Actions actions;
    actions.openRom = OpenRomDialog;
    actions.reset = []() {
        if (sPlaying) {
            sGame.Init(&sRom);
        }
    };
    actions.game = []() -> Metroid::Game* { return sPlaying ? &sGame : nullptr; };
    window->GetGui()->SetMenuBar(std::make_shared<Metroid::Menu>(actions));
    window->SetTextureFilter((Fast::FilteringMode)CVarGetInteger(Metroid::LusCVar::kTextureFilter, Fast::FILTER_NONE));

    OSMesgQueue queue;
    uint8_t controllerBits;
    OSContStatus status[4] = {};
    osContInit(&queue, &controllerBits, status);

    if (argc > 1) {
        TryLoadRom(argv[1]);
    }
    if (!sPlaying) {
        FindRom();
    }

    // Optional automation for headless smoke tests: quit after N rendered frames.
    int frameLimit = getenv("METROID64_FRAMES") ? atoi(getenv("METROID64_FRAMES")) : -1;

    // Game logic runs at a fixed 60 Hz like the original; frames in between are interpolated.
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
        if (sPlaying) {
            bool interpolate = CVarGetInteger(Metroid::kCVarInterpolation, 1) != 0;
            float alpha = 1.0f;
            if (interpolate && frameLimit < 0) {
                accumulator += std::min(dt, 0.25);
                for (int ticks = 0; accumulator >= kTickSeconds && ticks < 5; ticks++) {
                    sGame.Update(input.Take());
                    accumulator -= kTickSeconds;
                }
                accumulator = std::min(accumulator, kTickSeconds);
                alpha = (float)(accumulator / kTickSeconds);
            } else {
                // One tick per frame (also used for deterministic headless runs).
                sGame.Update(input.Take());
            }
            dl = sRenderer.BuildFrame(sGame, alpha);
        } else {
            dl = BuildIdleFrame();
        }
        window->DrawAndRunGraphicsCommands(dl, {});
    }

    context->GetConfig()->Save();
    return 0;
}
