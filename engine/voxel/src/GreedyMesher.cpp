#include <tenebris/voxel/GreedyMesher.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tenebris::voxel {

namespace {

struct MaskCell final {
    MaterialId material{airMaterial};
    bool positiveNormal{false};

    [[nodiscard]] bool visible() const noexcept {
        return material != airMaterial;
    }

    [[nodiscard]] friend bool operator==(const MaskCell&, const MaskCell&) noexcept = default;
};

using Coordinate = std::array<std::int32_t, 3U>;
using Normal = std::array<std::int8_t, 3U>;

[[nodiscard]] constexpr std::size_t maskIndex(std::int32_t x, std::int32_t y) noexcept {
    return static_cast<std::size_t>(x)
        + VoxelChunk::extentValue * static_cast<std::size_t>(y);
}

[[nodiscard]] constexpr LocalPosition toLocalPosition(const Coordinate& coordinate) noexcept {
    return {
        .x = coordinate[0U],
        .y = coordinate[1U],
        .z = coordinate[2U],
    };
}

[[nodiscard]] constexpr Coordinate add(
    const Coordinate& left,
    const Coordinate& right
) noexcept {
    return {
        left[0U] + right[0U],
        left[1U] + right[1U],
        left[2U] + right[2U],
    };
}

[[nodiscard]] VoxelVertex makeVertex(
    const Coordinate& position,
    const Normal& normal,
    MaterialId material
) {
    return {
        .x = static_cast<std::uint16_t>(position[0U]),
        .y = static_cast<std::uint16_t>(position[1U]),
        .z = static_cast<std::uint16_t>(position[2U]),
        .normalX = normal[0U],
        .normalY = normal[1U],
        .normalZ = normal[2U],
        .material = material,
    };
}

void appendQuad(
    VoxelMesh& mesh,
    const Coordinate& origin,
    const Coordinate& deltaU,
    const Coordinate& deltaV,
    std::size_t axis,
    const MaskCell& cell
) {
    constexpr std::size_t verticesPerQuad = 4U;
    if (mesh.vertices.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())
            - verticesPerQuad) {
        throw std::length_error("Voxel mesh exceeds 32-bit index capacity.");
    }

    Normal normal{};
    normal[axis] = static_cast<std::int8_t>(cell.positiveNormal ? 1 : -1);

    const Coordinate opposite = add(add(origin, deltaU), deltaV);
    const Coordinate alongU = add(origin, deltaU);
    const Coordinate alongV = add(origin, deltaV);

    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    if (cell.positiveNormal) {
        mesh.vertices.push_back(makeVertex(origin, normal, cell.material));
        mesh.vertices.push_back(makeVertex(alongU, normal, cell.material));
        mesh.vertices.push_back(makeVertex(opposite, normal, cell.material));
        mesh.vertices.push_back(makeVertex(alongV, normal, cell.material));
    } else {
        mesh.vertices.push_back(makeVertex(origin, normal, cell.material));
        mesh.vertices.push_back(makeVertex(alongV, normal, cell.material));
        mesh.vertices.push_back(makeVertex(opposite, normal, cell.material));
        mesh.vertices.push_back(makeVertex(alongU, normal, cell.material));
    }

    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1U);
    mesh.indices.push_back(base + 2U);
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 2U);
    mesh.indices.push_back(base + 3U);
}

} // namespace

bool VoxelMesh::empty() const noexcept {
    return vertices.empty() && indices.empty();
}

std::size_t VoxelMesh::quadCount() const noexcept {
    return vertices.size() / 4U;
}

std::size_t VoxelMesh::triangleCount() const noexcept {
    return indices.size() / 3U;
}

VoxelMesh GreedyMesher::build(const VoxelChunk& chunk) {
    VoxelMesh mesh;
    if (chunk.solidCount() == 0U) {
        return mesh;
    }

    mesh.vertices.reserve(256U);
    mesh.indices.reserve(384U);

    std::array<MaskCell, VoxelChunk::extentValue * VoxelChunk::extentValue> mask{};

    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        const std::size_t axisU = (axis + 1U) % 3U;
        const std::size_t axisV = (axis + 2U) % 3U;

        Coordinate position{};
        Coordinate step{};
        step[axis] = 1;

        for (std::int32_t slice = -1; slice < VoxelChunk::extent; ++slice) {
            position[axis] = slice;

            for (std::int32_t v = 0; v < VoxelChunk::extent; ++v) {
                position[axisV] = v;
                for (std::int32_t u = 0; u < VoxelChunk::extent; ++u) {
                    position[axisU] = u;

                    const MaterialId current = chunk.sample(toLocalPosition(position));
                    const Coordinate adjacentPosition = add(position, step);
                    const MaterialId adjacent = chunk.sample(toLocalPosition(adjacentPosition));
                    const bool currentSolid = current != airMaterial;
                    const bool adjacentSolid = adjacent != airMaterial;

                    MaskCell cell{};
                    if (currentSolid != adjacentSolid) {
                        cell.material = currentSolid ? current : adjacent;
                        cell.positiveNormal = currentSolid;
                    }
                    mask[maskIndex(u, v)] = cell;
                }
            }

            for (std::int32_t v = 0; v < VoxelChunk::extent; ++v) {
                std::int32_t u = 0;
                while (u < VoxelChunk::extent) {
                    const MaskCell cell = mask[maskIndex(u, v)];
                    if (!cell.visible()) {
                        ++u;
                        continue;
                    }

                    std::int32_t width = 1;
                    while (u + width < VoxelChunk::extent
                           && mask[maskIndex(u + width, v)] == cell) {
                        ++width;
                    }

                    std::int32_t height = 1;
                    bool canGrow = true;
                    while (v + height < VoxelChunk::extent && canGrow) {
                        for (std::int32_t offset = 0; offset < width; ++offset) {
                            if (mask[maskIndex(u + offset, v + height)] != cell) {
                                canGrow = false;
                                break;
                            }
                        }
                        if (canGrow) {
                            ++height;
                        }
                    }

                    Coordinate origin{};
                    origin[axis] = slice + 1;
                    origin[axisU] = u;
                    origin[axisV] = v;

                    Coordinate deltaU{};
                    deltaU[axisU] = width;
                    Coordinate deltaV{};
                    deltaV[axisV] = height;
                    appendQuad(mesh, origin, deltaU, deltaV, axis, cell);

                    for (std::int32_t clearV = 0; clearV < height; ++clearV) {
                        for (std::int32_t clearU = 0; clearU < width; ++clearU) {
                            mask[maskIndex(u + clearU, v + clearV)] = {};
                        }
                    }
                    u += width;
                }
            }
        }
    }

    return mesh;
}

} // namespace tenebris::voxel
