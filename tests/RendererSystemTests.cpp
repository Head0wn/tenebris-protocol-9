#include <tenebris/renderer/RendererSystem.hpp>

#include <cstdint>
#include <iostream>

int main() {
    constexpr std::uint32_t lifecycleCount = 500U;

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
        if (!renderer.drawFrame()) {
            std::cerr << "Headless renderer frame failed at cycle " << cycle << '\n';
            return 3;
        }

        renderer.shutdown();
        if (renderer.state() != tenebris::renderer::RendererState::Stopped) {
            std::cerr << "Renderer did not stop cleanly at cycle " << cycle << '\n';
            return 4;
        }
    }

    std::cout << "Renderer completed " << lifecycleCount << " headless lifecycle cycles.\n";
    return 0;
}
