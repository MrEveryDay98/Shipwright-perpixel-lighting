#pragma once

#include "Gbi.h"
#include "Mat4.h"
#include "Skeleton.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Ship {
class IResource;
}

namespace Metroid {

class Renderer;

// Where and how to draw a model this frame.
struct ModelInstance {
    std::string anim;   // animation name (see docs/MODELS.md)
    float time = 0;     // seconds since the animation started
    float x = 0, y = 0; // anchor in camera-local pixels (NES orientation, y down)
    bool facingLeft = false;
};

// A model made of display-list resources (e.g. exported with Fast64), described by a JSON
// manifest inside any loaded archive or the mods/ folder:
//
//   {
//     "scale": 1.0, "offset": [0, 0, 0],      // pixels; offset y is up
//     "yaw_right": 90, "yaw_left": -90,        // degrees around the vertical axis per facing
//     "fallback": "Stand",
//     "animations": {
//       "Run": { "fps": 12, "loop": true, "frames": ["models/samus/run_0", "models/samus/run_1"] },
//       ...
//     }
//   }
//
// Each frame is a DisplayList resource path. A skeletal model instead names a Skeleton resource and
// gives each animation an Animation resource (Shipwright's formats, see Skeleton.h):
//
//   { "skeleton": "models/samus/skel",
//     "animations": { "Run": { "animation": "models/samus/anim_run", "fps": 20, "loop": true } } }
class Model {
  public:
    static std::shared_ptr<Model> Load(const std::string& manifestPath);
    void Draw(Renderer& r, Gfx*& g, const ModelInstance& m) const;

  private:
    struct Anim {
        std::vector<Gfx*> frames;             // flipbook
        std::shared_ptr<Animation> skelAnim;  // skeletal
        float fps = 10;
        bool loop = true;
    };
    const Anim* Find(const std::string& name) const;
    const Gfx* LimbDisplayList(const std::string& path) const;

    std::map<std::string, Anim> mAnims;
    std::string mFallback = "Stand";
    float mScale = 1.0f;
    Vec3 mOffset{};
    float mYawRight = 0, mYawLeft = 180;
    std::shared_ptr<Skeleton> mSkeleton;
    mutable std::map<std::string, const Gfx*> mLimbLists;
    mutable std::vector<std::shared_ptr<Ship::IResource>> mResources; // keep the display lists alive
};

// Looks models up once in the loaded archives. A character without a model is drawn with the
// game's own sprites.
class ModelLibrary {
  public:
    void Reset();
    const Model* Samus();

  private:
    bool mSamusLooked = false;
    std::shared_ptr<Model> mSamus;
};

// Animation name for Samus's current ROM animation (the value the game keeps at $0305).
const char* SamusAnimName(uint8_t animReset);

} // namespace Metroid
