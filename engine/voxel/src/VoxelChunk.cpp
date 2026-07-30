#include <tenebris/voxel/VoxelChunk.hpp>

#include <algorithm>

namespace tenebris::voxel {

bool VoxelChunk::set(LocalPosition position, MaterialId material) noexcept {
    if (!contains(position)) {
        return false;
    }

    MaterialId& current = cells_[indexUnchecked(position)];
    if (current == material) {
        return true;
    }

    if (current == airMaterial && material != airMaterial) {
        ++solidCount_;
    } else if (current != airMaterial && material == airMaterial) {
        --solidCount_;
    }

    current = material;
    ++revision_;
    return true;
}

std::optional<MaterialId> VoxelChunk::tryGet(LocalPosition position) const noexcept {
    if (!contains(position)) {
        return std::nullopt;
    }
    return cells_[indexUnchecked(position)];
}

MaterialId VoxelChunk::sample(LocalPosition position) const noexcept {
    if (!contains(position)) {
        return airMaterial;
    }
    return cells_[indexUnchecked(position)];
}

bool VoxelChunk::isSolid(LocalPosition position) const noexcept {
    return sample(position) != airMaterial;
}

void VoxelChunk::clear() noexcept {
    if (solidCount_ == 0U) {
        return;
    }

    cells_.fill(airMaterial);
    solidCount_ = 0U;
    ++revision_;
}

void VoxelChunk::fill(MaterialId material) noexcept {
    const bool alreadyFilled = std::all_of(
        cells_.begin(),
        cells_.end(),
        [material](MaterialId current) { return current == material; }
    );
    if (alreadyFilled) {
        return;
    }

    cells_.fill(material);
    solidCount_ = material == airMaterial ? 0U : volume;
    ++revision_;
}

std::size_t VoxelChunk::solidCount() const noexcept {
    return solidCount_;
}

std::uint64_t VoxelChunk::revision() const noexcept {
    return revision_;
}

const VoxelChunk::Storage& VoxelChunk::cells() const noexcept {
    return cells_;
}

} // namespace tenebris::voxel
