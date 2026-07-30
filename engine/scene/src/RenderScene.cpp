#include <tenebris/scene/RenderScene.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace tenebris::scene {

namespace {

constexpr float minimumDirectionLength = 0.0001F;
constexpr std::uint64_t fnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;

void expandBounds(Bounds3f& bounds, Vec3f position) noexcept {
    if (!bounds.valid) {
        bounds.minimum = position;
        bounds.maximum = position;
        bounds.valid = true;
        return;
    }

    bounds.minimum.x = std::min(bounds.minimum.x, position.x);
    bounds.minimum.y = std::min(bounds.minimum.y, position.y);
    bounds.minimum.z = std::min(bounds.minimum.z, position.z);
    bounds.maximum.x = std::max(bounds.maximum.x, position.x);
    bounds.maximum.y = std::max(bounds.maximum.y, position.y);
    bounds.maximum.z = std::max(bounds.maximum.z, position.z);
}

void setVoxel(
    voxel::VoxelChunk& chunk,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    voxel::MaterialId material
) noexcept {
    static_cast<void>(chunk.set({x, y, z}, material));
}

void fillBox(
    voxel::VoxelChunk& chunk,
    voxel::LocalPosition minimum,
    voxel::LocalPosition maximumExclusive,
    voxel::MaterialId material
) noexcept {
    for (std::int32_t z = minimum.z; z < maximumExclusive.z; ++z) {
        for (std::int32_t y = minimum.y; y < maximumExclusive.y; ++y) {
            for (std::int32_t x = minimum.x; x < maximumExclusive.x; ++x) {
                setVoxel(chunk, x, y, z, material);
            }
        }
    }
}

void hashByte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= fnvPrime;
}

void hashU16(std::uint64_t& hash, std::uint16_t value) noexcept {
    hashByte(hash, static_cast<std::uint8_t>(value & 0xFFU));
    hashByte(hash, static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void hashU32(std::uint64_t& hash, std::uint32_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        hashByte(hash, static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void hashU64(std::uint64_t& hash, std::uint64_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        hashByte(hash, static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void hashFloat(std::uint64_t& hash, float value) noexcept {
    hashU32(hash, std::bit_cast<std::uint32_t>(value));
}

} // namespace

bool RenderMesh::empty() const noexcept {
    return vertices.empty() || indices.empty();
}

std::size_t RenderMesh::triangleCount() const noexcept {
    return indices.size() / 3U;
}

bool RenderMesh::hasValidIndices() const noexcept {
    if ((indices.size() % 3U) != 0U) {
        return false;
    }
    return std::all_of(
        indices.begin(),
        indices.end(),
        [vertexCount = vertices.size()](std::uint32_t index) {
            return static_cast<std::size_t>(index) < vertexCount;
        }
    );
}

SceneCamera::SceneCamera(CameraConfig config) {
    if (!setConfig(config)) {
        config_ = CameraConfig{};
    }
}

bool SceneCamera::setConfig(CameraConfig config) noexcept {
    std::string error;
    if (!validate(config, error)) {
        lastError_ = std::move(error);
        return false;
    }

    config_ = config;
    lastError_.clear();
    return true;
}

const CameraConfig& SceneCamera::config() const noexcept {
    return config_;
}

const std::string& SceneCamera::lastError() const noexcept {
    return lastError_;
}

Mat4f SceneCamera::viewMatrix() const noexcept {
    return makeLookAt(config_.position, config_.target, config_.up);
}

Mat4f SceneCamera::projectionMatrix() const noexcept {
    return makePerspective(
        config_.verticalFieldOfViewRadians,
        config_.aspectRatio,
        config_.nearPlane,
        config_.farPlane
    );
}

Mat4f SceneCamera::viewProjectionMatrix() const noexcept {
    return multiply(projectionMatrix(), viewMatrix());
}

bool SceneCamera::validate(const CameraConfig& config, std::string& error) noexcept {
    if (!isFinite(config.position) || !isFinite(config.target) || !isFinite(config.up)) {
        error = "Camera vectors must contain finite values.";
        return false;
    }

    const Vec3f direction = config.target - config.position;
    if (length(direction) <= minimumDirectionLength) {
        error = "Camera position and target must be different.";
        return false;
    }
    if (length(config.up) <= minimumDirectionLength) {
        error = "Camera up vector must not be zero.";
        return false;
    }
    if (length(cross(direction, config.up)) <= minimumDirectionLength) {
        error = "Camera up vector must not be parallel to the viewing direction.";
        return false;
    }
    if (!std::isfinite(config.verticalFieldOfViewRadians)
        || config.verticalFieldOfViewRadians <= 0.1F
        || config.verticalFieldOfViewRadians >= 3.0F) {
        error = "Camera vertical field of view is outside the supported range.";
        return false;
    }
    if (!std::isfinite(config.aspectRatio) || config.aspectRatio <= 0.0F) {
        error = "Camera aspect ratio must be positive.";
        return false;
    }
    if (!std::isfinite(config.nearPlane) || !std::isfinite(config.farPlane)
        || config.nearPlane <= 0.0F || config.farPlane <= config.nearPlane) {
        error = "Camera clipping planes are invalid.";
        return false;
    }
    return true;
}

RenderMesh buildRenderMesh(
    const voxel::VoxelMesh& mesh,
    float voxelSize,
    Vec3f origin,
    std::uint64_t sourceRevision
) {
    RenderMesh result{};
    if (!std::isfinite(voxelSize) || voxelSize <= 0.0F || !isFinite(origin)) {
        return result;
    }
    if ((mesh.indices.size() % 3U) != 0U) {
        return result;
    }

    result.vertices.reserve(mesh.vertices.size());
    for (const voxel::VoxelVertex& source : mesh.vertices) {
        const Vec3f position{
            origin.x + static_cast<float>(source.x) * voxelSize,
            origin.y + static_cast<float>(source.y) * voxelSize,
            origin.z + static_cast<float>(source.z) * voxelSize,
        };

        result.vertices.push_back({
            .x = position.x,
            .y = position.y,
            .z = position.z,
            .normalX = source.normalX,
            .normalY = source.normalY,
            .normalZ = source.normalZ,
            .normalPadding = 0U,
            .material = source.material,
            .materialPadding = 0U,
        });
        expandBounds(result.bounds, position);
    }

    result.indices = mesh.indices;
    result.sourceRevision = sourceRevision;
    if (!result.hasValidIndices()) {
        return {};
    }
    return result;
}

SceneAsset buildSite47Blockout() {
    constexpr voxel::MaterialId sand = 1U;
    constexpr voxel::MaterialId concrete = 2U;
    constexpr voxel::MaterialId steel = 3U;
    constexpr voxel::MaterialId emergencyLight = 4U;
    constexpr voxel::MaterialId organic = 5U;

    SceneAsset asset{};

    fillBox(asset.chunk, {0, 0, 0}, {32, 1, 32}, sand);
    fillBox(asset.chunk, {6, 1, 7}, {26, 2, 27}, concrete);

    fillBox(asset.chunk, {6, 2, 7}, {26, 9, 8}, concrete);
    fillBox(asset.chunk, {6, 2, 26}, {26, 9, 27}, concrete);
    fillBox(asset.chunk, {6, 2, 8}, {7, 9, 26}, concrete);
    fillBox(asset.chunk, {25, 2, 8}, {26, 9, 26}, concrete);

    fillBox(asset.chunk, {7, 8, 8}, {25, 9, 26}, steel);
    fillBox(asset.chunk, {15, 2, 8}, {17, 8, 21}, steel);
    fillBox(asset.chunk, {9, 2, 17}, {23, 8, 18}, concrete);

    fillBox(asset.chunk, {14, 2, 7}, {18, 6, 8}, voxel::airMaterial);
    fillBox(asset.chunk, {15, 2, 17}, {17, 5, 18}, voxel::airMaterial);
    fillBox(asset.chunk, {15, 2, 20}, {17, 5, 21}, voxel::airMaterial);

    fillBox(asset.chunk, {10, 2, 11}, {14, 4, 15}, steel);
    fillBox(asset.chunk, {19, 2, 20}, {23, 6, 24}, steel);
    fillBox(asset.chunk, {20, 3, 21}, {22, 7, 23}, voxel::airMaterial);

    setVoxel(asset.chunk, 8, 7, 9, emergencyLight);
    setVoxel(asset.chunk, 23, 7, 9, emergencyLight);
    setVoxel(asset.chunk, 8, 7, 24, emergencyLight);
    setVoxel(asset.chunk, 23, 7, 24, emergencyLight);

    for (std::int32_t step = 0; step < 10; ++step) {
        setVoxel(asset.chunk, 16 + (step / 3), 2, 10 + step, organic);
        if ((step % 2) == 0) {
            setVoxel(asset.chunk, 17 + (step / 4), 2, 10 + step, organic);
        }
    }
    fillBox(asset.chunk, {20, 2, 21}, {22, 4, 23}, organic);

    asset.voxelMesh = voxel::GreedyMesher::build(asset.chunk);
    asset.renderMesh = buildRenderMesh(asset.voxelMesh, 0.5F, {-8.0F, 0.0F, -8.0F}, asset.chunk.revision());
    asset.camera = SceneCamera({
        .position = {0.0F, 10.5F, 14.0F},
        .target = {0.0F, 2.5F, 0.0F},
        .up = {0.0F, 1.0F, 0.0F},
        .verticalFieldOfViewRadians = 1.0471975512F,
        .aspectRatio = 16.0F / 9.0F,
        .nearPlane = 0.05F,
        .farPlane = 200.0F,
    });
    return asset;
}

std::uint64_t stableRenderMeshHash(const RenderMesh& mesh) noexcept {
    std::uint64_t hash = fnvOffsetBasis;
    hashU64(hash, static_cast<std::uint64_t>(mesh.vertices.size()));
    hashU64(hash, static_cast<std::uint64_t>(mesh.indices.size()));
    hashU64(hash, mesh.sourceRevision);

    for (const GpuVoxelVertex& vertex : mesh.vertices) {
        hashFloat(hash, vertex.x);
        hashFloat(hash, vertex.y);
        hashFloat(hash, vertex.z);
        hashByte(hash, static_cast<std::uint8_t>(vertex.normalX));
        hashByte(hash, static_cast<std::uint8_t>(vertex.normalY));
        hashByte(hash, static_cast<std::uint8_t>(vertex.normalZ));
        hashU16(hash, vertex.material);
    }
    for (const std::uint32_t index : mesh.indices) {
        hashU32(hash, index);
    }
    return hash;
}

} // namespace tenebris::scene
