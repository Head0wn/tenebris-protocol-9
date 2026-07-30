#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace tenebris::scene {

struct Vec3f final {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    [[nodiscard]] friend constexpr bool operator==(const Vec3f&, const Vec3f&) noexcept = default;
};

[[nodiscard]] constexpr Vec3f operator+(Vec3f left, Vec3f right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] constexpr Vec3f operator-(Vec3f left, Vec3f right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] constexpr Vec3f operator*(Vec3f value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] constexpr float dot(Vec3f left, Vec3f right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] constexpr Vec3f cross(Vec3f left, Vec3f right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

[[nodiscard]] inline float length(Vec3f value) noexcept {
    return std::sqrt(dot(value, value));
}

[[nodiscard]] inline Vec3f normalize(Vec3f value) noexcept {
    const float magnitude = length(value);
    if (!std::isfinite(magnitude) || magnitude <= 0.000001F) {
        return {};
    }
    return value * (1.0F / magnitude);
}

[[nodiscard]] inline bool isFinite(Vec3f value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct Mat4f final {
    std::array<float, 16U> values{};

    [[nodiscard]] static constexpr Mat4f identity() noexcept {
        Mat4f matrix{};
        matrix.at(0U, 0U) = 1.0F;
        matrix.at(1U, 1U) = 1.0F;
        matrix.at(2U, 2U) = 1.0F;
        matrix.at(3U, 3U) = 1.0F;
        return matrix;
    }

    [[nodiscard]] constexpr float& at(std::size_t row, std::size_t column) noexcept {
        return values[column * 4U + row];
    }

    [[nodiscard]] constexpr float at(std::size_t row, std::size_t column) const noexcept {
        return values[column * 4U + row];
    }
};

[[nodiscard]] inline bool isFinite(const Mat4f& matrix) noexcept {
    for (const float value : matrix.values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr Mat4f multiply(const Mat4f& left, const Mat4f& right) noexcept {
    Mat4f result{};
    for (std::size_t column = 0U; column < 4U; ++column) {
        for (std::size_t row = 0U; row < 4U; ++row) {
            float value = 0.0F;
            for (std::size_t element = 0U; element < 4U; ++element) {
                value += left.at(row, element) * right.at(element, column);
            }
            result.at(row, column) = value;
        }
    }
    return result;
}

[[nodiscard]] inline Mat4f makeLookAt(Vec3f eye, Vec3f target, Vec3f up) noexcept {
    const Vec3f forward = normalize(target - eye);
    const Vec3f right = normalize(cross(forward, up));
    const Vec3f correctedUp = cross(right, forward);

    Mat4f result = Mat4f::identity();
    result.at(0U, 0U) = right.x;
    result.at(1U, 0U) = right.y;
    result.at(2U, 0U) = right.z;
    result.at(0U, 1U) = correctedUp.x;
    result.at(1U, 1U) = correctedUp.y;
    result.at(2U, 1U) = correctedUp.z;
    result.at(0U, 2U) = -forward.x;
    result.at(1U, 2U) = -forward.y;
    result.at(2U, 2U) = -forward.z;
    result.at(3U, 0U) = -dot(right, eye);
    result.at(3U, 1U) = -dot(correctedUp, eye);
    result.at(3U, 2U) = dot(forward, eye);
    return result;
}

[[nodiscard]] inline Mat4f makePerspective(
    float verticalFieldOfViewRadians,
    float aspectRatio,
    float nearPlane,
    float farPlane
) noexcept {
    const float tangent = std::tan(verticalFieldOfViewRadians * 0.5F);
    const float focalLength = 1.0F / tangent;

    Mat4f result{};
    result.at(0U, 0U) = focalLength / aspectRatio;
    result.at(1U, 1U) = -focalLength;
    result.at(2U, 2U) = farPlane / (nearPlane - farPlane);
    result.at(2U, 3U) = -1.0F;
    result.at(3U, 2U) = (nearPlane * farPlane) / (nearPlane - farPlane);
    return result;
}

} // namespace tenebris::scene
