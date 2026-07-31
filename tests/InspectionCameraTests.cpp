#include <tenebris/scene/InspectionCamera.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

[[nodiscard]] bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

[[nodiscard]] bool nearlyEqual(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::fabs(left - right) <= tolerance;
}

[[nodiscard]] bool nearlyEqual(
    tenebris::scene::Vec3f left,
    tenebris::scene::Vec3f right,
    float tolerance = 0.0001F
) noexcept {
    return nearlyEqual(left.x, right.x, tolerance)
        && nearlyEqual(left.y, right.y, tolerance)
        && nearlyEqual(left.z, right.z, tolerance);
}

} // namespace

int main() {
    using namespace tenebris::scene;

    InspectionCameraController controller;
    const CameraConfig initial = controller.camera().config();
    bool success = true;

    success &= expect(controller.lastError().empty(), "default inspection camera must be valid");
    success &= expect(
        isFinite(controller.camera().viewProjectionMatrix()),
        "default inspection camera matrix must be finite"
    );

    InspectionCameraInput forwardInput{};
    forwardInput.moveForward = 1.0F;
    success &= expect(
        controller.update(forwardInput, 0.5F) == InspectionCameraUpdate::Changed,
        "forward input must change the camera"
    );
    success &= expect(
        !nearlyEqual(controller.camera().config().position, initial.position),
        "forward input must move the camera position"
    );

    InspectionCameraInput resetInput{};
    resetInput.reset = true;
    success &= expect(
        controller.update(resetInput, 0.0F) == InspectionCameraUpdate::Changed,
        "reset input must report a camera change"
    );
    success &= expect(
        nearlyEqual(controller.camera().config().position, initial.position),
        "reset must restore the initial position"
    );
    success &= expect(
        nearlyEqual(controller.camera().config().target, initial.target),
        "reset must restore the initial target"
    );

    success &= expect(controller.setAutoOrbit(true), "auto orbit must enable cleanly");
    const Vec3f beforeOrbit = controller.camera().config().position;
    success &= expect(
        controller.update({}, 1.0F / 60.0F) == InspectionCameraUpdate::Changed,
        "auto orbit must update the camera every frame"
    );
    success &= expect(
        !nearlyEqual(controller.camera().config().position, beforeOrbit),
        "auto orbit must move the camera around its pivot"
    );
    success &= expect(controller.autoOrbitEnabled(), "auto orbit state must remain enabled");

    InspectionCameraInput manualInput{};
    manualInput.moveRight = 1.0F;
    success &= expect(
        controller.update(manualInput, 1.0F / 60.0F) == InspectionCameraUpdate::Changed,
        "manual input must remain usable after auto orbit"
    );
    success &= expect(
        !controller.autoOrbitEnabled(),
        "manual movement must disable cinematic auto orbit"
    );

    InspectionCameraInput extremeLook{};
    extremeLook.pitchDelta = 100000.0F;
    success &= expect(
        controller.update(extremeLook, 1.0F / 60.0F) == InspectionCameraUpdate::Changed,
        "extreme look input must be clamped rather than rejected"
    );
    success &= expect(
        isFinite(controller.camera().viewProjectionMatrix()),
        "pitch clamping must preserve a finite camera matrix"
    );

    InspectionCameraInput invalid{};
    invalid.yawDelta = std::numeric_limits<float>::quiet_NaN();
    success &= expect(
        controller.update(invalid, 1.0F / 60.0F) == InspectionCameraUpdate::Rejected,
        "non-finite input must be rejected"
    );
    success &= expect(!controller.lastError().empty(), "rejected input must expose an error");

    if (success) {
        std::cout << "Inspection camera tests passed.\n";
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
