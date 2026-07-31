#include <tenebris/scene/InspectionCamera.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace tenebris::scene {

namespace {

constexpr float minimumPositiveValue = 0.0001F;
constexpr float maximumDeltaSeconds = 0.1F;
constexpr Vec3f worldUp{0.0F, 1.0F, 0.0F};

[[nodiscard]] bool finiteInput(const InspectionCameraInput& input) noexcept {
    return std::isfinite(input.moveForward)
        && std::isfinite(input.moveRight)
        && std::isfinite(input.moveUp)
        && std::isfinite(input.yawDelta)
        && std::isfinite(input.pitchDelta)
        && std::isfinite(input.zoomDelta);
}

[[nodiscard]] bool hasManualInput(const InspectionCameraInput& input) noexcept {
    return std::fabs(input.moveForward) > minimumPositiveValue
        || std::fabs(input.moveRight) > minimumPositiveValue
        || std::fabs(input.moveUp) > minimumPositiveValue
        || std::fabs(input.yawDelta) > minimumPositiveValue
        || std::fabs(input.pitchDelta) > minimumPositiveValue
        || std::fabs(input.zoomDelta) > minimumPositiveValue;
}

[[nodiscard]] float safePositive(float value, float fallback) noexcept {
    return std::isfinite(value) && value > minimumPositiveValue ? value : fallback;
}

} // namespace

InspectionCameraController::InspectionCameraController(InspectionCameraConfig config)
    : config_(std::move(config)), camera_(config_.initialCamera) {
    sanitizeConfig();

    if (!camera_.lastError().empty()) {
        config_.initialCamera = CameraConfig{};
        camera_ = SceneCamera{};
    } else {
        config_.initialCamera = camera_.config();
    }

    reset();
}

InspectionCameraUpdate InspectionCameraController::update(
    const InspectionCameraInput& input,
    float deltaSeconds
) noexcept {
    lastError_.clear();
    if (!finiteInput(input) || !std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
        lastError_ = "Inspection camera rejected non-finite input or a negative frame delta.";
        return InspectionCameraUpdate::Rejected;
    }

    if (input.reset) {
        reset();
        return InspectionCameraUpdate::Changed;
    }

    if (input.toggleAutoOrbit) {
        if (!setAutoOrbit(!autoOrbitEnabled_)) {
            return InspectionCameraUpdate::Rejected;
        }
    }

    const float clampedDeltaSeconds = std::min(deltaSeconds, maximumDeltaSeconds);
    const bool manualInput = hasManualInput(input);
    if (manualInput && autoOrbitEnabled_) {
        autoOrbitEnabled_ = false;
    }

    if (autoOrbitEnabled_) {
        yawRadians_ += config_.autoOrbitRadiansPerSecond * clampedDeltaSeconds;
        const Vec3f forward = forwardDirection();
        const Vec3f position = orbitPivot_ - forward * orbitRadius_;
        if (!applyCamera(position, orbitPivot_)) {
            return InspectionCameraUpdate::Rejected;
        }
        return InspectionCameraUpdate::Changed;
    }

    if (!manualInput) {
        return input.toggleAutoOrbit ? InspectionCameraUpdate::Changed
                                     : InspectionCameraUpdate::Unchanged;
    }

    yawRadians_ += input.yawDelta * config_.lookSensitivity;
    pitchRadians_ += input.pitchDelta * config_.lookSensitivity;
    pitchRadians_ = std::clamp(
        pitchRadians_,
        -config_.maximumPitchRadians,
        config_.maximumPitchRadians
    );

    const Vec3f forward = forwardDirection();
    Vec3f right = normalize(cross(forward, worldUp));
    if (length(right) <= minimumPositiveValue) {
        right = {1.0F, 0.0F, 0.0F};
    }

    Vec3f movement = forward * input.moveForward
        + right * input.moveRight
        + worldUp * input.moveUp;
    const float movementLength = length(movement);
    if (movementLength > 1.0F) {
        movement = movement * (1.0F / movementLength);
    }

    const float speed = config_.moveSpeed
        * (input.speedBoost ? config_.speedBoostMultiplier : 1.0F);
    Vec3f position = camera_.config().position;
    position = position + movement * (speed * clampedDeltaSeconds);
    position = position + forward * (input.zoomDelta * config_.zoomSpeed);

    if (!applyCamera(position, position + forward)) {
        return InspectionCameraUpdate::Rejected;
    }
    return InspectionCameraUpdate::Changed;
}

void InspectionCameraController::reset() noexcept {
    lastError_.clear();
    autoOrbitEnabled_ = false;
    if (!camera_.setConfig(config_.initialCamera)) {
        camera_ = SceneCamera{};
        config_.initialCamera = camera_.config();
    }
    deriveAnglesFromCamera();
    orbitPivot_ = camera_.config().target;
    orbitRadius_ = std::max(
        length(camera_.config().position - orbitPivot_),
        minimumPositiveValue
    );
}

bool InspectionCameraController::setAutoOrbit(bool enabled) noexcept {
    lastError_.clear();
    if (enabled) {
        orbitPivot_ = camera_.config().target;
        orbitRadius_ = std::max(
            length(camera_.config().position - orbitPivot_),
            minimumPositiveValue
        );
        deriveAnglesFromCamera();
    }
    autoOrbitEnabled_ = enabled;
    return true;
}

const SceneCamera& InspectionCameraController::camera() const noexcept {
    return camera_;
}

bool InspectionCameraController::autoOrbitEnabled() const noexcept {
    return autoOrbitEnabled_;
}

const std::string& InspectionCameraController::lastError() const noexcept {
    return lastError_;
}

bool InspectionCameraController::applyCamera(Vec3f position, Vec3f target) noexcept {
    CameraConfig next = camera_.config();
    next.position = position;
    next.target = target;
    if (!camera_.setConfig(next)) {
        lastError_ = camera_.lastError();
        return false;
    }
    return true;
}

Vec3f InspectionCameraController::forwardDirection() const noexcept {
    const float horizontal = std::cos(pitchRadians_);
    return normalize({
        horizontal * std::sin(yawRadians_),
        std::sin(pitchRadians_),
        -horizontal * std::cos(yawRadians_),
    });
}

void InspectionCameraController::deriveAnglesFromCamera() noexcept {
    const Vec3f direction = normalize(camera_.config().target - camera_.config().position);
    pitchRadians_ = std::asin(std::clamp(direction.y, -1.0F, 1.0F));
    yawRadians_ = std::atan2(direction.x, -direction.z);
}

void InspectionCameraController::sanitizeConfig() noexcept {
    config_.moveSpeed = safePositive(config_.moveSpeed, 8.0F);
    config_.speedBoostMultiplier = safePositive(config_.speedBoostMultiplier, 4.0F);
    config_.zoomSpeed = safePositive(config_.zoomSpeed, 3.0F);
    config_.lookSensitivity = safePositive(config_.lookSensitivity, 0.0025F);
    config_.autoOrbitRadiansPerSecond = safePositive(
        config_.autoOrbitRadiansPerSecond,
        0.25F
    );
    config_.maximumPitchRadians = std::clamp(
        safePositive(config_.maximumPitchRadians, 1.45F),
        0.1F,
        1.55F
    );
}

} // namespace tenebris::scene
