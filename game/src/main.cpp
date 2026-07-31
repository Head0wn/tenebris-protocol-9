#include <tenebris/core/Application.hpp>
#include <tenebris/platform/PlatformSystem.hpp>
#include <tenebris/renderer/RendererSystem.hpp>
#include <tenebris/scene/GpuUploadPlan.hpp>
#include <tenebris/scene/InspectionCamera.hpp>
#include <tenebris/scene/RenderScene.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

[[nodiscard]] bool hasArgument(int argc, char** argv, std::string_view expected) {
    return std::any_of(
        argv + 1,
        argv + argc,
        [expected](const char* argument) { return std::string_view{argument} == expected; }
    );
}

[[nodiscard]] tenebris::scene::InspectionCameraInput makeCameraInput(
    const tenebris::platform::PlatformInputState& input
) noexcept {
    return {
        .moveForward = input.moveForward,
        .moveRight = input.moveRight,
        .moveUp = input.moveUp,
        .yawDelta = input.mouseDeltaX,
        .pitchDelta = -input.mouseDeltaY,
        .zoomDelta = input.wheelDelta,
        .speedBoost = input.speedBoost,
        .reset = input.resetCameraPressed,
        .toggleAutoOrbit = input.toggleAutoOrbitPressed,
    };
}

void printInspectionControls() {
    std::cout
        << "TENEBRIS Site 47 visual inspection controls:\n"
        << "  Right mouse + movement : look around\n"
        << "  W/A/S/D or arrows     : move\n"
        << "  Q/E or Ctrl/Space     : descend / ascend\n"
        << "  Shift                  : fast movement\n"
        << "  Mouse wheel            : dolly\n"
        << "  O                       : toggle cinematic orbit\n"
        << "  R                       : reset camera\n"
        << "  Escape                  : release mouse, then quit\n";
}

} // namespace

int main(int argc, char** argv) {
    const bool headlessSmokeTest = hasArgument(argc, argv, "--headless-smoke-test");
    const bool gpuSmokeTest = hasArgument(argc, argv, "--gpu-smoke-test");
    const bool printSceneStats = hasArgument(argc, argv, "--scene-stats");
    const bool startAutoOrbit = gpuSmokeTest || hasArgument(argc, argv, "--auto-orbit");
    constexpr std::uint64_t gpuSmokeFrameBudget = 300U;

    if (hasArgument(argc, argv, "--version")) {
        std::cout << "TENEBRIS 0.7.0\n";
        return 0;
    }

    const tenebris::scene::SceneAsset site47Blockout = tenebris::scene::buildSite47Blockout();
    if (site47Blockout.renderMesh.empty() || !site47Blockout.renderMesh.hasValidIndices()
        || !tenebris::scene::isFinite(site47Blockout.camera.viewProjectionMatrix())) {
        std::cerr << "TENEBRIS failed to build the deterministic Site 47 render scene.\n";
        return 1;
    }

    const tenebris::scene::GpuUploadPlan uploadPlan =
        tenebris::scene::makeGpuUploadPlan(site47Blockout.renderMesh);
    const tenebris::scene::VoxelPushConstants voxelConstants =
        tenebris::scene::makeVoxelPushConstants(site47Blockout.camera);
    if (!uploadPlan.valid() || !uploadPlan.drawable()
        || !tenebris::scene::isFinite(voxelConstants.viewProjection)) {
        std::cerr << "TENEBRIS rejected the Site 47 GPU upload contract: "
                  << uploadPlan.errorMessage() << '\n';
        return 2;
    }

    if (printSceneStats || headlessSmokeTest) {
        std::cout << "Site 47 render scene: " << site47Blockout.chunk.solidCount() << " solid voxels, "
                  << site47Blockout.renderMesh.vertices.size() << " vertices, "
                  << site47Blockout.renderMesh.triangleCount() << " triangles, "
                  << uploadPlan.stagingSize << " staging bytes, hash "
                  << tenebris::scene::stableRenderMeshHash(site47Blockout.renderMesh) << ".\n";
    }

    tenebris::scene::InspectionCameraController inspectionCamera({
        .initialCamera = site47Blockout.camera.config(),
        .moveSpeed = 8.0F,
        .speedBoostMultiplier = 4.0F,
        .zoomSpeed = 3.0F,
        .lookSensitivity = 0.0025F,
        .autoOrbitRadiansPerSecond = 0.25F,
        .maximumPitchRadians = 1.45F,
    });
    if (startAutoOrbit && !inspectionCamera.setAutoOrbit(true)) {
        std::cerr << inspectionCamera.lastError() << '\n';
        return 3;
    }

    tenebris::platform::PlatformSystem platform({
        .title = gpuSmokeTest ? "TENEBRIS — Vulkan Validation" : "TENEBRIS — Site 47 Inspection",
        .width = 1600,
        .height = 900,
        .resizable = true,
        .highPixelDensity = true,
        .vulkan = true,
    });

    if (!platform.initialize(headlessSmokeTest)) {
        std::cerr << platform.lastError() << '\n';
        return 4;
    }

#ifndef NDEBUG
    constexpr bool requestValidation = true;
#else
    constexpr bool requestValidation = false;
#endif

    tenebris::renderer::RendererSystem renderer({
        .applicationName = "TENEBRIS — Le Protocole 9",
        .shaderDirectory = "assets/shaders/compiled",
        .framesInFlight = 2U,
        .requestValidation = requestValidation,
    });

    if (!renderer.initialize(platform.window(), headlessSmokeTest)) {
        std::cerr << renderer.lastError() << '\n';
        platform.shutdown();
        return 5;
    }

    if (!renderer.setVoxelScene(site47Blockout.renderMesh, inspectionCamera.camera())) {
        std::cerr << renderer.lastError() << '\n';
        renderer.shutdown();
        platform.shutdown();
        return 6;
    }

    tenebris::core::Application application({
        .name = "TENEBRIS — Le Protocole 9",
        .headless = headlessSmokeTest,
    });

    if (!application.initialize()) {
        std::cerr << "TENEBRIS failed to initialize.\n";
        renderer.shutdown();
        platform.shutdown();
        return 7;
    }

    if (!headlessSmokeTest && !gpuSmokeTest) {
        printInspectionControls();
    }

    using Clock = std::chrono::steady_clock;
    auto previousFrameTime = Clock::now();
    std::uint64_t cameraUpdates = 0U;
    bool runtimeFailure = false;

    while (application.state() != tenebris::core::ApplicationState::Stopped) {
        if (application.state() == tenebris::core::ApplicationState::Running) {
            const bool keepRunning = platform.pumpEvents();
            if (platform.consumeFramebufferResize()) {
                renderer.notifyFramebufferResized();
            }
            if (!keepRunning) {
                application.requestShutdown();
            }
        }

        const auto currentFrameTime = Clock::now();
        float deltaSeconds = std::chrono::duration<float>(currentFrameTime - previousFrameTime).count();
        previousFrameTime = currentFrameTime;
        deltaSeconds = gpuSmokeTest ? (1.0F / 60.0F) : std::clamp(deltaSeconds, 0.0F, 0.1F);

        if (application.state() == tenebris::core::ApplicationState::Running && !headlessSmokeTest) {
            const tenebris::scene::InspectionCameraInput cameraInput = gpuSmokeTest
                ? tenebris::scene::InspectionCameraInput{}
                : makeCameraInput(platform.input());
            const tenebris::scene::InspectionCameraUpdate cameraResult =
                inspectionCamera.update(cameraInput, deltaSeconds);

            if (cameraResult == tenebris::scene::InspectionCameraUpdate::Rejected) {
                std::cerr << inspectionCamera.lastError() << '\n';
                runtimeFailure = true;
                application.requestShutdown();
            } else if (cameraResult == tenebris::scene::InspectionCameraUpdate::Changed) {
                if (!renderer.setVoxelScene(site47Blockout.renderMesh, inspectionCamera.camera())) {
                    std::cerr << renderer.lastError() << '\n';
                    runtimeFailure = true;
                    application.requestShutdown();
                } else {
                    ++cameraUpdates;
                }
            }
        }

        if (application.state() == tenebris::core::ApplicationState::Running && !renderer.drawFrame()) {
            std::cerr << renderer.lastError() << '\n';
            runtimeFailure = true;
            application.requestShutdown();
        }

        const bool frameExecuted = application.tick();
        if (headlessSmokeTest && frameExecuted && application.frameIndex() >= 3U) {
            application.requestShutdown();
        }
        if (gpuSmokeTest && frameExecuted && application.frameIndex() >= gpuSmokeFrameBudget) {
            application.requestShutdown();
        }

        if (!frameExecuted && application.state() != tenebris::core::ApplicationState::Stopped) {
            std::cerr << "TENEBRIS runtime stopped unexpectedly.\n";
            runtimeFailure = true;
            application.requestShutdown();
        }

        platform.delay(1U);
    }

    const tenebris::renderer::RendererStats rendererStats = renderer.stats();
    renderer.shutdown();
    platform.shutdown();

    if (headlessSmokeTest) {
        std::cout << "TENEBRIS headless smoke test passed after " << application.frameIndex()
                  << " frames.\n";
    }
    if (gpuSmokeTest && !runtimeFailure) {
        std::cout << "TENEBRIS Vulkan smoke test passed: " << rendererStats.submittedFrames
                  << " frames, " << rendererStats.sceneUploads << " scene upload, "
                  << cameraUpdates << " camera updates, "
                  << rendererStats.drawCalls << " indexed draws, "
                  << rendererStats.drawnTriangles << " triangles drawn, "
                  << rendererStats.swapchainRebuilds << " swapchain rebuilds.\n";
    }

    return runtimeFailure ? 8 : 0;
}
