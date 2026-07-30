#pragma once

#include <tenebris/scene/Math.hpp>
#include <tenebris/voxel/GreedyMesher.hpp>
#include <tenebris/voxel/VoxelChunk.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tenebris::scene {

struct alignas(4) GpuVoxelVertex final {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    std::int8_t normalX{0};
    std::int8_t normalY{0};
    std::int8_t normalZ{0};
    std::uint8_t normalPadding{0U};
    voxel::MaterialId material{voxel::airMaterial};
    std::uint16_t materialPadding{0U};

    [[nodiscard]] friend bool operator==(const GpuVoxelVertex&, const GpuVoxelVertex&) noexcept = default;
};

static_assert(sizeof(GpuVoxelVertex) == 20U);
static_assert(alignof(GpuVoxelVertex) == 4U);

struct Bounds3f final {
    Vec3f minimum{};
    Vec3f maximum{};
    bool valid{false};
};

struct RenderMesh final {
    std::vector<GpuVoxelVertex> vertices;
    std::vector<std::uint32_t> indices;
    Bounds3f bounds{};
    std::uint64_t sourceRevision{0U};

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t triangleCount() const noexcept;
    [[nodiscard]] bool hasValidIndices() const noexcept;
};

struct CameraConfig final {
    Vec3f position{16.0F, 15.0F, 40.0F};
    Vec3f target{16.0F, 4.0F, 16.0F};
    Vec3f up{0.0F, 1.0F, 0.0F};
    float verticalFieldOfViewRadians{1.0471975512F};
    float aspectRatio{16.0F / 9.0F};
    float nearPlane{0.05F};
    float farPlane{500.0F};
};

class SceneCamera final {
public:
    SceneCamera() = default;
    explicit SceneCamera(CameraConfig config);

    [[nodiscard]] bool setConfig(CameraConfig config) noexcept;
    [[nodiscard]] const CameraConfig& config() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;
    [[nodiscard]] Mat4f viewMatrix() const noexcept;
    [[nodiscard]] Mat4f projectionMatrix() const noexcept;
    [[nodiscard]] Mat4f viewProjectionMatrix() const noexcept;

private:
    [[nodiscard]] static bool validate(const CameraConfig& config, std::string& error) noexcept;

    CameraConfig config_{};
    std::string lastError_;
};

struct SceneAsset final {
    voxel::VoxelChunk chunk;
    voxel::VoxelMesh voxelMesh;
    RenderMesh renderMesh;
    SceneCamera camera;
};

[[nodiscard]] RenderMesh buildRenderMesh(
    const voxel::VoxelMesh& mesh,
    float voxelSize,
    Vec3f origin,
    std::uint64_t sourceRevision
);

[[nodiscard]] SceneAsset buildSite47Blockout();
[[nodiscard]] std::uint64_t stableRenderMeshHash(const RenderMesh& mesh) noexcept;

} // namespace tenebris::scene
