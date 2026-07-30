#pragma once

#include <tenebris/scene/RenderScene.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tenebris::scene {

inline constexpr std::uint64_t gpuBufferAlignment = 16U;
inline constexpr std::uint32_t voxelPositionLocation = 0U;
inline constexpr std::uint32_t voxelNormalLocation = 1U;
inline constexpr std::uint32_t voxelMaterialLocation = 2U;
inline constexpr std::uint32_t maximumSite47Material = 5U;

struct BufferRegion final {
    std::uint64_t offset{0U};
    std::uint64_t size{0U};

    [[nodiscard]] std::uint64_t end() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
};

struct IndexedDrawPlan final {
    std::uint32_t indexCount{0U};
    std::uint32_t instanceCount{1U};
    std::uint32_t firstIndex{0U};
    std::int32_t vertexOffset{0};
    std::uint32_t firstInstance{0U};
};

enum class UploadPlanError : std::uint8_t {
    None,
    IncompleteMesh,
    InvalidIndices,
    VertexCountOverflow,
    IndexCountOverflow,
    VertexSizeOverflow,
    IndexSizeOverflow,
    StagingSizeOverflow,
};

struct GpuUploadPlan final {
    BufferRegion vertexRegion{};
    BufferRegion indexRegion{};
    std::uint64_t stagingSize{0U};
    std::uint64_t sourceRevision{0U};
    IndexedDrawPlan draw{};
    UploadPlanError error{UploadPlanError::None};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool drawable() const noexcept;
    [[nodiscard]] std::string errorMessage() const;
};

struct alignas(16) VoxelPushConstants final {
    Mat4f viewProjection{};
    std::array<float, 4U> lightDirectionAmbient{0.25F, -1.0F, 0.35F, 0.22F};
};

static_assert(sizeof(VoxelPushConstants) == 80U);
static_assert(alignof(VoxelPushConstants) == 16U);

struct alignas(16) VoxelMaterial final {
    std::array<float, 4U> baseColor{};
    std::array<float, 4U> emissiveAndResponse{};
};

static_assert(sizeof(VoxelMaterial) == 32U);
static_assert(alignof(VoxelMaterial) == 16U);

using Site47MaterialPalette = std::array<VoxelMaterial, maximumSite47Material + 1U>;

[[nodiscard]] GpuUploadPlan makeGpuUploadPlan(const RenderMesh& mesh) noexcept;
[[nodiscard]] VoxelPushConstants makeVoxelPushConstants(
    const SceneCamera& camera,
    Vec3f lightDirection = {0.25F, -1.0F, 0.35F},
    float ambientLight = 0.22F
) noexcept;
[[nodiscard]] const Site47MaterialPalette& site47MaterialPalette() noexcept;

} // namespace tenebris::scene
