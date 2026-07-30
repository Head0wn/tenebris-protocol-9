#include <tenebris/core/Application.hpp>

#include <utility>

namespace tenebris::core {

Application::Application(ApplicationConfig config)
    : config_(std::move(config)) {
}

bool Application::initialize() noexcept {
    if (state_ != ApplicationState::Created) {
        state_ = ApplicationState::Failed;
        return false;
    }

    frameIndex_ = 0;
    state_ = ApplicationState::Running;
    return true;
}

bool Application::tick() noexcept {
    if (state_ == ApplicationState::Stopping) {
        shutdown();
        return false;
    }

    if (state_ != ApplicationState::Running) {
        return false;
    }

    ++frameIndex_;
    return true;
}

void Application::requestShutdown() noexcept {
    if (state_ == ApplicationState::Running) {
        state_ = ApplicationState::Stopping;
    }
}

void Application::shutdown() noexcept {
    if (state_ == ApplicationState::Stopped) {
        return;
    }

    if (state_ == ApplicationState::Failed) {
        return;
    }

    state_ = ApplicationState::Stopped;
}

ApplicationState Application::state() const noexcept {
    return state_;
}

std::uint64_t Application::frameIndex() const noexcept {
    return frameIndex_;
}

const ApplicationConfig& Application::config() const noexcept {
    return config_;
}

} // namespace tenebris::core
