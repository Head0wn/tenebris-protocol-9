#include <tenebris/scene/RenderScene.hpp>

#include <algorithm>
#include <array>
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

    // Broken Nevada ground replaces the old rectangular presentation plinth.
    fillBox(asset.chunk, {2, 0, 3}, {30, 1, 31}, sand);
    fillBox(asset.chunk, {0, 0, 11}, {7, 1, 27}, sand);
    fillBox(asset.chunk, {9, 0, 0}, {23, 1, 9}, sand);
    fillBox(asset.chunk, {25, 0, 5}, {32, 1, 19}, sand);
    fillBox(asset.chunk, {27, 0, 21}, {32, 2, 32}, sand);
    fillBox(asset.chunk, {0, 0, 24}, {5, 2, 32}, sand);
    fillBox(asset.chunk, {4, 1, 5}, {10, 2, 9}, sand);
    fillBox(asset.chunk, {25, 1, 8}, {31, 3, 13}, sand);
    fillBox(asset.chunk, {1, 1, 17}, {5, 3, 23}, sand);
    fillBox(asset.chunk, {4, 0, 3}, {7, 1, 7}, voxel::airMaterial);
    fillBox(asset.chunk, {24, 0, 27}, {28, 2, 32}, voxel::airMaterial);
    fillBox(asset.chunk, {2, 0, 28}, {5, 1, 32}, voxel::airMaterial);

    // Fractured approach and stepped tactical apron establish human scale.
    fillBox(asset.chunk, {12, 1, 24}, {20, 2, 32}, concrete);
    fillBox(asset.chunk, {9, 1, 21}, {23, 2, 27}, concrete);
    fillBox(asset.chunk, {7, 1, 24}, {10, 2, 30}, concrete);
    fillBox(asset.chunk, {22, 1, 23}, {25, 2, 28}, concrete);
    fillBox(asset.chunk, {10, 2, 27}, {22, 3, 31}, concrete);
    fillBox(asset.chunk, {12, 3, 27}, {20, 4, 29}, steel);
    fillBox(asset.chunk, {15, 1, 25}, {16, 2, 28}, sand);
    setVoxel(asset.chunk, 18, 1, 30, sand);
    setVoxel(asset.chunk, 12, 2, 27, voxel::airMaterial);
    setVoxel(asset.chunk, 21, 2, 29, voxel::airMaterial);

    // Deep foundation and service trenches keep the laboratory rooted in the terrain.
    fillBox(asset.chunk, {4, 1, 6}, {28, 3, 28}, concrete);
    fillBox(asset.chunk, {5, 3, 7}, {27, 4, 27}, steel);
    fillBox(asset.chunk, {7, 3, 17}, {10, 4, 25}, concrete);
    fillBox(asset.chunk, {22, 3, 16}, {27, 4, 24}, concrete);

    // Front facade: asymmetric blast wings, a recessed gate and a damaged canopy.
    fillBox(asset.chunk, {4, 4, 24}, {12, 10, 28}, concrete);
    fillBox(asset.chunk, {20, 4, 25}, {28, 9, 28}, concrete);
    fillBox(asset.chunk, {4, 4, 21}, {6, 8, 25}, concrete);
    fillBox(asset.chunk, {26, 4, 21}, {28, 11, 26}, steel);
    fillBox(asset.chunk, {11, 4, 27}, {13, 11, 30}, steel);
    fillBox(asset.chunk, {19, 4, 27}, {21, 10, 30}, steel);
    fillBox(asset.chunk, {11, 9, 27}, {21, 11, 30}, steel);
    fillBox(asset.chunk, {9, 10, 26}, {23, 11, 28}, steel);
    fillBox(asset.chunk, {10, 10, 29}, {21, 11, 31}, steel);
    fillBox(asset.chunk, {5, 8, 26}, {10, 9, 29}, steel);

    // Blast damage and exposed reinforcement interrupt every large planar surface.
    fillBox(asset.chunk, {6, 7, 27}, {8, 10, 28}, voxel::airMaterial);
    setVoxel(asset.chunk, 8, 8, 27, voxel::airMaterial);
    setVoxel(asset.chunk, 9, 9, 27, voxel::airMaterial);
    fillBox(asset.chunk, {23, 4, 27}, {26, 7, 28}, voxel::airMaterial);
    setVoxel(asset.chunk, 22, 8, 27, voxel::airMaterial);
    setVoxel(asset.chunk, 25, 7, 27, concrete);
    fillBox(asset.chunk, {4, 4, 24}, {5, 12, 26}, steel);
    fillBox(asset.chunk, {9, 4, 24}, {10, 10, 28}, steel);
    fillBox(asset.chunk, {22, 4, 25}, {23, 10, 28}, steel);
    fillBox(asset.chunk, {27, 4, 21}, {28, 13, 25}, steel);

    // Collapsed checkpoint and scattered blast debris extend the story beyond the doorway.
    fillBox(asset.chunk, {5, 2, 28}, {9, 5, 31}, concrete);
    fillBox(asset.chunk, {6, 3, 29}, {8, 5, 31}, voxel::airMaterial);
    fillBox(asset.chunk, {5, 5, 28}, {9, 6, 29}, steel);
    setVoxel(asset.chunk, 8, 2, 30, steel);
    setVoxel(asset.chunk, 24, 2, 29, concrete);
    fillBox(asset.chunk, {25, 2, 27}, {27, 3, 29}, steel);
    setVoxel(asset.chunk, 3, 2, 24, concrete);
    setVoxel(asset.chunk, 29, 2, 18, steel);

    // Entry corridor: layered door frames, overhead ribs and readable technical alcoves.
    fillBox(asset.chunk, {11, 3, 13}, {21, 4, 28}, steel);
    fillBox(asset.chunk, {10, 4, 13}, {11, 9, 27}, concrete);
    fillBox(asset.chunk, {21, 4, 16}, {22, 9, 27}, concrete);
    fillBox(asset.chunk, {10, 5, 18}, {11, 8, 21}, voxel::airMaterial);
    fillBox(asset.chunk, {21, 5, 21}, {22, 8, 24}, voxel::airMaterial);

    constexpr std::array<std::int32_t, 4U> corridorFrames{15, 18, 22, 26};
    for (const std::int32_t z : corridorFrames) {
        fillBox(asset.chunk, {11, 4, z}, {12, 9, z + 1}, steel);
        fillBox(asset.chunk, {20, 4, z}, {21, 9, z + 1}, steel);
        fillBox(asset.chunk, {11, 8, z}, {21, 9, z + 1}, steel);
    }

    fillBox(asset.chunk, {11, 4, 23}, {14, 6, 26}, steel);
    fillBox(asset.chunk, {12, 5, 24}, {14, 6, 25}, concrete);
    fillBox(asset.chunk, {18, 4, 19}, {21, 6, 22}, steel);
    fillBox(asset.chunk, {18, 5, 20}, {20, 6, 21}, concrete);
    fillBox(asset.chunk, {11, 4, 15}, {13, 7, 18}, steel);
    fillBox(asset.chunk, {19, 4, 14}, {21, 7, 17}, steel);
    fillBox(asset.chunk, {14, 3, 16}, {15, 4, 24}, concrete);
    fillBox(asset.chunk, {17, 3, 14}, {18, 4, 22}, concrete);

    // Left service wing: lower roofline, damaged workshop, power cabinets and pipework.
    fillBox(asset.chunk, {4, 4, 7}, {5, 9, 24}, concrete);
    fillBox(asset.chunk, {5, 4, 7}, {12, 9, 8}, concrete);
    fillBox(asset.chunk, {5, 4, 15}, {10, 5, 24}, concrete);
    fillBox(asset.chunk, {4, 9, 14}, {11, 10, 25}, steel);
    fillBox(asset.chunk, {6, 4, 18}, {9, 8, 22}, steel);
    fillBox(asset.chunk, {7, 5, 19}, {8, 7, 21}, voxel::airMaterial);
    fillBox(asset.chunk, {6, 4, 9}, {10, 7, 13}, steel);
    fillBox(asset.chunk, {5, 5, 14}, {7, 8, 17}, steel);
    fillBox(asset.chunk, {9, 4, 10}, {11, 6, 14}, concrete);
    fillBox(asset.chunk, {5, 10, 18}, {7, 12, 22}, steel);
    fillBox(asset.chunk, {8, 10, 15}, {10, 11, 24}, steel);
    fillBox(asset.chunk, {4, 7, 10}, {5, 9, 13}, voxel::airMaterial);
    setVoxel(asset.chunk, 5, 9, 23, voxel::airMaterial);

    // Right cutaway: structural frames reveal the route while a vent tower anchors the skyline.
    fillBox(asset.chunk, {22, 4, 15}, {28, 6, 16}, concrete);
    fillBox(asset.chunk, {27, 4, 7}, {28, 9, 23}, concrete);
    fillBox(asset.chunk, {22, 4, 18}, {23, 11, 23}, steel);
    fillBox(asset.chunk, {26, 4, 18}, {27, 11, 23}, steel);
    fillBox(asset.chunk, {22, 10, 18}, {27, 11, 23}, steel);
    fillBox(asset.chunk, {23, 4, 18}, {26, 5, 23}, concrete);

    fillBox(asset.chunk, {24, 4, 9}, {28, 14, 10}, steel);
    fillBox(asset.chunk, {24, 4, 13}, {28, 14, 14}, steel);
    fillBox(asset.chunk, {24, 4, 10}, {25, 14, 13}, steel);
    fillBox(asset.chunk, {27, 4, 10}, {28, 14, 13}, steel);
    fillBox(asset.chunk, {24, 13, 9}, {28, 15, 14}, steel);
    fillBox(asset.chunk, {25, 6, 9}, {27, 8, 10}, voxel::airMaterial);
    fillBox(asset.chunk, {25, 10, 13}, {27, 12, 14}, voxel::airMaterial);
    fillBox(asset.chunk, {23, 11, 8}, {29, 12, 15}, steel);
    fillBox(asset.chunk, {25, 12, 10}, {27, 13, 13}, voxel::airMaterial);

    // Chamber 9: open technical hall, gantry, observation booths and broken roof trusses.
    fillBox(asset.chunk, {7, 2, 5}, {25, 4, 16}, steel);
    fillBox(asset.chunk, {7, 4, 5}, {25, 11, 6}, concrete);
    fillBox(asset.chunk, {7, 4, 6}, {8, 10, 16}, concrete);
    fillBox(asset.chunk, {24, 4, 6}, {25, 8, 16}, concrete);
    fillBox(asset.chunk, {7, 10, 5}, {13, 11, 16}, steel);
    fillBox(asset.chunk, {19, 10, 5}, {25, 11, 10}, steel);
    fillBox(asset.chunk, {21, 10, 12}, {25, 11, 16}, steel);
    fillBox(asset.chunk, {13, 10, 5}, {15, 11, 9}, steel);
    fillBox(asset.chunk, {17, 10, 12}, {19, 11, 16}, steel);

    fillBox(asset.chunk, {9, 5, 5}, {13, 9, 6}, voxel::airMaterial);
    fillBox(asset.chunk, {19, 5, 5}, {23, 9, 6}, voxel::airMaterial);
    fillBox(asset.chunk, {8, 4, 7}, {11, 7, 10}, steel);
    fillBox(asset.chunk, {9, 5, 8}, {11, 7, 9}, concrete);
    fillBox(asset.chunk, {21, 4, 7}, {24, 7, 10}, steel);
    fillBox(asset.chunk, {21, 5, 8}, {23, 7, 9}, concrete);
    fillBox(asset.chunk, {8, 8, 11}, {12, 9, 15}, steel);
    fillBox(asset.chunk, {20, 8, 11}, {24, 9, 15}, steel);

    // Containment dais, maintenance ring and cage are the unmistakable end point of the vista.
    fillBox(asset.chunk, {12, 3, 8}, {20, 4, 16}, steel);
    fillBox(asset.chunk, {13, 4, 9}, {19, 5, 15}, concrete);
    fillBox(asset.chunk, {11, 4, 8}, {13, 5, 16}, steel);
    fillBox(asset.chunk, {19, 4, 8}, {21, 5, 16}, steel);
    fillBox(asset.chunk, {12, 4, 8}, {13, 10, 9}, steel);
    fillBox(asset.chunk, {19, 4, 8}, {20, 10, 9}, steel);
    fillBox(asset.chunk, {12, 4, 15}, {13, 10, 16}, steel);
    fillBox(asset.chunk, {19, 4, 15}, {20, 10, 16}, steel);
    fillBox(asset.chunk, {12, 9, 8}, {20, 10, 9}, steel);
    fillBox(asset.chunk, {12, 9, 15}, {20, 10, 16}, steel);
    fillBox(asset.chunk, {12, 9, 9}, {13, 10, 15}, steel);
    fillBox(asset.chunk, {19, 9, 9}, {20, 10, 15}, steel);
    fillBox(asset.chunk, {14, 4, 7}, {18, 6, 8}, steel);

    // Emergency lighting is intentionally local and discontinuous, never a debug-red ribbon.
    fillBox(asset.chunk, {14, 9, 29}, {18, 10, 30}, emergencyLight);
    fillBox(asset.chunk, {5, 7, 26}, {9, 8, 27}, emergencyLight);
    fillBox(asset.chunk, {23, 8, 26}, {26, 9, 27}, emergencyLight);
    fillBox(asset.chunk, {10, 8, 16}, {11, 9, 18}, emergencyLight);
    fillBox(asset.chunk, {21, 8, 19}, {22, 9, 21}, emergencyLight);
    fillBox(asset.chunk, {10, 8, 24}, {11, 9, 26}, emergencyLight);
    fillBox(asset.chunk, {12, 6, 24}, {14, 7, 25}, emergencyLight);
    fillBox(asset.chunk, {19, 6, 20}, {21, 7, 21}, emergencyLight);
    fillBox(asset.chunk, {6, 7, 10}, {10, 8, 11}, emergencyLight);
    fillBox(asset.chunk, {6, 7, 22}, {9, 8, 23}, emergencyLight);
    fillBox(asset.chunk, {24, 6, 18}, {26, 7, 19}, emergencyLight);
    fillBox(asset.chunk, {24, 8, 22}, {26, 9, 23}, emergencyLight);
    fillBox(asset.chunk, {25, 8, 9}, {27, 9, 10}, emergencyLight);
    fillBox(asset.chunk, {8, 8, 5}, {12, 9, 6}, emergencyLight);
    fillBox(asset.chunk, {20, 8, 5}, {24, 9, 6}, emergencyLight);
    fillBox(asset.chunk, {14, 8, 8}, {18, 9, 9}, emergencyLight);

    // Protocol 9 core: dense origin, vertical growth and three distinct escape routes.
    fillBox(asset.chunk, {14, 5, 10}, {18, 8, 14}, organic);
    fillBox(asset.chunk, {15, 8, 11}, {18, 10, 13}, organic);
    fillBox(asset.chunk, {13, 5, 11}, {15, 7, 13}, organic);
    setVoxel(asset.chunk, 18, 6, 10, organic);
    setVoxel(asset.chunk, 18, 7, 13, organic);
    setVoxel(asset.chunk, 14, 8, 12, organic);
    setVoxel(asset.chunk, 16, 10, 12, organic);
    setVoxel(asset.chunk, 17, 9, 10, organic);

    constexpr std::array<voxel::LocalPosition, 22U> floorTrail{{
        {16, 4, 14}, {16, 4, 15}, {17, 4, 16}, {17, 4, 17},
        {16, 4, 18}, {15, 4, 19}, {15, 4, 20}, {16, 4, 21},
        {17, 4, 22}, {17, 4, 23}, {16, 4, 24}, {16, 4, 25},
        {15, 4, 26}, {16, 4, 27}, {16, 4, 28}, {17, 4, 28},
        {18, 4, 27}, {14, 4, 22}, {18, 4, 19}, {15, 4, 17},
        {17, 4, 15}, {15, 4, 24},
    }};
    for (const voxel::LocalPosition position : floorTrail) {
        setVoxel(asset.chunk, position.x, position.y, position.z, organic);
    }

    fillBox(asset.chunk, {21, 4, 18}, {22, 8, 20}, organic);
    setVoxel(asset.chunk, 21, 8, 19, organic);
    setVoxel(asset.chunk, 20, 7, 19, organic);
    setVoxel(asset.chunk, 22, 5, 18, organic);
    fillBox(asset.chunk, {9, 4, 11}, {10, 7, 13}, organic);
    setVoxel(asset.chunk, 10, 6, 12, organic);
    setVoxel(asset.chunk, 8, 4, 12, organic);
    setVoxel(asset.chunk, 15, 9, 15, organic);
    setVoxel(asset.chunk, 16, 10, 15, organic);
    setVoxel(asset.chunk, 18, 9, 14, organic);

    asset.voxelMesh = voxel::GreedyMesher::build(asset.chunk);
    asset.renderMesh = buildRenderMesh(
        asset.voxelMesh,
        0.5F,
        {-8.0F, 0.0F, -8.0F},
        asset.chunk.revision()
    );
    asset.camera = SceneCamera({
        .position = {7.2F, 4.35F, 14.8F},
        .target = {0.0F, 3.15F, 1.25F},
        .up = {0.0F, 1.0F, 0.0F},
        .verticalFieldOfViewRadians = 0.820304748F,
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
