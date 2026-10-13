// Metroid64: a 2.5D remake of NES Metroid running on libultraship (Shipwright's engine).

#include "Game.h"
#include "Hud.h"
#include "Renderer.h"
#include "Rom.h"

#include <fast/Fast3dWindow.h>
#include <libultraship/controller/controldeck/ControlDeck.h>
#include <libultraship/libultraship.h>
#include <ship/Context.h>
#include <ship/window/FileDropMgr.h>

#include <filesystem>
#include <memory>

namespace {

constexpr const char* kAppName = "Metroid64";
constexpr const char* kAppShortName = "metroid64";
constexpr const char* kRomPathConfigKey = "Metroid64.RomPath";

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

Metroid::Input ReadInput() {
    static CONTROLLERBUTTONS_T prevButtons = 0;
    OSContPad pads[4];
    osContGetReadData(pads);
    const OSContPad& p = pads[0];
    CONTROLLERBUTTONS_T pressed = p.button & ~prevButtons;
    prevButtons = p.button;

    Metroid::Input in;
    in.left = p.stick_x < -30 || (p.button & BTN_DLEFT);
    in.right = p.stick_x > 30 || (p.button & BTN_DRIGHT);
    in.up = p.stick_y > 40 || (p.button & BTN_DUP);
    in.down = p.stick_y < -40 || (p.button & BTN_DDOWN);
    in.jump = p.button & BTN_A;
    in.jumpPressed = pressed & BTN_A;
    in.shootPressed = pressed & BTN_B;
    in.toggleMissilePressed = pressed & (BTN_R | BTN_Z);
    in.startPressed = pressed & BTN_START;
    return in;
}

Gfx* BuildIdleFrame() {
    // Nothing to draw while waiting for a ROM; Fast3D clears the frame to black.
    static Gfx dl[2];
    Gfx* g = dl;
    gDPFullSync(g++);
    gSPEndDisplayList(g++);
    return dl;
}

} // namespace

int main(int argc, char** argv) {
    auto* context = Ship::Context::CreateUninitializedInstance(kAppName, kAppShortName, "metroid64.json");
    context->InitConfiguration();
    context->InitConsoleVariables();
    context->InitLogging(spdlog::level::debug, spdlog::level::info);

    std::string archive = Ship::Context::LocateFileAcrossAppDirs("metroid64.o2r", kAppShortName);
    context->InitControlDeck(std::make_shared<LUS::ControlDeck>());
    context->InitResourceManager({ archive }, {}, 1, false);
    context->InitConsole();

    sHud = std::make_shared<Metroid::Hud>();
    auto window = std::make_shared<Fast::Fast3dWindow>(std::vector<std::shared_ptr<Ship::GuiWindow>>({ sHud }));
    context->InitWindow(window);
    sHud->Show();
    context->InitFileDropMgr();
    context->GetFileDropMgr()->RegisterDropHandler(OnFileDropped);

    OSMesgQueue queue;
    uint8_t controllerBits;
    OSContStatus status[4] = {};
    osContInit(&queue, &controllerBits, status);
    window->SetTargetFps(60);

    if (argc > 1) {
        TryLoadRom(argv[1]);
    }
    if (!sPlaying) {
        FindRom();
    }

    // Optional automation for headless smoke tests: quit after N frames.
    int frameLimit = getenv("METROID64_FRAMES") ? atoi(getenv("METROID64_FRAMES")) : -1;
    for (int frame = 0; window->IsRunning() && frame != frameLimit; frame++) {
        window->HandleEvents();
        Metroid::Input in = ReadInput();
        Gfx* dl;
        if (sPlaying) {
            sGame.Update(in);
            dl = sRenderer.BuildFrame(sGame);
        } else {
            dl = BuildIdleFrame();
        }
        window->DrawAndRunGraphicsCommands(dl, {});
    }

    context->GetConfig()->Save();
    return 0;
}
