#pragma once

#include "Game.h"
#include "Gbi.h"
#include "Mat4.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Ship {
class IResource;
}

namespace Metroid {

class Renderer;

// Where and how to draw one character this frame.
struct VisualInstance {
    Pose pose = Pose::Stand;
    int ticks = 0;      // ticks since the pose started
    float x = 0, y = 0; // anchor in world pixels (NES coordinates, y down)
    int facing = 1;
    float angle = 0; // degrees, rotation in the play plane
    uint32_t tint = 0xFFFFFFFF;
};

// Something that can draw a character or object: a flat ROM sprite today, a 3D model when one is
// provided. Gameplay only ever hands out poses, so the two are interchangeable.
class Visual {
  public:
    virtual ~Visual() = default;
    virtual void Draw(Renderer& r, Gfx*& g, const VisualInstance& v) = 0;
};

// ROM sprite frames drawn as flat, camera-facing cards (no extrusion).
class SpriteVisual : public Visual {
  public:
    enum class Anchor { Feet, Centre };

    explicit SpriteVisual(Anchor anchor) : mAnchor(anchor) {
    }
    // frames are drawn facing right; when facing left they are mirrored unless a left-facing
    // set is given.
    void SetAnim(Pose pose, std::vector<const SpriteFrame*> frames, int ticksPerFrame,
                 std::vector<const SpriteFrame*> leftFrames = {});
    void Draw(Renderer& r, Gfx*& g, const VisualInstance& v) override;

  private:
    struct Anim {
        std::vector<const SpriteFrame*> right, left;
        int ticksPerFrame = 6;
        int bottom = 0; // lowest opaque row of the first frame, for feet anchoring
    };
    Anchor mAnchor;
    std::map<Pose, Anim> mAnims;
};

// A model made of display-list resources (e.g. exported with Fast64), described by a JSON
// manifest inside any loaded archive or the mods/ folder:
//
//   {
//     "scale": 1.0, "offset": [0, 0, 0],      // world pixels; offset y is up
//     "yaw_right": 90, "yaw_left": -90,        // degrees around the vertical axis per facing
//     "fallback": "Stand",
//     "animations": {
//       "Run": { "fps": 12, "loop": true, "frames": ["models/samus/run_0", "models/samus/run_1"] },
//       ...
//     }
//   }
//
// Animation names are the Pose names (Stand, Run, Jump, Spin, Morph, RunShoot, JumpShoot, StandUp,
// RunUp, JumpUp, Front, Move). Each frame is a DisplayList resource path.
class ModelVisual : public Visual {
  public:
    static std::shared_ptr<ModelVisual> Load(const std::string& manifestPath);
    void Draw(Renderer& r, Gfx*& g, const VisualInstance& v) override;

  private:
    struct Anim {
        std::vector<Gfx*> frames;
        float fps = 10;
        bool loop = true;
    };
    const Anim* Find(Pose pose) const;

    std::map<std::string, Anim> mAnims;
    std::string mFallback = "Stand";
    float mScale = 1.0f;
    Vec3 mOffset{};
    float mYawRight = 0, mYawLeft = 180;
    std::vector<std::shared_ptr<Ship::IResource>> mResources; // keep the display lists alive
};

// Picks a model when the archives provide one, otherwise builds the ROM sprite visual.
class VisualLibrary {
  public:
    void Init(const Rom* rom);
    Visual* Samus();
    Visual* Enemy(Area area, uint8_t type);
    Visual* PowerUp(uint8_t type);

  private:
    std::shared_ptr<Visual> TryModel(const std::string& manifest);

    const Rom* mRom = nullptr;
    std::shared_ptr<Visual> mSamus;
    std::map<int, std::shared_ptr<Visual>> mEnemies;
    std::map<int, std::shared_ptr<Visual>> mPowerUps;
};

} // namespace Metroid
