#include "Visual.h"
#include "Renderer.h"

#include <fast/resource/type/DisplayList.h>
#include <nlohmann/json.hpp>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Metroid {

namespace {

const char* const kAreaKeys[AREA_COUNT] = { "", "brinstar", "norfair", "tourian", "kraid", "ridley" };

// ROM animation table offsets for each Samus pose, and how many ticks each frame shows.
struct SamusAnimDef {
    Pose pose;
    uint8_t offset;
    int ticksPerFrame;
};
const SamusAnimDef kSamusAnims[] = {
    { Pose::Stand, ANIM_STAND, 8 },
    { Pose::Run, ANIM_RUN, 5 },
    { Pose::Jump, ANIM_JUMP, 8 },
    { Pose::Spin, ANIM_SPIN, 3 },
    { Pose::Morph, ANIM_MORPH, 4 },
    { Pose::RunShoot, ANIM_RUN_SHOOT, 5 },
    { Pose::JumpShoot, ANIM_JUMP_SHOOT, 8 },
    { Pose::StandUp, ANIM_STAND_UP, 8 },
    { Pose::RunUp, ANIM_RUN_UP, 5 },
    { Pose::JumpUp, ANIM_JUMP_UP, 8 },
    { Pose::Front, ANIM_FRONT, 8 },
};

} // namespace

// --- SpriteVisual -----------------------------------------------------------------------------

void SpriteVisual::SetAnim(Pose pose, std::vector<const SpriteFrame*> frames, int ticksPerFrame,
                           std::vector<const SpriteFrame*> leftFrames) {
    Anim a;
    a.right = std::move(frames);
    a.left = std::move(leftFrames);
    a.ticksPerFrame = std::max(1, ticksPerFrame);
    if (!a.right.empty()) {
        a.bottom = a.right[0]->Empty() ? 0 : a.right[0]->maxY;
    }
    mAnims[pose] = std::move(a);
}

void SpriteVisual::Draw(Renderer& r, Gfx*& g, const VisualInstance& v) {
    auto it = mAnims.find(v.pose);
    if (it == mAnims.end()) {
        it = mAnims.begin();
        if (it == mAnims.end()) {
            return;
        }
    }
    const Anim& a = it->second;
    bool useLeft = v.facing < 0 && !a.left.empty();
    const auto& frames = useLeft ? a.left : a.right;
    if (frames.empty()) {
        return;
    }
    const SpriteFrame* f = frames[(v.ticks / a.ticksPerFrame) % frames.size()];
    float ox = v.x, oy = v.y;
    if (mAnchor == Anchor::Feet) {
        oy = v.y - (a.bottom + 1);
    }
    bool mirror = v.facing < 0 && !useLeft;
    r.DrawSprite(g, *f, ox, oy, mirror, v.angle, v.tint);
}

// --- ModelVisual ------------------------------------------------------------------------------

std::shared_ptr<ModelVisual> ModelVisual::Load(const std::string& manifestPath) {
    auto ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return nullptr;
    }
    auto archives = ctx->GetResourceManager()->GetArchiveManager();
    if (!archives->HasFile(manifestPath)) {
        return nullptr;
    }
    auto file = archives->LoadFile(manifestPath);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        return nullptr;
    }
    nlohmann::json json;
    try {
        const char* begin = file->Buffer->data() + file->BufferOffset;
        const char* end = file->Buffer->data() + file->Buffer->size();
        // Archives may hand back a trailing NUL; stop at it.
        end = std::find(begin, end, '\0');
        json = nlohmann::json::parse(begin, end);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Model manifest {} is not valid JSON: {}", manifestPath, e.what());
        return nullptr;
    }
    if (!json.is_object()) {
        SPDLOG_ERROR("Model manifest {} must be a JSON object ({} bytes read)", manifestPath, file->Buffer->size());
        return nullptr;
    }

    auto model = std::make_shared<ModelVisual>();
    model->mScale = json.value("scale", 1.0f);
    if (json.contains("offset") && json["offset"].is_array() && json["offset"].size() == 3) {
        model->mOffset = { json["offset"][0].get<float>(), json["offset"][1].get<float>(),
                           json["offset"][2].get<float>() };
    }
    model->mYawRight = json.value("yaw_right", 0.0f);
    model->mYawLeft = json.value("yaw_left", 180.0f);
    model->mFallback = json.value("fallback", std::string("Stand"));

    const nlohmann::json animations = json.value("animations", nlohmann::json::object());
    for (auto& [name, anim] : animations.items()) {
        if (!anim.is_object()) {
            continue;
        }
        Anim a;
        a.fps = anim.value("fps", 10.0f);
        a.loop = anim.value("loop", true);
        const nlohmann::json frames = anim.value("frames", nlohmann::json::array());
        for (auto& path : frames) {
            if (!path.is_string()) {
                continue;
            }
            auto res = ctx->GetResourceManager()->LoadResource(path.get<std::string>());
            auto dl = std::dynamic_pointer_cast<Fast::DisplayList>(res);
            if (dl == nullptr) {
                SPDLOG_ERROR("Model {}: {} is not a DisplayList resource", manifestPath, path.get<std::string>());
                continue;
            }
            model->mResources.push_back(res);
            a.frames.push_back((Gfx*)dl->GetRawPointer());
        }
        if (!a.frames.empty()) {
            model->mAnims[name] = std::move(a);
        }
    }
    if (model->mAnims.empty()) {
        SPDLOG_ERROR("Model manifest {} has no usable animations", manifestPath);
        return nullptr;
    }
    SPDLOG_INFO("Loaded model {}", manifestPath);
    return model;
}

const ModelVisual::Anim* ModelVisual::Find(Pose pose) const {
    auto it = mAnims.find(PoseName(pose));
    if (it == mAnims.end()) {
        it = mAnims.find(mFallback);
    }
    if (it == mAnims.end()) {
        it = mAnims.begin();
    }
    return &it->second;
}

void ModelVisual::Draw(Renderer& r, Gfx*& g, const VisualInstance& v) {
    const Anim* a = Find(v.pose);
    if (a == nullptr || a->frames.empty()) {
        return;
    }
    int frame = (int)(v.ticks * a->fps / 60.0f);
    frame = a->loop ? frame % (int)a->frames.size() : std::min(frame, (int)a->frames.size() - 1);

    Mat4 m = Scale(mScale, mScale, mScale);
    m = Mul(m, RotateY(v.facing < 0 ? mYawLeft : mYawRight));
    m = Mul(m, RotateZ(v.angle));
    m = Mul(m, Translate(v.x + mOffset.x, -v.y + mOffset.y, mOffset.z));
    r.DrawModel(g, a->frames[frame], m);
}

// --- VisualLibrary ----------------------------------------------------------------------------

std::shared_ptr<Visual> VisualLibrary::TryModel(const std::string& manifest) {
    return ModelVisual::Load(manifest);
}

void VisualLibrary::Init(const Rom* rom) {
    mRom = rom;
    mSamus.reset();
    mEnemies.clear();
    mPowerUps.clear();
}

Visual* VisualLibrary::Samus() {
    if (mSamus == nullptr) {
        mSamus = TryModel("models/samus.json");
        if (mSamus == nullptr) {
            auto sprite = std::make_shared<SpriteVisual>(SpriteVisual::Anchor::Feet);
            for (const SamusAnimDef& d : kSamusAnims) {
                std::vector<const SpriteFrame*> frames;
                for (uint8_t f : mRom->ObjectAnim(d.offset)) {
                    if (f != 0xFF && f < mRom->ObjectFrameCount()) {
                        frames.push_back(&mRom->ObjectFrame(f));
                    }
                }
                sprite->SetAnim(d.pose, std::move(frames), d.ticksPerFrame);
            }
            mSamus = sprite;
        }
    }
    return mSamus.get();
}

Visual* VisualLibrary::Enemy(Area area, uint8_t type) {
    int key = area * 16 + (type & 0x0F);
    auto it = mEnemies.find(key);
    if (it != mEnemies.end()) {
        return it->second.get();
    }
    char manifest[64];
    snprintf(manifest, sizeof(manifest), "models/enemies/%s_%X.json", kAreaKeys[area], type & 0x0F);
    std::shared_ptr<Visual> v = TryModel(manifest);
    if (v == nullptr) {
        auto sprite = std::make_shared<SpriteVisual>(SpriteVisual::Anchor::Centre);
        std::vector<const SpriteFrame*> right, left;
        for (uint8_t f : mRom->EnemyAnim(area, type, true)) {
            right.push_back(&mRom->EnemyFrame(area, f));
        }
        for (uint8_t f : mRom->EnemyAnim(area, type, false)) {
            left.push_back(&mRom->EnemyFrame(area, f));
        }
        sprite->SetAnim(Pose::Move, std::move(right), 8, std::move(left));
        v = sprite;
    }
    mEnemies[key] = v;
    return v.get();
}

Visual* VisualLibrary::PowerUp(uint8_t type) {
    auto it = mPowerUps.find(type);
    if (it != mPowerUps.end()) {
        return it->second.get();
    }
    char manifest[64];
    snprintf(manifest, sizeof(manifest), "models/items/%d.json", type);
    std::shared_ptr<Visual> v = TryModel(manifest);
    if (v == nullptr) {
        auto sprite = std::make_shared<SpriteVisual>(SpriteVisual::Anchor::Centre);
        int frame = kFramePowerUpBase + type;
        if (frame < mRom->ObjectFrameCount()) {
            sprite->SetAnim(Pose::Stand, { &mRom->ObjectFrame(frame) }, 8);
        }
        v = sprite;
    }
    mPowerUps[type] = v;
    return v.get();
}

} // namespace Metroid
