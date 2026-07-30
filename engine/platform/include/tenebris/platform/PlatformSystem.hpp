#pragma once

#include <cstdint>
#include <string>

struct SDL_Window;

namespace tenebris::platform {

struct PlatformConfig final {
    std::string title{"TENEBRIS — Le Protocole 9"};
    std::int32_t width{1600};
    std::int32_t height{900};
    bool resizable{true};
    bool highPixelDensity{true};
};

class PlatformSystem final {
public:
    explicit PlatformSystem(PlatformConfig config);
    ~PlatformSystem();

    PlatformSystem(const PlatformSystem&) = delete;
    PlatformSystem& operator=(const PlatformSystem&) = delete;
    PlatformSystem(PlatformSystem&&) = delete;
    PlatformSystem& operator=(PlatformSystem&&) = delete;

    [[nodiscard]] bool initialize(bool headless);
    [[nodiscard]] bool pumpEvents() noexcept;
    void delay(std::uint32_t milliseconds) const noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool isHeadless() const noexcept;
    [[nodiscard]] SDL_Window* window() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;

private:
    PlatformConfig config_;
    SDL_Window* window_{nullptr};
    std::string lastError_;
    bool initialized_{false};
    bool headless_{false};
    bool sdlInitialized_{false};
};

} // namespace tenebris::platform
