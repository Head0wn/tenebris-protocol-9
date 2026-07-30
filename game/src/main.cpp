#include <tenebris/core/Application.hpp>
#include <tenebris/platform/PlatformSystem.hpp>

#include <algorithm>
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

} // namespace

int main(int argc, char** argv) {
    const bool smokeTest = hasArgument(argc, argv, "--headless-smoke-test");

    if (hasArgument(argc, argv, "--version")) {
        std::cout << "TENEBRIS 0.2.0\n";
        return 0;
    }

    tenebris::platform::PlatformSystem platform({
        .title = "TENEBRIS — Le Protocole 9",
        .width = 1600,
        .height = 900,
        .resizable = true,
        .highPixelDensity = true,
    });

    if (!platform.initialize(smokeTest)) {
        std::cerr << platform.lastError() << '\n';
        return 1;
    }

    tenebris::core::Application application({
        .name = "TENEBRIS — Le Protocole 9",
        .headless = smokeTest,
    });

    if (!application.initialize()) {
        std::cerr << "TENEBRIS failed to initialize.\n";
        return 2;
    }

    while (application.state() != tenebris::core::ApplicationState::Stopped) {
        if (application.state() == tenebris::core::ApplicationState::Running && !platform.pumpEvents()) {
            application.requestShutdown();
        }

        const bool frameExecuted = application.tick();
        if (smokeTest && frameExecuted && application.frameIndex() >= 3U) {
            application.requestShutdown();
        }

        if (!frameExecuted && application.state() != tenebris::core::ApplicationState::Stopped) {
            std::cerr << "TENEBRIS runtime stopped unexpectedly.\n";
            return 3;
        }

        platform.delay(1U);
    }

    platform.shutdown();

    if (smokeTest) {
        std::cout << "TENEBRIS smoke test passed after " << application.frameIndex() << " frames.\n";
    }

    return 0;
}
