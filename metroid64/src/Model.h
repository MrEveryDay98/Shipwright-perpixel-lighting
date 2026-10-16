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
    // Smooth animation: the facing as a fraction from right (0) to left (1), the model turning in between
    // (negative: just `facingLeft`); and a pose to blend from, `blend` being the new pose's weight
    // (skeletal models; flipbooks just switch).
    float turn = -1;
    const SkeletonPose* fromPose = nullptr;
    float blend = 1;
    // If set, receives the pose drawn (skeletal models), to blend from later.
    SkeletonPose* drawnPose = nullptr;
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
    // Room geometry only: also draw the game's own tiles (the model adds to them instead of replacing).
    bool KeepTiles() const {
        return mKeepTiles;
    }

  private:
    struct Anim {
        std::vector<Gfx*> frames;            // flipbook
        std::shared_ptr<Animation> skelAnim; // skeletal
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
    bool mKeepTiles = false;
    std::shared_ptr<Skeleton> mSkeleton;
    mutable std::map<std::string, const Gfx*> mLimbLists;
    mutable std::vector<std::shared_ptr<Ship::IResource>> mResources; // keep the display lists alive
};

// Looks models up once in the loaded archives. A character without a model is drawn with the
// game's own sprites.
class ModelLibrary {
  public:
    void Reset();
    // Samus: models/samus.json (the power suit), models/samus_varia.json (with the Varia),
    // models/samus_ball.json (as the morph ball). Variants without a model use the power suit's.
    enum class SamusForm { Power, Varia, Ball };
    const Model* Samus(SamusForm form = SamusForm::Power);
    // Custom geometry for a room of the world map: rooms/<area>/<XX>_<YY>.json for that map position
    // (hex), else rooms/<area>/room_<NN>.json for every place the room is used. Areas: brinstar,
    // norfair, tourian, kraid, ridley.
    const Model* Room(uint8_t bank, uint8_t mapX, uint8_t mapY, uint8_t room);

  private:
    std::map<uint32_t, std::shared_ptr<Model>> mRooms; // looked up (null: none)
    bool mSamusLooked[3] = {};
    std::shared_ptr<Model> mSamus[3];
};

// Animation name for Samus's current ROM animation (the value the game keeps at $0305).
const char* SamusAnimName(uint8_t animReset);

} // namespace Metroid
