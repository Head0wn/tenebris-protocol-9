#include <tenebris/renderer/RendererSystem.hpp>
#include <tenebris/scene/RenderScene.hpp>

#include <cstdint>
#include <iostream>

int main() {
    constexpr std::uint32_t lifecycleCount = 500U;
    const tenebris::scene::SceneAsset site47 = tenebris::scene::buildSite47Blockout();

    tenebris::renderer::RendererSystem renderer({
        .applicationName = "TENEBRIS Renderer Tests",
        .framesInFlight = 2U,
        .requestValidation = false,
    });

    for (std::uint32_t cycle = 0U; cycle < lifecycleCount; ++cycle) {
        if (!renderer.initialize(nullptr, true)) {
            std::cerr << "Headless renderer initialization failed at cycle " << cycle << ": "
                      << renderer.lastError() << '\n';
            return 1;
        }
        if (renderer.state() != tenebris::renderer::RendererState::Headless) {
            std::cerr << "Unexpected renderer state after initialization at cycle " << cycle << '\n';
            return 2;
        }
        if (!renderer.setVoxelScene(site47.renderMesh, site47.camera)) {
            std::cerr << "Headless Site 47 submission failed at cycle " << cycle << ": "
                      << renderer.lastError() << '\n';
            return 3;
        }
        if (!renderer.drawFrame()) {
            std::cerr << "Headless renderer frame failed at cycle " << cycle << '\n';
            return 4;
        }

        renderer.shutdown();
        if (renderer.state() != tenebris::renderer::RendererState::Stopped) {
            std::cerr << "Renderer did not stop cleanly at cycle " << cycle << '\n';
            return 5;
        }
    }

    std::cout << "Renderer completed " << lifecycleCount
              << " headless lifecycle cycles with deterministic Site 47 submission.\n";
    return 0;
}
