#include <tenebris/platform/PlatformSystem.hpp>

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
    tenebris::platform::PlatformSystem platform({
        .title = "TENEBRIS platform test",
        .width = 640,
        .height = 360,
        .resizable = false,
        .highPixelDensity = false,
    });

    bool success = true;
    success &= expect(!platform.isInitialized(), "platform starts uninitialized");
    success &= expect(platform.initialize(true), "headless platform initializes");
    success &= expect(platform.isInitialized(), "platform reports initialized");
    success &= expect(platform.isHeadless(), "platform reports headless mode");
    success &= expect(platform.window() == nullptr, "headless mode does not create a window");
    success &= expect(platform.pumpEvents(), "headless event pump remains active");
    success &= expect(!platform.initialize(true), "double initialization is rejected");

    platform.shutdown();
    success &= expect(!platform.isInitialized(), "platform shuts down cleanly");
    success &= expect(!platform.isHeadless(), "headless state resets on shutdown");

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
