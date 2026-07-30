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

    if (headless_) {
        initialized_ = true;
        return true;
    }

    if (!SDL_SetAppMetadata(config_.title.c_str(), "0.3.0", "games.vx.tenebris.protocol9")) {
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
    }

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

    initialized_ = false;
    headless_ = false;
    framebufferResized_ = false;
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

const std::string& PlatformSystem::lastError() const noexcept {
    return lastError_;
}

} // namespace tenebris::platform
