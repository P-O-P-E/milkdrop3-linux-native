#include "OverlayManager.hpp"

#include <algorithm>
#include <utility>

namespace md3 {

void OverlayManager::push(std::string text, const OverlaySeverity severity,
                          const std::chrono::milliseconds duration) {
    messages_.push_back({std::move(text), severity, std::chrono::steady_clock::now() + duration});
    if (messages_.size() > 8) {
        messages_.erase(messages_.begin(), messages_.begin() + static_cast<std::ptrdiff_t>(messages_.size() - 8));
    }
}

std::vector<OverlayMessage> OverlayManager::active() {
    const auto now = std::chrono::steady_clock::now();
    std::erase_if(messages_, [now](const OverlayMessage& message) { return message.expiresAt <= now; });
    return messages_;
}

void OverlayManager::clear() noexcept { messages_.clear(); }

} // namespace md3
