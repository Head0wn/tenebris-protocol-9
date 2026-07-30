#include <tenebris/voxel/GreedyMesher.hpp>
#include <tenebris/voxel/VoxelChunk.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

class TestContext final {
public:
    void expect(bool condition, std::string_view message) {
        if (!condition) {
            ++failures_;
            std::cerr << "FAILED: " << message << '\n';
        }
    }

    [[nodiscard]] int result() const noexcept {
        return failures_ == 0U ? 0 : 1;
    }

private:
    std::size_t failures_{0U};
};

struct Vector3 final {
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};
};

[[nodiscard]] Vector3 subtract(
    const tenebris::voxel::VoxelVertex& right,
    const tenebris::voxel::VoxelVertex& left
) noexcept {
    return {
        .x = static_cast<std::int32_t>(right.x) - static_cast<std::int32_t>(left.x),
        .y = static_cast<std::int32_t>(right.y) - static_cast<std::int32_t>(left.y),
        .z = static_cast<std::int32_t>(right.z) - static_cast<std::int32_t>(left.z),
    };
}

[[nodiscard]] Vector3 cross(const Vector3& left, const Vector3& right) noexcept {
    return {
        .x = left.y * right.z - left.z * right.y,
        .y = left.z * right.x - left.x * right.z,
        .z = left.x * right.y - left.y * right.x,
    };
}

[[nodiscard]] bool hasValidWinding(const tenebris::voxel::VoxelMesh& mesh) {
    if (mesh.indices.size() % 3U != 0U) {
        return false;
    }

    for (std::size_t index = 0U; index < mesh.indices.size(); index += 3U) {
        const std::uint32_t indexA = mesh.indices[index];
        const std::uint32_t indexB = mesh.indices[index + 1U];
        const std::uint32_t indexC = mesh.indices[index + 2U];
        if (indexA >= mesh.vertices.size() || indexB >= mesh.vertices.size()
            || indexC >= mesh.vertices.size()) {
            return false;
        }

        const tenebris::voxel::VoxelVertex& a = mesh.vertices[indexA];
        const tenebris::voxel::VoxelVertex& b = mesh.vertices[indexB];
        const tenebris::voxel::VoxelVertex& c = mesh.vertices[indexC];
        if (a.normalX != b.normalX || a.normalY != b.normalY || a.normalZ != b.normalZ
            || a.normalX != c.normalX || a.normalY != c.normalY || a.normalZ != c.normalZ) {
            return false;
        }

        const Vector3 normal = cross(subtract(b, a), subtract(c, a));
        const std::int32_t dot = normal.x * static_cast<std::int32_t>(a.normalX)
            + normal.y * static_cast<std::int32_t>(a.normalY)
            + normal.z * static_cast<std::int32_t>(a.normalZ);
        if (dot <= 0) {
            return false;
        }
    }
    return true;
}

void testChunkStorage(TestContext& context) {
    using namespace tenebris::voxel;

    VoxelChunk chunk;
    context.expect(chunk.solidCount() == 0U, "new chunk must be empty");
    context.expect(chunk.revision() == 0U, "new chunk revision must be zero");
    context.expect(
        !chunk.set({.x = -1, .y = 0, .z = 0}, 1U),
        "out-of-bounds negative write must fail"
    );
    context.expect(
        !chunk.set({.x = VoxelChunk::extent, .y = 0, .z = 0}, 1U),
        "out-of-bounds positive write must fail"
    );
    context.expect(
        !chunk.tryGet({.x = 0, .y = VoxelChunk::extent, .z = 0}).has_value(),
        "out-of-bounds read must return no value"
    );
    context.expect(
        chunk.sample({.x = 0, .y = 0, .z = -1}) == airMaterial,
        "out-of-bounds sampling must behave as air"
    );
    context.expect(chunk.revision() == 0U, "rejected writes must not change revision");

    context.expect(chunk.set({.x = 3, .y = 4, .z = 5}, 7U), "valid write must succeed");
    context.expect(chunk.solidCount() == 1U, "solid count must increment");
    context.expect(chunk.revision() == 1U, "changed write must increment revision");
    context.expect(
        chunk.tryGet({.x = 3, .y = 4, .z = 5}).value_or(airMaterial) == 7U,
        "stored material must be readable"
    );

    context.expect(chunk.set({.x = 3, .y = 4, .z = 5}, 7U), "same-value write must succeed");
    context.expect(chunk.revision() == 1U, "same-value write must not increment revision");

    context.expect(
        chunk.set({.x = 3, .y = 4, .z = 5}, airMaterial),
        "clearing a voxel must succeed"
    );
    context.expect(chunk.solidCount() == 0U, "solid count must decrement");
    context.expect(chunk.revision() == 2U, "clearing must increment revision");

    chunk.fill(2U);
    context.expect(chunk.solidCount() == VoxelChunk::volume, "fill must mark every voxel solid");
    const std::uint64_t filledRevision = chunk.revision();
    chunk.fill(2U);
    context.expect(
        chunk.revision() == filledRevision,
        "filling with the current material must not increment revision"
    );
    chunk.clear();
    context.expect(chunk.solidCount() == 0U, "clear must empty the chunk");
    context.expect(chunk.revision() == filledRevision + 1U, "clear must increment revision once");
}

void expectMeshShape(
    TestContext& context,
    const tenebris::voxel::VoxelMesh& mesh,
    std::size_t expectedQuads,
    std::string_view label
) {
    context.expect(mesh.quadCount() == expectedQuads, label);
    context.expect(mesh.vertices.size() == expectedQuads * 4U, "quad vertex count must be exact");
    context.expect(mesh.indices.size() == expectedQuads * 6U, "quad index count must be exact");
    context.expect(mesh.triangleCount() == expectedQuads * 2U, "triangle count must be exact");
    context.expect(hasValidWinding(mesh), "all triangles must follow their encoded normal");
}

void testGreedyMeshing(TestContext& context) {
    using namespace tenebris::voxel;

    VoxelChunk emptyChunk;
    const VoxelMesh emptyMesh = GreedyMesher::build(emptyChunk);
    context.expect(emptyMesh.empty(), "empty chunk must produce no geometry");

    VoxelChunk singleBlock;
    static_cast<void>(singleBlock.set({.x = 0, .y = 0, .z = 0}, 1U));
    const VoxelMesh singleMesh = GreedyMesher::build(singleBlock);
    expectMeshShape(context, singleMesh, 6U, "single block must produce six quads");
    context.expect(
        std::all_of(
            singleMesh.vertices.begin(),
            singleMesh.vertices.end(),
            [](const VoxelVertex& vertex) { return vertex.material == 1U; }
        ),
        "single block mesh must preserve its material"
    );

    VoxelChunk adjacentSame;
    static_cast<void>(adjacentSame.set({.x = 0, .y = 0, .z = 0}, 4U));
    static_cast<void>(adjacentSame.set({.x = 1, .y = 0, .z = 0}, 4U));
    const VoxelMesh adjacentSameMesh = GreedyMesher::build(adjacentSame);
    expectMeshShape(
        context,
        adjacentSameMesh,
        6U,
        "equal adjacent blocks must merge into a rectangular prism"
    );

    VoxelChunk adjacentDifferent;
    static_cast<void>(adjacentDifferent.set({.x = 0, .y = 0, .z = 0}, 4U));
    static_cast<void>(adjacentDifferent.set({.x = 1, .y = 0, .z = 0}, 5U));
    const VoxelMesh adjacentDifferentMesh = GreedyMesher::build(adjacentDifferent);
    expectMeshShape(
        context,
        adjacentDifferentMesh,
        10U,
        "different materials must not merge exposed coplanar faces"
    );
    const std::size_t materialFourVertices = static_cast<std::size_t>(std::count_if(
        adjacentDifferentMesh.vertices.begin(),
        adjacentDifferentMesh.vertices.end(),
        [](const VoxelVertex& vertex) { return vertex.material == 4U; }
    ));
    const std::size_t materialFiveVertices = static_cast<std::size_t>(std::count_if(
        adjacentDifferentMesh.vertices.begin(),
        adjacentDifferentMesh.vertices.end(),
        [](const VoxelVertex& vertex) { return vertex.material == 5U; }
    ));
    context.expect(materialFourVertices == 20U, "first material must own five quads");
    context.expect(materialFiveVertices == 20U, "second material must own five quads");

    VoxelChunk solidChunk;
    solidChunk.fill(9U);
    const VoxelMesh solidMesh = GreedyMesher::build(solidChunk);
    expectMeshShape(context, solidMesh, 6U, "solid chunk must collapse to six quads");

    VoxelChunk cavityChunk;
    cavityChunk.fill(9U);
    static_cast<void>(cavityChunk.set({.x = 16, .y = 16, .z = 16}, airMaterial));
    const VoxelMesh cavityMesh = GreedyMesher::build(cavityChunk);
    expectMeshShape(context, cavityMesh, 12U, "single-cell cavity must add six internal quads");

    const VoxelMesh repeatedMesh = GreedyMesher::build(cavityChunk);
    context.expect(
        cavityMesh.vertices == repeatedMesh.vertices,
        "meshing the same chunk must produce identical vertices"
    );
    context.expect(
        cavityMesh.indices == repeatedMesh.indices,
        "meshing the same chunk must produce identical indices"
    );
}

} // namespace

int main() {
    TestContext context;
    testChunkStorage(context);
    testGreedyMeshing(context);

    if (context.result() == 0) {
        std::cout << "Voxel storage and greedy meshing tests passed.\n";
    }
    return context.result();
}
