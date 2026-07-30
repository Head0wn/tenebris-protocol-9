#pragma once

#include <tenebris/voxel/VoxelChunk.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tenebris::voxel {

struct VoxelVertex final {
    std::uint16_t x{0U};
    std::uint16_t y{0U};
    std::uint16_t z{0U};
    std::int8_t normalX{0};
    std::int8_t normalY{0};
    std::int8_t normalZ{0};
    MaterialId material{airMaterial};

    [[nodiscard]] friend bool operator==(const VoxelVertex&, const VoxelVertex&) noexcept = default;
};

struct VoxelMesh final {
    std::vector<VoxelVertex> vertices;
    std::vector<std::uint32_t> indices;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t quadCount() const noexcept;
    [[nodiscard]] std::size_t triangleCount() const noexcept;
};

class GreedyMesher final {
public:
    [[nodiscard]] static VoxelMesh build(const VoxelChunk& chunk);
};

} // namespace tenebris::voxel
