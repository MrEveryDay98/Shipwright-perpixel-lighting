#include "Skeleton.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/utils/binarytools/BinaryReader.h>
#include <spdlog/spdlog.h>
#include <tinyxml2.h>

namespace Metroid {

namespace {

// Shipwright marks resource paths inside display lists with this prefix.
std::string StripOtrPrefix(std::string path) {
    static const std::string kPrefix = "__OTR__";
    if (path.compare(0, kPrefix.size(), kPrefix) == 0) {
        path.erase(0, kPrefix.size());
    }
    return path;
}

std::shared_ptr<SkeletonLimb> LoadLimb(const std::string& path) {
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(path);
    auto limb = std::dynamic_pointer_cast<SkeletonLimb>(res);
    if (limb == nullptr) {
        SPDLOG_ERROR("Skeleton limb {} is missing or not a SkeletonLimb", path);
    }
    return limb;
}

float BinaryAngleToDegrees(int16_t a) {
    return a * (360.0f / 65536.0f);
}

} // namespace

// --- Skeleton ---------------------------------------------------------------------------------

std::shared_ptr<Ship::IResource>
ResourceFactoryBinarySkeletonV0::ReadResource(std::shared_ptr<Ship::File> file,
                                              std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    auto skel = std::make_shared<Skeleton>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    skel->type = (SkeletonType)reader->ReadInt8();
    skel->limbType = (LimbType)reader->ReadInt8();
    skel->limbCount = reader->ReadUInt32();
    skel->dListCount = reader->ReadUInt32();
    reader->ReadInt8(); // limb table type
    uint32_t tableCount = reader->ReadUInt32();
    for (uint32_t i = 0; i < tableCount; i++) {
        skel->limbPaths.push_back(StripOtrPrefix(reader->ReadString()));
    }
    for (const std::string& path : skel->limbPaths) {
        skel->limbs.push_back(LoadLimb(path));
    }
    if (skel->type == SkeletonType::Curve) {
        SPDLOG_WARN("Curve skeletons are not supported");
    }
    return skel;
}

std::shared_ptr<Ship::IResource>
ResourceFactoryXMLSkeletonV0::ReadResource(std::shared_ptr<Ship::File> file,
                                           std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    auto skel = std::make_shared<Skeleton>(initData);
    auto root = std::get<std::shared_ptr<tinyxml2::XMLDocument>>(file->Reader)->FirstChildElement();

    skel->type = SkeletonType::Flex; // Shipwright's default
    if (const char* type = root->Attribute("Type")) {
        std::string t = type;
        skel->type = t == "Normal" ? SkeletonType::Normal : t == "Curve" ? SkeletonType::Curve : SkeletonType::Flex;
    }
    skel->limbType = LimbType::LOD;
    if (const char* type = root->Attribute("LimbType")) {
        std::string t = type;
        skel->limbType = t == "Standard" ? LimbType::Standard : LimbType::LOD;
    }
    skel->limbCount = root->IntAttribute("LimbCount");
    skel->dListCount = root->IntAttribute("DisplayListCount");
    for (auto child = root->FirstChildElement(); child != nullptr; child = child->NextSiblingElement()) {
        if (std::string(child->Name()) == "SkeletonLimb" && child->Attribute("Path")) {
            skel->limbPaths.push_back(StripOtrPrefix(child->Attribute("Path")));
        }
    }
    for (const std::string& path : skel->limbPaths) {
        skel->limbs.push_back(LoadLimb(path));
    }
    return skel;
}

// --- Limbs ------------------------------------------------------------------------------------

std::shared_ptr<Ship::IResource>
ResourceFactoryBinarySkeletonLimbV0::ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    auto limb = std::make_shared<SkeletonLimb>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    limb->type = (LimbType)reader->ReadInt8();
    reader->ReadInt8();   // skin segment type
    reader->ReadString(); // skin display list
    reader->ReadUInt16(); // skin vertex count
    uint32_t modifCount = reader->ReadUInt32();
    for (uint32_t i = 0; i < modifCount; i++) {
        reader->ReadUInt16();
        int32_t vtxCount = reader->ReadInt32();
        for (int32_t k = 0; k < vtxCount; k++) {
            reader->ReadInt16();
            reader->ReadInt16();
            reader->ReadInt16();
            reader->ReadInt8();
            reader->ReadInt8();
            reader->ReadInt8();
            reader->ReadUByte();
        }
        int32_t transformCount = reader->ReadInt32();
        for (int32_t k = 0; k < transformCount; k++) {
            reader->ReadUByte();
            reader->ReadInt16();
            reader->ReadInt16();
            reader->ReadInt16();
            reader->ReadUByte();
        }
    }
    reader->ReadString(); // skin display list 2
    reader->ReadFloat();  // legacy translation
    reader->ReadFloat();
    reader->ReadFloat();
    reader->ReadUInt16(); // legacy rotation
    reader->ReadUInt16();
    reader->ReadUInt16();
    reader->ReadString(); // child limb path
    reader->ReadString(); // sibling limb path
    std::string dList = reader->ReadString();
    reader->ReadString(); // far LOD display list
    limb->jointPos[0] = reader->ReadInt16();
    limb->jointPos[1] = reader->ReadInt16();
    limb->jointPos[2] = reader->ReadInt16();
    limb->child = reader->ReadUByte();
    limb->sibling = reader->ReadUByte();

    if (limb->type == LimbType::Standard || limb->type == LimbType::LOD) {
        limb->dList = StripOtrPrefix(dList);
    } else {
        SPDLOG_WARN("Skeleton limb type {} is not supported; drawing it empty", (int)limb->type);
    }
    return limb;
}

std::shared_ptr<Ship::IResource>
ResourceFactoryXMLSkeletonLimbV0::ReadResource(std::shared_ptr<Ship::File> file,
                                               std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    auto limb = std::make_shared<SkeletonLimb>(initData);
    auto root = std::get<std::shared_ptr<tinyxml2::XMLDocument>>(file->Reader)->FirstChildElement();

    limb->type = LimbType::LOD;
    limb->jointPos[0] = (int16_t)root->FloatAttribute("LegTransX");
    limb->jointPos[1] = (int16_t)root->FloatAttribute("LegTransY");
    limb->jointPos[2] = (int16_t)root->FloatAttribute("LegTransZ");
    limb->child = (uint8_t)root->IntAttribute("ChildIndex", 0xFF);
    limb->sibling = (uint8_t)root->IntAttribute("SiblingIndex", 0xFF);
    const char* dl = root->Attribute("DisplayList1");
    if (dl != nullptr && std::string(dl) != "gEmptyDL") {
        limb->dList = StripOtrPrefix(dl);
    }
    return limb;
}

// --- Animations -------------------------------------------------------------------------------

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryAnimationV0::ReadResource(std::shared_ptr<Ship::File> file,
                                               std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    auto anim = std::make_shared<Animation>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    uint32_t type = reader->ReadUInt32();
    if (type != 0) { // 0 = Normal; Link, Curve and Legacy animations are OoT-specific
        SPDLOG_WARN("Animation type {} is not supported", type);
        return anim;
    }
    anim->frameCount = reader->ReadInt16();
    uint32_t valueCount = reader->ReadUInt32();
    anim->frameData.reserve(valueCount);
    for (uint32_t i = 0; i < valueCount; i++) {
        anim->frameData.push_back((int16_t)reader->ReadUInt16());
    }
    uint32_t indexCount = reader->ReadUInt32();
    anim->jointIndices.reserve(indexCount * 3);
    for (uint32_t i = 0; i < indexCount * 3; i++) {
        anim->jointIndices.push_back(reader->ReadUInt16());
    }
    anim->staticIndexMax = (uint16_t)reader->ReadInt16();
    anim->supported = true;
    return anim;
}

// <Animation FrameCount="20" StaticIndexMax="4">
//   <Values>0 0 0 16384 ...</Values>          frame data (s16)
//   <JointIndices>0 1 2 3 4 5 ...</JointIndices> x y z per joint (root translation first)
// </Animation>
std::shared_ptr<Ship::IResource>
ResourceFactoryXMLAnimationV0::ReadResource(std::shared_ptr<Ship::File> file,
                                            std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    auto anim = std::make_shared<Animation>(initData);
    auto root = std::get<std::shared_ptr<tinyxml2::XMLDocument>>(file->Reader)->FirstChildElement();
    anim->frameCount = (int16_t)root->IntAttribute("FrameCount");
    anim->staticIndexMax = (uint16_t)root->IntAttribute("StaticIndexMax");
    auto readList = [](const tinyxml2::XMLElement* e, auto& out) {
        if (e == nullptr || e->GetText() == nullptr) {
            return;
        }
        const char* p = e->GetText();
        char* end;
        for (long v = strtol(p, &end, 0); end != p; v = strtol(p, &end, 0)) {
            out.push_back((typename std::decay_t<decltype(out)>::value_type)v);
            p = end;
        }
    };
    readList(root->FirstChildElement("Values"), anim->frameData);
    readList(root->FirstChildElement("JointIndices"), anim->jointIndices);
    anim->supported = true;
    return anim;
}

void Animation::GetFrame(int frame, int jointCount, std::vector<int16_t>& out) const {
    out.assign(jointCount * 3, 0);
    for (int i = 0; i < jointCount * 3 && i < (int)jointIndices.size(); i++) {
        size_t idx = jointIndices[i];
        if (idx >= staticIndexMax) {
            idx += frame; // animated value: one entry per frame
        }
        out[i] = idx < frameData.size() ? frameData[idx] : 0;
    }
}

void RegisterSkeletonResourceFactories() {
    auto loader = Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceLoader();
    loader->RegisterResourceFactory(std::make_shared<ResourceFactoryBinarySkeletonV0>(), RESOURCE_FORMAT_BINARY,
                                    "Skeleton", (uint32_t)SkelResourceType::Skeleton, 0);
    loader->RegisterResourceFactory(std::make_shared<ResourceFactoryXMLSkeletonV0>(), RESOURCE_FORMAT_XML, "Skeleton",
                                    (uint32_t)SkelResourceType::Skeleton, 0);
    loader->RegisterResourceFactory(std::make_shared<ResourceFactoryBinarySkeletonLimbV0>(), RESOURCE_FORMAT_BINARY,
                                    "SkeletonLimb", (uint32_t)SkelResourceType::SkeletonLimb, 0);
    loader->RegisterResourceFactory(std::make_shared<ResourceFactoryXMLSkeletonLimbV0>(), RESOURCE_FORMAT_XML,
                                    "SkeletonLimb", (uint32_t)SkelResourceType::SkeletonLimb, 0);
    loader->RegisterResourceFactory(std::make_shared<ResourceFactoryBinaryAnimationV0>(), RESOURCE_FORMAT_BINARY,
                                    "Animation", (uint32_t)SkelResourceType::Animation, 0);
    loader->RegisterResourceFactory(std::make_shared<ResourceFactoryXMLAnimationV0>(), RESOURCE_FORMAT_XML, "Animation",
                                    (uint32_t)SkelResourceType::Animation, 0);
}

// --- Posing and drawing (after z_skelanime.c) ---------------------------------------------------

void SkeletonPose::Sample(const Skeleton& skel, const Animation& anim, float frame, bool loop) {
    int jointCount = skel.limbCount + 1; // root translation, then one rotation per limb
    int frames = std::max<int>(1, anim.frameCount);
    float f = loop ? std::fmod(std::max(frame, 0.0f), (float)frames) : std::clamp(frame, 0.0f, (float)(frames - 1));
    int f0 = (int)f;
    int f1 = loop ? (f0 + 1) % frames : std::min(f0 + 1, frames - 1);
    float t = f - f0;

    std::vector<int16_t> a, b;
    anim.GetFrame(f0, jointCount, a);
    anim.GetFrame(f1, jointCount, b);
    mJoints.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        if (i < 3) {
            mJoints[i] = (int16_t)std::lround(a[i] + (b[i] - a[i]) * t); // translation
        } else {
            int16_t d = (int16_t)(b[i] - a[i]); // rotations take the short way round
            mJoints[i] = (int16_t)(a[i] + (int)std::lround(d * t));
        }
    }
}

void SkeletonPose::BlendFrom(const SkeletonPose& from, float weight) {
    if (from.mJoints.size() != mJoints.size() || weight >= 1.0f) {
        return;
    }
    float t = std::max(weight, 0.0f);
    for (size_t i = 0; i < mJoints.size(); i++) {
        int16_t a = from.mJoints[i], b = mJoints[i];
        if (i < 3) {
            mJoints[i] = (int16_t)std::lround(a + (b - a) * t);
        } else {
            int16_t d = (int16_t)(b - a);
            mJoints[i] = (int16_t)(a + (int)std::lround(d * t));
        }
    }
}

namespace {
struct LimbDraw {
    const std::string* dList;
    Mat4 matrix;
};
} // namespace

static void CollectLimbs(const Skeleton& skel, const std::vector<int16_t>& joints, int index, const Mat4& parent,
                         std::vector<LimbDraw>& out, int depth) {
    if (index < 0 || index >= (int)skel.limbs.size() || depth > 255) {
        return;
    }
    const SkeletonLimb* limb = skel.limbs[index].get();
    if (limb == nullptr) {
        return;
    }
    auto joint = [&](int j, int k) { return (size_t)(j * 3 + k) < joints.size() ? joints[j * 3 + k] : (int16_t)0; };
    // The root limb is placed by the animation's translation; the others by their joint position.
    float px = index == 0 ? joint(0, 0) : limb->jointPos[0];
    float py = index == 0 ? joint(0, 1) : limb->jointPos[1];
    float pz = index == 0 ? joint(0, 2) : limb->jointPos[2];
    int r = index + 1;
    // Matrix_TranslateRotateZYX: the limb's vertices are rotated about X, then Y, then Z, then moved.
    Mat4 local = Mul(Mul(Mul(RotateX(BinaryAngleToDegrees(joint(r, 0))), RotateY(BinaryAngleToDegrees(joint(r, 1)))),
                         RotateZ(BinaryAngleToDegrees(joint(r, 2)))),
                     Translate(px, py, pz));
    Mat4 world = Mul(local, parent);
    if (!limb->dList.empty()) {
        out.push_back({ &limb->dList, world });
    }
    if (limb->child != 0xFF) {
        CollectLimbs(skel, joints, limb->child, world, out, depth + 1);
    }
    if (limb->sibling != 0xFF) {
        CollectLimbs(skel, joints, limb->sibling, parent, out, depth + 1);
    }
}

void SkeletonPose::Draw(Gfx*& g, const Skeleton& skel, const Mat4& model, const Mat4& view,
                        const std::function<Mtx*(const Mat4&)>& allocMtx,
                        const std::function<const Gfx*(const std::string&)>& dl) {
    std::vector<LimbDraw> limbs;
    CollectLimbs(skel, mJoints, 0, model, limbs, 0);
    // All limb matrices go in one contiguous block: flex skeleton display lists address them through
    // segment 0x0D, in drawing order.
    std::vector<Mtx*> mtx;
    for (const LimbDraw& l : limbs) {
        Mtx* m = allocMtx(Mul(l.matrix, view));
        if (m == nullptr) {
            return;
        }
        mtx.push_back(m);
    }
    if (mtx.empty()) {
        return;
    }
    __gSPSegment(g++, 0x0D, (uintptr_t)mtx[0]);
    for (size_t i = 0; i < limbs.size(); i++) {
        const Gfx* list = dl(*limbs[i].dList);
        if (list == nullptr) {
            continue;
        }
        gSPMatrix(g++, mtx[i], G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPDisplayList(g++, list);
    }
}

} // namespace Metroid
