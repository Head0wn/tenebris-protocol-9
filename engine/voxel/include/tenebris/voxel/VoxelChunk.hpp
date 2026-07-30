#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace tenebris::voxel {

using MaterialId = std::uint16_t;
inline constexpr MaterialId airMaterial = 0U;

struct LocalPosition final {
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};

    [[nodiscard]] friend constexpr bool operator==(
        const LocalPosition&,
        const LocalPosition&
    ) noexcept = default;
};

class VoxelChunk final {
public:
    static constexpr std::int32_t extent = 32;
    static constexpr std::size_t extentValue = static_cast<std::size_t>(extent);
    static constexpr std::size_t volume = extentValue * extentValue * extentValue;
    using Storage = std::array<MaterialId, volume>;

    VoxelChunk() = default;

    [[nodiscard]] static constexpr bool contains(LocalPosition position) noexcept {
        return position.x >= 0 && position.x < extent && position.y >= 0
            && position.y < extent && position.z >= 0 && position.z < extent;
    }

    [[nodiscard]] bool set(LocalPosition position, MaterialId material) noexcept;
    [[nodiscard]] std::optional<MaterialId> tryGet(LocalPosition position) const noexcept;
    [[nodiscard]] MaterialId sample(LocalPosition position) const noexcept;
    [[nodiscard]] bool isSolid(LocalPosition position) const noexcept;

    void clear() noexcept;
    void fill(MaterialId material) noexcept;

    [[nodiscard]] std::size_t solidCount() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] const Storage& cells() const noexcept;

private:
    [[nodiscard]] static constexpr std::size_t indexUnchecked(LocalPosition position) noexcept {
        const std::size_t x = static_cast<std::size_t>(position.x);
        const std::size_t y = static_cast<std::size_t>(position.y);
        const std::size_t z = static_cast<std::size_t>(position.z);
        return x + extentValue * (y + extentValue * z);
    }

    Storage cells_{};
    std::size_t solidCount_{0U};
    std::uint64_t revision_{0U};
};

} // namespace tenebris::voxel
