#include <tenebris/core/Application.hpp>

#include <cstdlib>
#include <iostream>

namespace {

[[nodiscard]] bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }

    return true;
}

} // namespace

int main() {
    using tenebris::core::Application;
    using tenebris::core::ApplicationState;

    Application application({.name = "TENEBRIS test", .headless = true});

    bool success = true;
    success &= expect(application.state() == ApplicationState::Created, "application starts in Created state");
    success &= expect(application.initialize(), "application initializes once");
    success &= expect(application.state() == ApplicationState::Running, "application enters Running state");
    success &= expect(application.tick(), "first frame executes");
    success &= expect(application.tick(), "second frame executes");
    success &= expect(application.frameIndex() == 2U, "frame counter is deterministic");

    application.requestShutdown();
    success &= expect(application.state() == ApplicationState::Stopping, "shutdown request enters Stopping state");
    success &= expect(!application.tick(), "stopping frame returns false");
    success &= expect(application.state() == ApplicationState::Stopped, "application reaches Stopped state");
    success &= expect(!application.tick(), "stopped application cannot tick");

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
