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

const Model::Anim* Model::Find(const std::string& name) const {
    auto it = mAnims.find(name);
    if (it == mAnims.end()) {
        it = mAnims.find(mFallback);
    }
    if (it == mAnims.end()) {
        it = mAnims.begin();
    }
    return &it->second;
}

void Model::Draw(Renderer& r, Gfx*& g, const ModelInstance& v) const {
    const Anim* a = Find(v.anim);
    if (a == nullptr || a->frames.empty()) {
        return;
    }
    int frame = (int)(v.ticks * a->fps / 60.0f);
    frame = a->loop ? frame % (int)a->frames.size() : std::min(frame, (int)a->frames.size() - 1);

    Mat4 m = Scale(mScale, mScale, mScale);
    m = Mul(m, RotateY(v.facingLeft ? mYawLeft : mYawRight));
    m = Mul(m, Translate(v.x + mOffset.x, -v.y + mOffset.y, mOffset.z));
    r.DrawModel(g, a->frames[frame], m);
}

void ModelLibrary::Reset() {
    mSamusLooked = false;
    mSamus.reset();
}

const Model* ModelLibrary::Samus() {
    if (!mSamusLooked) {
        mSamusLooked = true;
        mSamus = Model::Load("models/samus.json");
    }
    return mSamus.get();
}

} // namespace Metroid
