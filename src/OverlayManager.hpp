#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace md3 {

enum class OverlaySeverity { Information, Success, Warning, Error };

struct OverlayMessage {
    std::string text;
    OverlaySeverity severity{OverlaySeverity::Information};
    std::chrono::steady_clock::time_point expiresAt{};
};

class OverlayManager {
public:
    void push(std::string text, OverlaySeverity severity = OverlaySeverity::Information,
              std::chrono::milliseconds duration = std::chrono::milliseconds(3500));
    [[nodiscard]] std::vector<OverlayMessage> active();
    void clear() noexcept;

private:
    std::vector<OverlayMessage> messages_;
};

} // namespace md3
