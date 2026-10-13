#pragma once

// Skeletal models in Ship of Harkinian's resource formats (Skeleton "OSKL", SkeletonLimb "OSLB",
// Animation "OANM"), so skeletons and animations exported for Shipwright (e.g. with Fast64) load
// unchanged. Ported from Shipwright's soh/resource importers and the limb drawing of z_skelanime.c.
// Supported: Normal and Flex skeletons with Standard or LOD limbs, and Normal animations. Curve and
// Skin limbs and Link-format animations are OoT-specific and not loaded.

#include "Gbi.h"
#include "Mat4.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <ship/resource/Resource.h>
#include <ship/resource/ResourceFactoryBinary.h>
#include <ship/resource/ResourceFactoryXML.h>
#include <string>
#include <vector>

namespace Metroid {

enum class SkelResourceType : uint32_t {
    Animation = 0x4F414E4D,    // OANM
    Skeleton = 0x4F534B4C,     // OSKL
    SkeletonLimb = 0x4F534C42, // OSLB
};

enum class SkeletonType : uint8_t { Normal, Flex, Curve };
enum class LimbType : uint8_t { Invalid, Standard, LOD, Skin, Curve, Legacy };

class SkeletonLimb : public Ship::Resource<void> {
  public:
    using Resource::Resource;
    void* GetPointer() override {
        return this;
    }
    size_t GetPointerSize() override {
        return sizeof(*this);
    }

    LimbType type = LimbType::Invalid;
    int16_t jointPos[3] = {}; // relative to the parent limb (the root's comes from the animation)
    uint8_t child = 0xFF, sibling = 0xFF;
    std::string dList; // resource path, or empty
};

class Skeleton : public Ship::Resource<void> {
  public:
    using Resource::Resource;
    void* GetPointer() override {
        return this;
    }
    size_t GetPointerSize() override {
        return sizeof(*this);
    }

    SkeletonType type = SkeletonType::Normal;
    LimbType limbType = LimbType::Standard;
    int limbCount = 0;
    int dListCount = 0;
    std::vector<std::string> limbPaths;
    std::vector<std::shared_ptr<SkeletonLimb>> limbs;
};

class Animation : public Ship::Resource<void> {
  public:
    using Resource::Resource;
    void* GetPointer() override {
        return this;
    }
    size_t GetPointerSize() override {
        return sizeof(*this);
    }

    bool supported = false; // only Normal animations play
    int16_t frameCount = 0;
    std::vector<int16_t> frameData;
    std::vector<uint16_t> jointIndices; // x, y, z per joint
    uint16_t staticIndexMax = 0;

    // Joint values (root translation, then one rotation per limb) at a whole frame.
    void GetFrame(int frame, int jointCount, std::vector<int16_t>& out) const;
};

class ResourceFactoryBinarySkeletonV0 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};
class ResourceFactoryXMLSkeletonV0 final : public Ship::ResourceFactoryXML {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};
class ResourceFactoryBinarySkeletonLimbV0 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};
class ResourceFactoryXMLSkeletonLimbV0 final : public Ship::ResourceFactoryXML {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};
class ResourceFactoryBinaryAnimationV0 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};
// Not a Shipwright format: a readable form of a Normal animation for hand-made test assets.
class ResourceFactoryXMLAnimationV0 final : public Ship::ResourceFactoryXML {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

void RegisterSkeletonResourceFactories();

// Poses a skeleton and draws it.
class SkeletonPose {
  public:
    // Joint values for the animation at a fractional frame, interpolated between whole frames.
    void Sample(const Skeleton& skel, const Animation& anim, float frame, bool loop);
    // Draws every limb. `model` places the skeleton in the scene and `view` is the camera; limb
    // matrices are loaded whole (model * view), as the flex skeleton display lists expect.
    void Draw(Gfx*& g, const Skeleton& skel, const Mat4& model, const Mat4& view,
              const std::function<Mtx*(const Mat4&)>& allocMtx, const std::function<const Gfx*(const std::string&)>& dl);

  private:
    std::vector<int16_t> mJoints; // 3 values per joint
};

} // namespace Metroid
