#include <tenebris/scene/GpuUploadPlan.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace tenebris::scene {

namespace {

[[nodiscard]] bool checkedMultiply(
    std::uint64_t left,
    std::uint64_t right,
    std::uint64_t& result
) noexcept {
    if (left == 0U || right == 0U) {
        result = 0U;
        return true;
    }
    if (left > std::numeric_limits<std::uint64_t>::max() / right) {
        return false;
    }
    result = left * right;
    return true;
}

[[nodiscard]] bool checkedAlignUp(
    std::uint64_t value,
    std::uint64_t alignment,
    std::uint64_t& result
) noexcept {
    if (alignment == 0U) {
        return false;
    }
    const std::uint64_t remainder = value % alignment;
    if (remainder == 0U) {
        result = value;
        return true;
    }
    const std::uint64_t padding = alignment - remainder;
    if (value > std::numeric_limits<std::uint64_t>::max() - padding) {
        return false;
    }
    result = value + padding;
    return true;
}

[[nodiscard]] GpuUploadPlan failPlan(
    const RenderMesh& mesh,
    UploadPlanError error
) noexcept {
    GpuUploadPlan plan{};
    plan.sourceRevision = mesh.sourceRevision;
    plan.error = error;
    return plan;
}

} // namespace

std::uint64_t BufferRegion::end() const noexcept {
    if (offset > std::numeric_limits<std::uint64_t>::max() - size) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return offset + size;
}

bool BufferRegion::empty() const noexcept {
    return size == 0U;
}

bool GpuUploadPlan::valid() const noexcept {
    if (error != UploadPlanError::None) {
        return false;
    }
    if (!drawable()) {
        return vertexRegion.empty() && indexRegion.empty() && stagingSize == 0U;
    }
    if (vertexRegion.empty() || indexRegion.empty()) {
        return false;
    }
    if ((vertexRegion.offset % gpuBufferAlignment) != 0U
        || (indexRegion.offset % gpuBufferAlignment) != 0U) {
        return false;
    }
    if (vertexRegion.end() > indexRegion.offset || indexRegion.end() > stagingSize) {
        return false;
    }
    return draw.instanceCount > 0U;
}

bool GpuUploadPlan::drawable() const noexcept {
    return error == UploadPlanError::None && draw.indexCount > 0U;
}

std::string GpuUploadPlan::errorMessage() const {
    switch (error) {
    case UploadPlanError::None:
        return {};
    case UploadPlanError::IncompleteMesh:
        return "Render mesh must contain both vertices and indices, or be completely empty.";
    case UploadPlanError::InvalidIndices:
        return "Render mesh contains malformed or out-of-range indices.";
    case UploadPlanError::VertexCountOverflow:
        return "Render mesh vertex count exceeds the supported 32-bit draw range.";
    case UploadPlanError::IndexCountOverflow:
        return "Render mesh index count exceeds the supported 32-bit draw range.";
    case UploadPlanError::VertexSizeOverflow:
        return "Vertex buffer byte size overflowed the 64-bit upload range.";
    case UploadPlanError::IndexSizeOverflow:
        return "Index buffer byte size overflowed the 64-bit upload range.";
    case UploadPlanError::StagingSizeOverflow:
        return "Combined staging allocation size overflowed the 64-bit upload range.";
    }
    return "Unknown GPU upload plan error.";
}

GpuUploadPlan makeGpuUploadPlan(const RenderMesh& mesh) noexcept {
    const bool hasVertices = !mesh.vertices.empty();
    const bool hasIndices = !mesh.indices.empty();
    if (!hasVertices && !hasIndices) {
        GpuUploadPlan emptyPlan{};
        emptyPlan.sourceRevision = mesh.sourceRevision;
        return emptyPlan;
    }
    if (hasVertices != hasIndices) {
        return failPlan(mesh, UploadPlanError::IncompleteMesh);
    }
    if (!mesh.hasValidIndices()) {
        return failPlan(mesh, UploadPlanError::InvalidIndices);
    }

    constexpr std::uint64_t maximumDrawCount = std::numeric_limits<std::uint32_t>::max();
    const std::uint64_t vertexCount = static_cast<std::uint64_t>(mesh.vertices.size());
    const std::uint64_t indexCount = static_cast<std::uint64_t>(mesh.indices.size());
    if (vertexCount > maximumDrawCount) {
        return failPlan(mesh, UploadPlanError::VertexCountOverflow);
    }
    if (indexCount > maximumDrawCount) {
        return failPlan(mesh, UploadPlanError::IndexCountOverflow);
    }

    std::uint64_t vertexBytes = 0U;
    if (!checkedMultiply(vertexCount, sizeof(GpuVoxelVertex), vertexBytes)) {
        return failPlan(mesh, UploadPlanError::VertexSizeOverflow);
    }

    std::uint64_t indexBytes = 0U;
    if (!checkedMultiply(indexCount, sizeof(std::uint32_t), indexBytes)) {
        return failPlan(mesh, UploadPlanError::IndexSizeOverflow);
    }

    std::uint64_t indexOffset = 0U;
    if (!checkedAlignUp(vertexBytes, gpuBufferAlignment, indexOffset)) {
        return failPlan(mesh, UploadPlanError::StagingSizeOverflow);
    }
    if (indexOffset > std::numeric_limits<std::uint64_t>::max() - indexBytes) {
        return failPlan(mesh, UploadPlanError::StagingSizeOverflow);
    }

    const std::uint64_t unalignedStagingSize = indexOffset + indexBytes;
    std::uint64_t stagingSize = 0U;
    if (!checkedAlignUp(unalignedStagingSize, gpuBufferAlignment, stagingSize)) {
        return failPlan(mesh, UploadPlanError::StagingSizeOverflow);
    }

    GpuUploadPlan plan{};
    plan.vertexRegion = {.offset = 0U, .size = vertexBytes};
    plan.indexRegion = {.offset = indexOffset, .size = indexBytes};
    plan.stagingSize = stagingSize;
    plan.sourceRevision = mesh.sourceRevision;
    plan.draw = {
        .indexCount = static_cast<std::uint32_t>(indexCount),
        .instanceCount = 1U,
        .firstIndex = 0U,
        .vertexOffset = 0,
        .firstInstance = 0U,
    };
    return plan;
}

VoxelPushConstants makeVoxelPushConstants(
    const SceneCamera& camera,
    Vec3f lightDirection,
    float ambientLight
) noexcept {
    Vec3f normalizedLight = normalize(lightDirection);
    if (length(normalizedLight) <= 0.000001F) {
        normalizedLight = normalize({0.25F, -1.0F, 0.35F});
    }
    if (!std::isfinite(ambientLight)) {
        ambientLight = 0.22F;
    }

    return {
        .viewProjection = camera.viewProjectionMatrix(),
        .lightDirectionAmbient = {
            normalizedLight.x,
            normalizedLight.y,
            normalizedLight.z,
            std::clamp(ambientLight, 0.0F, 1.0F),
        },
    };
}

const Site47MaterialPalette& site47MaterialPalette() noexcept {
    static constexpr Site47MaterialPalette palette{
        VoxelMaterial{
            .baseColor = {0.0F, 0.0F, 0.0F, 0.0F},
            .emissiveAndResponse = {0.0F, 1.0F, 0.0F, 0.0F},
        },
        VoxelMaterial{
            .baseColor = {0.34F, 0.22F, 0.12F, 1.0F},
            .emissiveAndResponse = {0.0F, 0.96F, 0.0F, 0.0F},
        },
        VoxelMaterial{
            .baseColor = {0.19F, 0.20F, 0.22F, 1.0F},
            .emissiveAndResponse = {0.0F, 0.88F, 0.0F, 0.0F},
        },
        VoxelMaterial{
            .baseColor = {0.24F, 0.28F, 0.31F, 1.0F},
            .emissiveAndResponse = {0.0F, 0.48F, 0.78F, 0.0F},
        },
        VoxelMaterial{
            .baseColor = {0.62F, 0.025F, 0.012F, 1.0F},
            .emissiveAndResponse = {1.75F, 0.35F, 0.08F, 0.0F},
        },
        VoxelMaterial{
            .baseColor = {0.035F, 0.008F, 0.012F, 1.0F},
            .emissiveAndResponse = {0.42F, 0.72F, 0.02F, 0.0F},
        },
    };
    return palette;
}

} // namespace tenebris::scene
