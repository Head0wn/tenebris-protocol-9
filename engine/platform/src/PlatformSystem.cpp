#include <tenebris/platform/PlatformSystem.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <utility>

namespace tenebris::platform {

namespace {

[[nodiscard]] std::string makeSdlError(const char* context) {
    std::string message{context};
    message += ": ";
    message += SDL_GetError();
    return message;
}

[[nodiscard]] float axisValue(bool positive, bool negative) noexcept {
    return (positive ? 1.0F : 0.0F) - (negative ? 1.0F : 0.0F);
}

} // namespace

PlatformSystem::PlatformSystem(PlatformConfig config)
    : config_(std::move(config)) {
}

PlatformSystem::~PlatformSystem() {
    shutdown();
}

bool PlatformSystem::initialize(bool headless) {
    if (initialized_) {
        lastError_ = "PlatformSystem is already initialized.";
        return false;
    }

    lastError_.clear();
    headless_ = headless;
    framebufferResized_ = false;
    mouseCaptured_ = false;
    input_ = {};

    if (headless_) {
        initialized_ = true;
        return true;
    }

    if (!SDL_SetAppMetadata(config_.title.c_str(), "0.7.0", "games.vx.tenebris.protocol9")) {
        lastError_ = makeSdlError("SDL_SetAppMetadata failed");
        return false;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        lastError_ = makeSdlError("SDL_Init failed");
        return false;
    }
    sdlInitialized_ = true;

    if (config_.vulkan) {
        if (!SDL_Vulkan_LoadLibrary(nullptr)) {
            lastError_ = makeSdlError("SDL_Vulkan_LoadLibrary failed");
            shutdown();
            return false;
        }
        vulkanLoaded_ = true;
    }

    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;
    if (config_.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (config_.highPixelDensity) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    if (config_.vulkan) {
        flags |= SDL_WINDOW_VULKAN;
    }

    window_ = SDL_CreateWindow(config_.title.c_str(), config_.width, config_.height, flags);
    if (window_ == nullptr) {
        lastError_ = makeSdlError("SDL_CreateWindow failed");
        shutdown();
        return false;
    }

    if (!SDL_ShowWindow(window_)) {
        lastError_ = makeSdlError("SDL_ShowWindow failed");
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

bool PlatformSystem::pumpEvents() noexcept {
    if (!initialized_) {
        return false;
    }

    clearTransientInput();
    if (headless_) {
        return true;
    }

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            return false;
        }
        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            framebufferResized_ = true;
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            releaseMouseCapture();
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT) {
            if (SDL_SetWindowRelativeMouseMode(window_, true)) {
                mouseCaptured_ = true;
            }
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT) {
            releaseMouseCapture();
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION && mouseCaptured_) {
            input_.mouseDeltaX += event.motion.xrel;
            input_.mouseDeltaY += event.motion.yrel;
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            input_.wheelDelta += event.wheel.y;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            if (event.key.scancode == SDL_SCANCODE_R) {
                input_.resetCameraPressed = true;
            } else if (event.key.scancode == SDL_SCANCODE_O) {
                input_.toggleAutoOrbitPressed = true;
            } else if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                if (mouseCaptured_) {
                    releaseMouseCapture();
                } else {
                    return false;
                }
            }
        }
    }

    const bool* keyboard = SDL_GetKeyboardState(nullptr);
    if (keyboard != nullptr) {
        const bool forward = keyboard[SDL_SCANCODE_W] || keyboard[SDL_SCANCODE_UP];
        const bool backward = keyboard[SDL_SCANCODE_S] || keyboard[SDL_SCANCODE_DOWN];
        const bool right = keyboard[SDL_SCANCODE_D] || keyboard[SDL_SCANCODE_RIGHT];
        const bool left = keyboard[SDL_SCANCODE_A] || keyboard[SDL_SCANCODE_LEFT];
        const bool up = keyboard[SDL_SCANCODE_E] || keyboard[SDL_SCANCODE_SPACE];
        const bool down = keyboard[SDL_SCANCODE_Q] || keyboard[SDL_SCANCODE_LCTRL];

        input_.moveForward = axisValue(forward, backward);
        input_.moveRight = axisValue(right, left);
        input_.moveUp = axisValue(up, down);
        input_.speedBoost = keyboard[SDL_SCANCODE_LSHIFT] || keyboard[SDL_SCANCODE_RSHIFT];
    }
    input_.mouseCaptured = mouseCaptured_;
    return true;
}

bool PlatformSystem::consumeFramebufferResize() noexcept {
    const bool resized = framebufferResized_;
    framebufferResized_ = false;
    return resized;
}

void PlatformSystem::delay(std::uint32_t milliseconds) const noexcept {
    if (initialized_ && !headless_) {
        SDL_Delay(milliseconds);
    }
}

void PlatformSystem::shutdown() noexcept {
    releaseMouseCapture();

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    if (vulkanLoaded_) {
        SDL_Vulkan_UnloadLibrary();
        vulkanLoaded_ = false;
    }

    if (sdlInitialized_) {
        SDL_Quit();
        sdlInitialized_ = false;
    }

    input_ = {};
    initialized_ = false;
    headless_ = false;
    framebufferResized_ = false;
    mouseCaptured_ = false;
}

bool PlatformSystem::isInitialized() const noexcept {
    return initialized_;
}

bool PlatformSystem::isHeadless() const noexcept {
    return headless_;
}

bool PlatformSystem::isVulkanLoaded() const noexcept {
    return vulkanLoaded_;
}

SDL_Window* PlatformSystem::window() const noexcept {
    return window_;
}

const PlatformInputState& PlatformSystem::input() const noexcept {
    return input_;
}

const std::string& PlatformSystem::lastError() const noexcept {
    return lastError_;
}

void PlatformSystem::clearTransientInput() noexcept {
    input_.mouseDeltaX = 0.0F;
    input_.mouseDeltaY = 0.0F;
    input_.wheelDelta = 0.0F;
    input_.resetCameraPressed = false;
    input_.toggleAutoOrbitPressed = false;
}

void PlatformSystem::releaseMouseCapture() noexcept {
    if (window_ != nullptr && mouseCaptured_) {
        static_cast<void>(SDL_SetWindowRelativeMouseMode(window_, false));
    }
    mouseCaptured_ = false;
    input_.mouseCaptured = false;
}

} // namespace tenebris::platform
