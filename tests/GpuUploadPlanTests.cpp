#include <tenebris/scene/GpuUploadPlan.hpp>

#include <cmath>
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

[[nodiscard]] bool nearlyEqual(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::fabs(left - right) <= tolerance;
}

void testEmptyAndInvalidMeshes(TestContext& context) {
    using namespace tenebris::scene;

    RenderMesh empty{};
    empty.sourceRevision = 17U;
    const GpuUploadPlan emptyPlan = makeGpuUploadPlan(empty);
    context.expect(emptyPlan.valid(), "empty scene must produce a valid no-draw plan");
    context.expect(!emptyPlan.drawable(), "empty scene must not produce a draw");
    context.expect(emptyPlan.stagingSize == 0U, "empty scene must not allocate staging memory");
    context.expect(emptyPlan.sourceRevision == 17U, "empty plan must preserve source revision");

    RenderMesh incomplete{};
    incomplete.vertices.push_back({});
    const GpuUploadPlan incompletePlan = makeGpuUploadPlan(incomplete);
    context.expect(!incompletePlan.valid(), "incomplete mesh must be rejected");
    context.expect(
        incompletePlan.error == UploadPlanError::IncompleteMesh,
        "incomplete mesh must expose the correct error"
    );
    context.expect(!incompletePlan.errorMessage().empty(), "failed plan must provide an error message");

    RenderMesh invalidIndices{};
    invalidIndices.vertices.push_back({});
    invalidIndices.indices = {0U, 0U, 1U};
    const GpuUploadPlan invalidIndexPlan = makeGpuUploadPlan(invalidIndices);
    context.expect(!invalidIndexPlan.valid(), "out-of-range indices must be rejected");
    context.expect(
        invalidIndexPlan.error == UploadPlanError::InvalidIndices,
        "invalid indices must expose the correct error"
    );
}

void testSite47UploadPlan(TestContext& context) {
    using namespace tenebris::scene;

    const SceneAsset site47 = buildSite47Blockout();
    const GpuUploadPlan plan = makeGpuUploadPlan(site47.renderMesh);

    context.expect(plan.valid(), "Site 47 must produce a valid upload plan");
    context.expect(plan.drawable(), "Site 47 must produce an indexed draw");
    context.expect(plan.errorMessage().empty(), "valid plan must not expose an error message");
    context.expect(
        plan.vertexRegion.size
            == static_cast<std::uint64_t>(site47.renderMesh.vertices.size()) * sizeof(GpuVoxelVertex),
        "vertex region must match the exact packed vertex byte size"
    );
    context.expect(
        plan.indexRegion.size
            == static_cast<std::uint64_t>(site47.renderMesh.indices.size()) * sizeof(std::uint32_t),
        "index region must match the exact index byte size"
    );
    context.expect(
        (plan.vertexRegion.offset % gpuBufferAlignment) == 0U,
        "vertex region must respect the upload alignment"
    );
    context.expect(
        (plan.indexRegion.offset % gpuBufferAlignment) == 0U,
        "index region must respect the upload alignment"
    );
    context.expect(
        plan.vertexRegion.end() <= plan.indexRegion.offset,
        "vertex and index upload regions must not overlap"
    );
    context.expect(
        plan.indexRegion.end() <= plan.stagingSize,
        "staging allocation must contain the complete index region"
    );
    context.expect(
        (plan.stagingSize % gpuBufferAlignment) == 0U,
        "staging allocation must end on the selected alignment"
    );
    context.expect(
        plan.draw.indexCount == static_cast<std::uint32_t>(site47.renderMesh.indices.size()),
        "draw plan must use every Site 47 index"
    );
    context.expect(plan.draw.instanceCount == 1U, "initial voxel draw must use one instance");
    context.expect(
        plan.sourceRevision == site47.renderMesh.sourceRevision,
        "upload plan must retain the render mesh revision"
    );
}

void testShaderFrameContract(TestContext& context) {
    using namespace tenebris::scene;

    const SceneAsset site47 = buildSite47Blockout();
    const VoxelPushConstants constants = makeVoxelPushConstants(
        site47.camera,
        {.x = 2.0F, .y = -4.0F, .z = 1.0F},
        4.0F
    );

    context.expect(isFinite(constants.viewProjection), "push-constant matrix must be finite");
    const float directionLength = std::sqrt(
        constants.lightDirectionAmbient[0] * constants.lightDirectionAmbient[0]
        + constants.lightDirectionAmbient[1] * constants.lightDirectionAmbient[1]
        + constants.lightDirectionAmbient[2] * constants.lightDirectionAmbient[2]
    );
    context.expect(nearlyEqual(directionLength, 1.0F), "light direction must be normalized");
    context.expect(
        nearlyEqual(constants.lightDirectionAmbient[3], 1.0F),
        "ambient light must be clamped to the supported range"
    );

    const VoxelPushConstants fallback = makeVoxelPushConstants(
        site47.camera,
        {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        -2.0F
    );
    const float fallbackLength = std::sqrt(
        fallback.lightDirectionAmbient[0] * fallback.lightDirectionAmbient[0]
        + fallback.lightDirectionAmbient[1] * fallback.lightDirectionAmbient[1]
        + fallback.lightDirectionAmbient[2] * fallback.lightDirectionAmbient[2]
    );
    context.expect(nearlyEqual(fallbackLength, 1.0F), "zero light direction must use a valid fallback");
    context.expect(
        nearlyEqual(fallback.lightDirectionAmbient[3], 0.0F),
        "negative ambient light must clamp to zero"
    );

    const Site47MaterialPalette& palette = site47MaterialPalette();
    context.expect(palette.size() == 6U, "Site 47 palette must include air and five materials");
    context.expect(palette[0U].baseColor[3] == 0.0F, "air material must remain transparent in the contract");
    context.expect(palette[1U].baseColor[3] == 1.0F, "sand material must be opaque");
    context.expect(
        palette[4U].emissiveAndResponse[0] > palette[5U].emissiveAndResponse[0],
        "emergency material must emit more light than organic contamination"
    );
}

} // namespace

int main() {
    TestContext context;
    testEmptyAndInvalidMeshes(context);
    testSite47UploadPlan(context);
    testShaderFrameContract(context);

    if (context.result() == 0) {
        std::cout << "GPU upload and voxel shader contract tests passed.\n";
    }
    return context.result();
}
