#include <tenebris/scene/RenderScene.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
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

[[nodiscard]] bool nearlyEqual(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::fabs(left - right) <= tolerance;
}

void testCamera(TestContext& context) {
    using namespace tenebris::scene;

    SceneCamera camera;
    context.expect(camera.lastError().empty(), "default camera must be valid");
    context.expect(isFinite(camera.viewMatrix()), "view matrix must contain finite values");
    context.expect(isFinite(camera.projectionMatrix()), "projection matrix must contain finite values");
    context.expect(
        isFinite(camera.viewProjectionMatrix()),
        "view-projection matrix must contain finite values"
    );

    const CameraConfig previous = camera.config();
    CameraConfig invalid = previous;
    invalid.target = invalid.position;
    context.expect(!camera.setConfig(invalid), "camera must reject a zero viewing direction");
    context.expect(!camera.lastError().empty(), "invalid camera must expose an actionable error");
    context.expect(camera.config().position == previous.position, "invalid camera must not replace valid state");

    invalid = previous;
    invalid.aspectRatio = 0.0F;
    context.expect(!camera.setConfig(invalid), "camera must reject a zero aspect ratio");

    CameraConfig valid = previous;
    valid.aspectRatio = 21.0F / 9.0F;
    valid.position = {3.0F, 8.0F, 12.0F};
    context.expect(camera.setConfig(valid), "camera must accept a valid cinematic configuration");
    context.expect(camera.lastError().empty(), "valid camera update must clear the previous error");
}

void testRenderPacket(TestContext& context) {
    using namespace tenebris;

    voxel::VoxelChunk chunk;
    static_cast<void>(chunk.set({.x = 1, .y = 2, .z = 3}, 7U));
    const voxel::VoxelMesh source = voxel::GreedyMesher::build(chunk);
    const scene::RenderMesh render = scene::buildRenderMesh(
        source,
        0.5F,
        {.x = -1.0F, .y = 1.0F, .z = 2.0F},
        chunk.revision()
    );

    context.expect(!render.empty(), "single voxel must produce a render packet");
    context.expect(render.vertices.size() == source.vertices.size(), "vertex count must be preserved");
    context.expect(render.indices == source.indices, "index order must be preserved");
    context.expect(render.hasValidIndices(), "render packet indices must remain valid");
    context.expect(render.triangleCount() == source.triangleCount(), "triangle count must be preserved");
    context.expect(render.bounds.valid, "render packet must compute bounds");
    context.expect(render.sourceRevision == chunk.revision(), "render packet must retain source revision");
    context.expect(nearlyEqual(render.bounds.minimum.x, -0.5F), "minimum X bound must include origin and scale");
    context.expect(nearlyEqual(render.bounds.minimum.y, 2.0F), "minimum Y bound must include origin and scale");
    context.expect(nearlyEqual(render.bounds.minimum.z, 3.5F), "minimum Z bound must include origin and scale");
    context.expect(nearlyEqual(render.bounds.maximum.x, 0.0F), "maximum X bound must include voxel extent");
    context.expect(nearlyEqual(render.bounds.maximum.y, 2.5F), "maximum Y bound must include voxel extent");
    context.expect(nearlyEqual(render.bounds.maximum.z, 4.0F), "maximum Z bound must include voxel extent");

    const scene::RenderMesh invalidScale = scene::buildRenderMesh(source, 0.0F, {}, chunk.revision());
    context.expect(invalidScale.empty(), "zero voxel scale must be rejected");

    const scene::RenderMesh invalidOrigin = scene::buildRenderMesh(
        source,
        1.0F,
        {.x = std::numeric_limits<float>::quiet_NaN(), .y = 0.0F, .z = 0.0F},
        chunk.revision()
    );
    context.expect(invalidOrigin.empty(), "non-finite render origin must be rejected");

    voxel::VoxelMesh malformed = source;
    malformed.indices.push_back(std::numeric_limits<std::uint32_t>::max());
    const scene::RenderMesh invalidIndices = scene::buildRenderMesh(malformed, 1.0F, {}, chunk.revision());
    context.expect(invalidIndices.empty(), "non-triangular index buffers must be rejected");
}

void testSite47Blockout(TestContext& context) {
    using namespace tenebris;
    using namespace tenebris::scene;

    const SceneAsset first = buildSite47Blockout();
    const SceneAsset second = buildSite47Blockout();

    context.expect(
        first.chunk.solidCount() > 2'000U,
        "Site 47 composition must contain enough authored mass to read as a facility"
    );
    context.expect(!first.voxelMesh.empty(), "Site 47 blockout must produce voxel geometry");
    context.expect(!first.renderMesh.empty(), "Site 47 blockout must produce GPU-ready geometry");
    context.expect(first.renderMesh.hasValidIndices(), "Site 47 render packet must have valid indices");
    context.expect(first.renderMesh.bounds.valid, "Site 47 render packet must have valid bounds");
    context.expect(
        first.renderMesh.sourceRevision == first.chunk.revision(),
        "Site 47 render packet must match the chunk revision"
    );
    context.expect(
        stableRenderMeshHash(first.renderMesh) == stableRenderMeshHash(second.renderMesh),
        "Site 47 render data must be deterministic"
    );
    context.expect(
        first.renderMesh.vertices == second.renderMesh.vertices
            && first.renderMesh.indices == second.renderMesh.indices,
        "rebuilding Site 47 must reproduce identical geometry"
    );
    context.expect(isFinite(first.camera.viewProjectionMatrix()), "Site 47 camera must be renderable");

    context.expect(
        !first.chunk.isSolid({16, 5, 28}),
        "the tactical entrance must remain open at human height"
    );
    context.expect(
        first.chunk.sample({13, 5, 28}) == 3U,
        "the tactical entrance must be framed by steel"
    );
    context.expect(
        first.chunk.sample({15, 8, 28}) == 4U,
        "the entrance must expose a visible emergency light bar"
    );
    context.expect(
        first.chunk.sample({16, 5, 12}) == 5U,
        "the Protocol 9 mass must occupy the containment cage"
    );
    context.expect(
        !first.chunk.isSolid({22, 8, 20}),
        "the right wing must stay open as an intentional cinematic cutaway"
    );

    const CameraConfig& camera = first.camera.config();
    context.expect(camera.position.y < 7.0F, "default Site 47 camera must use a grounded tactical height");
    context.expect(camera.position.z > 10.0F, "default Site 47 camera must begin outside the entrance");
    context.expect(camera.verticalFieldOfViewRadians < 1.0F, "default Site 47 camera must avoid a wide prototype FOV");

    std::array<bool, 6U> materials{};
    for (const GpuVoxelVertex& vertex : first.renderMesh.vertices) {
        if (vertex.material < materials.size()) {
            materials[vertex.material] = true;
        }
    }
    for (std::size_t material = 1U; material < materials.size(); ++material) {
        context.expect(materials[material], "Site 47 blockout must expose every authored material");
    }
}

} // namespace

int main() {
    TestContext context;
    testCamera(context);
    testRenderPacket(context);
    testSite47Blockout(context);

    if (context.result() == 0) {
        std::cout << "Render-scene and Site 47 blockout tests passed.\n";
    }
    return context.result();
}
