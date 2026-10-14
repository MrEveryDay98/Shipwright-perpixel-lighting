#include "Model.h"
#include "Renderer.h"

#include <fast/resource/type/DisplayList.h>
#include <nlohmann/json.hpp>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>

namespace Metroid {

const char* SamusAnimName(uint8_t animReset) {
    // Offsets into the object animation table at $8572.
    switch (animReset) {
        case 0x00:
            return "Run";
        case 0x04:
            return "Front";
        case 0x07:
            return "Stand";
        case 0x0C:
            return "Jump";
        case 0x0E:
            return "Spin";
        case 0x16:
            return "Morph";
        case 0x20:
            return "JumpShoot";
        case 0x22:
            return "RunShoot";
        case 0x27:
            return "StandUp";
        case 0x37:
            return "RunUp";
        case 0x3B:
            return "JumpUp";
        default:
            return "Stand";
    }
}

std::shared_ptr<Model> Model::Load(const std::string& manifestPath) {
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

    auto model = std::make_shared<Model>();
    model->mScale = json.value("scale", 1.0f);
    if (json.contains("offset") && json["offset"].is_array() && json["offset"].size() == 3) {
        model->mOffset = { json["offset"][0].get<float>(), json["offset"][1].get<float>(),
                           json["offset"][2].get<float>() };
    }
    model->mYawRight = json.value("yaw_right", 0.0f);
    model->mYawLeft = json.value("yaw_left", 180.0f);
    model->mFallback = json.value("fallback", std::string("Stand"));
    model->mKeepTiles = json.value("keep_tiles", false);

    if (json.contains("skeleton") && json["skeleton"].is_string()) {
        std::string path = json["skeleton"].get<std::string>();
        auto res = ctx->GetResourceManager()->LoadResource(path);
        model->mSkeleton = std::dynamic_pointer_cast<Skeleton>(res);
        if (model->mSkeleton == nullptr) {
            SPDLOG_ERROR("Model {}: {} is not a Skeleton resource", manifestPath, path);
            return nullptr;
        }
    }

    const nlohmann::json animations = json.value("animations", nlohmann::json::object());
    for (auto& [name, anim] : animations.items()) {
        if (!anim.is_object()) {
            continue;
        }
        Anim a;
        a.loop = anim.value("loop", true);
        if (model->mSkeleton != nullptr) {
            // Skeletal: one Animation resource, played at 20 frames per second by default (the rate
            // Shipwright's animations are made for).
            a.fps = anim.value("fps", 20.0f);
            std::string path = anim.value("animation", std::string());
            a.skelAnim = std::dynamic_pointer_cast<Animation>(ctx->GetResourceManager()->LoadResource(path));
            if (a.skelAnim == nullptr || !a.skelAnim->supported) {
                SPDLOG_ERROR("Model {}: {} is not a supported Animation resource", manifestPath, path);
                continue;
            }
            model->mAnims[name] = std::move(a);
            continue;
        }
        a.fps = anim.value("fps", 10.0f);
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
    if (model->mAnims.empty() && model->mSkeleton == nullptr) {
        SPDLOG_ERROR("Model manifest {} has no usable animations", manifestPath);
        return nullptr;
    }
    SPDLOG_INFO("Loaded model {}", manifestPath);
    return model;
}

const Gfx* Model::LimbDisplayList(const std::string& path) const {
    auto it = mLimbLists.find(path);
    if (it != mLimbLists.end()) {
        return it->second;
    }
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path);
    auto dl = std::dynamic_pointer_cast<Fast::DisplayList>(res);
    if (dl == nullptr) {
        SPDLOG_ERROR("Limb display list {} is missing", path);
    } else {
        mResources.push_back(res);
    }
    const Gfx* list = dl ? (const Gfx*)dl->GetRawPointer() : nullptr;
    mLimbLists[path] = list;
    return list;
}

const Model::Anim* Model::Find(const std::string& name) const {
    auto it = mAnims.find(name);
    if (it == mAnims.end()) {
        it = mAnims.find(mFallback);
    }
    if (it == mAnims.end()) {
        it = mAnims.begin();
    }
    return it == mAnims.end() ? nullptr : &it->second;
}

void Model::Draw(Renderer& r, Gfx*& g, const ModelInstance& v) const {
    const Anim* a = Find(v.anim);
    Mat4 m = Scale(mScale, mScale, mScale);
    m = Mul(m, RotateY(v.facingLeft ? mYawLeft : mYawRight));
    m = Mul(m, Translate(v.x + mOffset.x, -v.y + mOffset.y, mOffset.z));

    if (mSkeleton != nullptr) {
        // Without an animation the skeleton stands in its rest pose (each limb at its joint position).
        SkeletonPose pose;
        if (a != nullptr && a->skelAnim != nullptr) {
            pose.Sample(*mSkeleton, *a->skelAnim, v.time * a->fps, a->loop);
        }
        r.DrawSkeleton(g, *mSkeleton, pose, m, [this](const std::string& path) { return LimbDisplayList(path); });
        return;
    }
    if (a == nullptr || a->frames.empty()) {
        return;
    }
    int frame = (int)(v.time * a->fps);
    frame = a->loop ? frame % (int)a->frames.size() : std::min(frame, (int)a->frames.size() - 1);
    r.DrawModel(g, a->frames[frame], m);
}

void ModelLibrary::Reset() {
    mSamusLooked = false;
    mSamus.reset();
    mRooms.clear();
}

const Model* ModelLibrary::Room(uint8_t bank, uint8_t mapX, uint8_t mapY, uint8_t room) {
    static const char* const kAreas[] = { "", "brinstar", "norfair", "tourian", "kraid", "ridley" };
    if (bank < 1 || bank > 5) {
        return nullptr;
    }
    uint32_t key = (bank << 24) | (mapX << 16) | (mapY << 8) | room;
    auto it = mRooms.find(key);
    if (it == mRooms.end()) {
        char path[64];
        snprintf(path, sizeof(path), "rooms/%s/%02X_%02X.json", kAreas[bank], mapX, mapY);
        auto model = Model::Load(path);
        if (model == nullptr) {
            snprintf(path, sizeof(path), "rooms/%s/room_%02X.json", kAreas[bank], room);
            model = Model::Load(path);
        }
        it = mRooms.emplace(key, model).first;
    }
    return it->second.get();
}

const Model* ModelLibrary::Samus() {
    if (!mSamusLooked) {
        mSamusLooked = true;
        mSamus = Model::Load("models/samus.json");
    }
    return mSamus.get();
}

} // namespace Metroid
