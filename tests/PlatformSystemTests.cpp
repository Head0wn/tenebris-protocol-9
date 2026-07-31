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

[[nodiscard]] bool inputIsNeutral(const tenebris::platform::PlatformInputState& input) noexcept {
    return input.moveForward == 0.0F
        && input.moveRight == 0.0F
        && input.moveUp == 0.0F
        && input.mouseDeltaX == 0.0F
        && input.mouseDeltaY == 0.0F
        && input.wheelDelta == 0.0F
        && !input.speedBoost
        && !input.resetCameraPressed
        && !input.toggleAutoOrbitPressed
        && !input.mouseCaptured;
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
    success &= expect(inputIsNeutral(platform.input()), "platform input starts neutral");
    success &= expect(platform.initialize(true), "headless platform initializes");
    success &= expect(platform.isInitialized(), "platform reports initialized");
    success &= expect(platform.isHeadless(), "platform reports headless mode");
    success &= expect(platform.window() == nullptr, "headless mode does not create a window");
    success &= expect(platform.pumpEvents(), "headless event pump remains active");
    success &= expect(inputIsNeutral(platform.input()), "headless event pump keeps input neutral");
    success &= expect(!platform.initialize(true), "double initialization is rejected");

    platform.shutdown();
    success &= expect(!platform.isInitialized(), "platform shuts down cleanly");
    success &= expect(!platform.isHeadless(), "headless state resets on shutdown");
    success &= expect(inputIsNeutral(platform.input()), "input state resets on shutdown");

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
