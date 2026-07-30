#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct SDL_Window;

namespace tenebris::renderer {

struct RendererConfig final {
    std::string applicationName{"TENEBRIS — Le Protocole 9"};
    std::uint32_t framesInFlight{2U};
    bool requestValidation{false};
};

enum class RendererState : std::uint8_t {
    Stopped,
    Headless,
    Ready,
    Suspended,
    Failed,
};

struct RendererStats final {
    std::uint64_t submittedFrames{0U};
    std::uint64_t swapchainRebuilds{0U};
};

class RendererSystem final {
public:
    explicit RendererSystem(RendererConfig config);
    ~RendererSystem();

    RendererSystem(const RendererSystem&) = delete;
    RendererSystem& operator=(const RendererSystem&) = delete;
    RendererSystem(RendererSystem&&) = delete;
    RendererSystem& operator=(RendererSystem&&) = delete;

    [[nodiscard]] bool initialize(SDL_Window* window, bool headless);
    [[nodiscard]] bool drawFrame();
    void notifyFramebufferResized() noexcept;
    void waitIdle() noexcept;
    void shutdown() noexcept;

    [[nodiscard]] RendererState state() const noexcept;
    [[nodiscard]] bool isValidationEnabled() const noexcept;
    [[nodiscard]] const RendererStats& stats() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tenebris::renderer
