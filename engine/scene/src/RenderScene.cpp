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

    // Nevada terrain and a broken concrete approach establish scale before the laboratory.
    fillBox(asset.chunk, {0, 0, 0}, {32, 1, 32}, sand);
    fillBox(asset.chunk, {0, 1, 0}, {8, 2, 6}, sand);
    fillBox(asset.chunk, {25, 1, 0}, {32, 2, 9}, sand);
    fillBox(asset.chunk, {0, 1, 25}, {6, 2, 32}, sand);
    fillBox(asset.chunk, {27, 1, 23}, {32, 3, 32}, sand);
    fillBox(asset.chunk, {14, 1, 0}, {18, 2, 8}, concrete);
    fillBox(asset.chunk, {11, 1, 5}, {21, 2, 10}, concrete);
    fillBox(asset.chunk, {9, 2, 7}, {23, 3, 9}, concrete);

    // Site 47 foundation is intentionally irregular instead of a single sealed box.
    fillBox(asset.chunk, {4, 1, 7}, {28, 2, 29}, concrete);
    fillBox(asset.chunk, {5, 2, 8}, {27, 3, 28}, concrete);

    // Front facade: two heavy wings framing a recessed tactical entrance.
    fillBox(asset.chunk, {4, 3, 27}, {12, 9, 28}, concrete);
    fillBox(asset.chunk, {20, 3, 27}, {28, 9, 28}, concrete);
    fillBox(asset.chunk, {4, 3, 24}, {6, 8, 28}, concrete);
    fillBox(asset.chunk, {26, 3, 24}, {28, 9, 28}, concrete);

    fillBox(asset.chunk, {12, 3, 27}, {14, 10, 29}, steel);
    fillBox(asset.chunk, {18, 3, 27}, {20, 10, 29}, steel);
    fillBox(asset.chunk, {12, 8, 27}, {20, 10, 29}, steel);
    fillBox(asset.chunk, {10, 9, 25}, {22, 10, 31}, steel);
    fillBox(asset.chunk, {14, 8, 28}, {18, 9, 29}, emergencyLight);

    // Facade damage and stepped buttresses prevent the silhouette from reading as a cube.
    fillBox(asset.chunk, {7, 7, 27}, {9, 9, 28}, voxel::airMaterial);
    setVoxel(asset.chunk, 9, 8, 27, voxel::airMaterial);
    fillBox(asset.chunk, {23, 3, 27}, {25, 5, 28}, voxel::airMaterial);
    fillBox(asset.chunk, {4, 3, 25}, {5, 10, 27}, steel);
    fillBox(asset.chunk, {9, 3, 27}, {10, 9, 28}, steel);
    fillBox(asset.chunk, {22, 3, 27}, {23, 9, 28}, steel);
    fillBox(asset.chunk, {27, 3, 24}, {28, 11, 27}, steel);

    // Entry corridor: readable from outside, with no full roof hiding the route.
    fillBox(asset.chunk, {12, 2, 14}, {20, 3, 28}, steel);
    fillBox(asset.chunk, {11, 3, 14}, {12, 8, 27}, concrete);
    fillBox(asset.chunk, {20, 3, 17}, {21, 8, 27}, concrete);
    fillBox(asset.chunk, {11, 4, 18}, {12, 7, 21}, voxel::airMaterial);
    fillBox(asset.chunk, {20, 4, 21}, {21, 7, 24}, voxel::airMaterial);

    for (std::int32_t z = 15; z < 28; z += 4) {
        fillBox(asset.chunk, {11, 8, z}, {21, 9, z + 1}, steel);
    }
    fillBox(asset.chunk, {11, 7, 16}, {12, 8, 18}, emergencyLight);
    fillBox(asset.chunk, {20, 7, 19}, {21, 8, 21}, emergencyLight);
    fillBox(asset.chunk, {11, 7, 24}, {12, 8, 26}, emergencyLight);

    // Left service wing, machinery and a lower roof create a second architectural mass.
    fillBox(asset.chunk, {4, 3, 8}, {5, 8, 25}, concrete);
    fillBox(asset.chunk, {5, 3, 8}, {12, 8, 9}, concrete);
    fillBox(asset.chunk, {5, 3, 15}, {11, 4, 26}, concrete);
    fillBox(asset.chunk, {4, 8, 15}, {11, 9, 27}, steel);
    fillBox(asset.chunk, {6, 3, 18}, {9, 7, 22}, steel);
    fillBox(asset.chunk, {7, 4, 19}, {8, 6, 21}, voxel::airMaterial);
    fillBox(asset.chunk, {6, 3, 10}, {10, 6, 14}, steel);
    fillBox(asset.chunk, {6, 6, 11}, {10, 7, 12}, emergencyLight);
    fillBox(asset.chunk, {5, 7, 23}, {9, 8, 24}, emergencyLight);

    // Right side is deliberately opened as a cinematic cutaway, with a vent tower as landmark.
    fillBox(asset.chunk, {21, 3, 16}, {28, 5, 17}, concrete);
    fillBox(asset.chunk, {27, 3, 8}, {28, 8, 24}, concrete);
    fillBox(asset.chunk, {23, 3, 18}, {24, 10, 23}, steel);
    fillBox(asset.chunk, {26, 3, 18}, {27, 10, 23}, steel);
    fillBox(asset.chunk, {23, 9, 18}, {27, 10, 23}, steel);
    fillBox(asset.chunk, {24, 3, 18}, {26, 4, 23}, concrete);
    fillBox(asset.chunk, {24, 5, 18}, {26, 6, 19}, emergencyLight);
    fillBox(asset.chunk, {24, 7, 18}, {26, 8, 19}, emergencyLight);

    // Chamber 9 occupies the rear of the cutaway, framed as the visual focal point.
    fillBox(asset.chunk, {8, 2, 7}, {24, 3, 16}, steel);
    fillBox(asset.chunk, {8, 3, 7}, {24, 10, 8}, concrete);
    fillBox(asset.chunk, {8, 3, 8}, {9, 9, 16}, concrete);
    fillBox(asset.chunk, {23, 3, 8}, {24, 6, 16}, concrete);
    fillBox(asset.chunk, {8, 9, 7}, {24, 10, 10}, steel);
    fillBox(asset.chunk, {8, 9, 12}, {24, 10, 13}, steel);
    fillBox(asset.chunk, {8, 9, 15}, {24, 10, 16}, steel);

    fillBox(asset.chunk, {10, 5, 7}, {14, 8, 8}, voxel::airMaterial);
    fillBox(asset.chunk, {18, 5, 7}, {22, 8, 8}, voxel::airMaterial);
    fillBox(asset.chunk, {9, 7, 8}, {13, 8, 9}, emergencyLight);
    fillBox(asset.chunk, {19, 7, 8}, {23, 8, 9}, emergencyLight);

    // Containment dais and cage: a recognizable destination beyond the corridor.
    fillBox(asset.chunk, {13, 3, 9}, {19, 4, 15}, steel);
    fillBox(asset.chunk, {14, 4, 10}, {18, 5, 14}, concrete);
    fillBox(asset.chunk, {13, 4, 9}, {14, 9, 10}, steel);
    fillBox(asset.chunk, {18, 4, 9}, {19, 9, 10}, steel);
    fillBox(asset.chunk, {13, 4, 14}, {14, 9, 15}, steel);
    fillBox(asset.chunk, {18, 4, 14}, {19, 9, 15}, steel);
    fillBox(asset.chunk, {13, 8, 9}, {19, 9, 10}, steel);
    fillBox(asset.chunk, {13, 8, 14}, {19, 9, 15}, steel);
    fillBox(asset.chunk, {13, 8, 10}, {14, 9, 14}, steel);
    fillBox(asset.chunk, {18, 8, 10}, {19, 9, 14}, steel);
    fillBox(asset.chunk, {14, 7, 9}, {18, 8, 10}, emergencyLight);

    // The Protocol 9 mass rises inside the cage and escapes towards the entrance.
    fillBox(asset.chunk, {15, 4, 11}, {17, 7, 13}, organic);
    setVoxel(asset.chunk, 14, 4, 12, organic);
    setVoxel(asset.chunk, 17, 5, 11, organic);
    setVoxel(asset.chunk, 16, 7, 12, organic);
    setVoxel(asset.chunk, 15, 6, 10, organic);

    constexpr std::int32_t trailOffsets[16]{0, 0, 1, 0, -1, -1, 0, 1, 1, 0, -1, 0, 1, 0, 0, -1};
    for (std::int32_t step = 0; step < 16; ++step) {
        const std::int32_t x = 16 + trailOffsets[step];
        const std::int32_t z = 12 + step;
        setVoxel(asset.chunk, x, 3, z, organic);
        if ((step % 3) == 0) {
            setVoxel(asset.chunk, x + 1, 3, z, organic);
        }
    }
    fillBox(asset.chunk, {20, 3, 18}, {21, 6, 20}, organic);
    setVoxel(asset.chunk, 20, 6, 19, organic);
    setVoxel(asset.chunk, 19, 3, 23, organic);
    setVoxel(asset.chunk, 17, 3, 27, organic);

    asset.voxelMesh = voxel::GreedyMesher::build(asset.chunk);
    asset.renderMesh = buildRenderMesh(
        asset.voxelMesh,
        0.5F,
        {-8.0F, 0.0F, -8.0F},
        asset.chunk.revision()
    );
    asset.camera = SceneCamera({
        .position = {8.75F, 5.5F, 15.0F},
        .target = {0.0F, 3.0F, 1.0F},
        .up = {0.0F, 1.0F, 0.0F},
        .verticalFieldOfViewRadians = 0.907571211F,
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
