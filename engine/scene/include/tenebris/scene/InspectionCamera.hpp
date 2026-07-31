#pragma once

#include <tenebris/scene/RenderScene.hpp>

#include <cstdint>
#include <string>

namespace tenebris::scene {

struct InspectionCameraInput final {
    float moveForward{0.0F};
    float moveRight{0.0F};
    float moveUp{0.0F};
    float yawDelta{0.0F};
    float pitchDelta{0.0F};
    float zoomDelta{0.0F};
    bool speedBoost{false};
    bool reset{false};
    bool toggleAutoOrbit{false};
};

struct InspectionCameraConfig final {
    CameraConfig initialCamera{};
    float moveSpeed{8.0F};
    float speedBoostMultiplier{4.0F};
    float zoomSpeed{3.0F};
    float lookSensitivity{0.0025F};
    float autoOrbitRadiansPerSecond{0.25F};
    float maximumPitchRadians{1.45F};
};

enum class InspectionCameraUpdate : std::uint8_t {
    Unchanged,
    Changed,
    Rejected,
};

class InspectionCameraController final {
public:
    explicit InspectionCameraController(InspectionCameraConfig config = {});

    [[nodiscard]] InspectionCameraUpdate update(
        const InspectionCameraInput& input,
        float deltaSeconds
    ) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool setAutoOrbit(bool enabled) noexcept;

    [[nodiscard]] const SceneCamera& camera() const noexcept;
    [[nodiscard]] bool autoOrbitEnabled() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;

private:
    [[nodiscard]] bool applyCamera(Vec3f position, Vec3f target) noexcept;
    [[nodiscard]] Vec3f forwardDirection() const noexcept;
    void deriveAnglesFromCamera() noexcept;
    void sanitizeConfig() noexcept;

    InspectionCameraConfig config_{};
    SceneCamera camera_{};
    Vec3f orbitPivot_{};
    float orbitRadius_{1.0F};
    float yawRadians_{0.0F};
    float pitchRadians_{0.0F};
    bool autoOrbitEnabled_{false};
    std::string lastError_;
};

} // namespace tenebris::scene
