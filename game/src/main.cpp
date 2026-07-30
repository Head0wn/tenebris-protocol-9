#include <tenebris/core/Application.hpp>

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
        std::cout << "TENEBRIS 0.1.0\n";
        return 0;
    }

    tenebris::core::Application application({
        .name = "TENEBRIS — Le Protocole 9",
        .headless = smokeTest,
    });

    if (!application.initialize()) {
        std::cerr << "TENEBRIS failed to initialize.\n";
        return 1;
    }

    const std::uint64_t frameBudget = smokeTest ? 3U : 1U;
    for (std::uint64_t frame = 0; frame < frameBudget; ++frame) {
        if (!application.tick()) {
            std::cerr << "TENEBRIS runtime stopped unexpectedly.\n";
            return 2;
        }
    }

    application.requestShutdown();
    static_cast<void>(application.tick());

    if (application.state() != tenebris::core::ApplicationState::Stopped) {
        std::cerr << "TENEBRIS did not shut down cleanly.\n";
        return 3;
    }

    if (smokeTest) {
        std::cout << "TENEBRIS smoke test passed after " << application.frameIndex() << " frames.\n";
    } else {
        std::cout << "TENEBRIS engine foundation initialized successfully.\n";
    }

    return 0;
}
