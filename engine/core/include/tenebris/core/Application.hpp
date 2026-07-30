#pragma once

#include <cstdint>
#include <string>

namespace tenebris::core {

enum class ApplicationState : std::uint8_t {
    Created,
    Running,
    Stopping,
    Stopped,
    Failed
};

struct ApplicationConfig final {
    std::string name{"TENEBRIS — Le Protocole 9"};
    bool headless{false};
};

class Application final {
public:
    explicit Application(ApplicationConfig config);

    [[nodiscard]] bool initialize() noexcept;
    [[nodiscard]] bool tick() noexcept;
    void requestShutdown() noexcept;
    void shutdown() noexcept;

    [[nodiscard]] ApplicationState state() const noexcept;
    [[nodiscard]] std::uint64_t frameIndex() const noexcept;
    [[nodiscard]] const ApplicationConfig& config() const noexcept;

private:
    ApplicationConfig config_;
    ApplicationState state_{ApplicationState::Created};
    std::uint64_t frameIndex_{0};
};

} // namespace tenebris::core
