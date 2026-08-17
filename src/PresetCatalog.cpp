#include "PresetCatalog.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <system_error>

namespace md3 {

PresetCatalog::PresetCatalog(const bool shuffle) : shuffle_(shuffle) {}

bool PresetCatalog::isPresetFile(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".milk" || extension == ".prjm";
}

void PresetCatalog::addCandidate(const std::filesystem::path& path) {
    if (!isPresetFile(path)) {
        return;
    }
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    if (error) {
        normalized = std::filesystem::absolute(path, error).lexically_normal();
    }
    presets_.push_back(std::move(normalized));
}

std::size_t PresetCatalog::addPath(const std::filesystem::path& path, const bool recursive) {
    const auto previousSize = presets_.size();
    std::error_code error;

    if (std::filesystem::is_regular_file(path, error)) {
        addCandidate(path);
    } else if (std::filesystem::is_directory(path, error)) {
        const auto options = std::filesystem::directory_options::skip_permission_denied;
        if (recursive) {
            for (std::filesystem::recursive_directory_iterator iterator(path, options, error), end;
                 iterator != end; iterator.increment(error)) {
                if (error) {
                    error.clear();
                    continue;
                }
                if (iterator->is_regular_file(error)) {
                    addCandidate(iterator->path());
                }
            }
        } else {
            for (std::filesystem::directory_iterator iterator(path, options, error), end;
                 iterator != end; iterator.increment(error)) {
                if (error) {
                    error.clear();
                    continue;
                }
                if (iterator->is_regular_file(error)) {
                    addCandidate(iterator->path());
                }
            }
        }
    }

    std::sort(presets_.begin(), presets_.end());
    presets_.erase(std::unique(presets_.begin(), presets_.end()), presets_.end());
    return presets_.size() - previousSize;
}

bool PresetCatalog::empty() const noexcept { return presets_.empty(); }
std::size_t PresetCatalog::size() const noexcept { return presets_.size(); }
bool PresetCatalog::shuffle() const noexcept { return shuffle_; }
void PresetCatalog::setShuffle(const bool enabled) noexcept { shuffle_ = enabled; }

std::filesystem::path PresetCatalog::chooseNext() {
    if (shuffle_) {
        std::uniform_int_distribution<std::size_t> distribution(0, presets_.size() - 1);
        auto chosen = presets_[distribution(random_)];
        if (presets_.size() > 1 && !history_.empty()) {
            for (int attempt = 0; attempt < 8 && chosen == history_.back(); ++attempt) {
                chosen = presets_[distribution(random_)];
            }
        }
        return chosen;
    }

    if (history_.empty()) {
        return presets_.front();
    }
    const auto iterator = std::find(presets_.begin(), presets_.end(), history_[historyPosition_]);
    if (iterator == presets_.end() || std::next(iterator) == presets_.end()) {
        return presets_.front();
    }
    return *std::next(iterator);
}

std::optional<std::filesystem::path> PresetCatalog::selectInitial() {
    if (presets_.empty()) {
        return std::nullopt;
    }
    if (!history_.empty()) {
        return history_[historyPosition_];
    }
    const auto selected = chooseNext();
    history_.push_back(selected);
    historyPosition_ = 0;
    return selected;
}

std::optional<std::filesystem::path> PresetCatalog::next() {
    if (presets_.empty()) {
        return std::nullopt;
    }
    if (historyPosition_ + 1 < history_.size()) {
        ++historyPosition_;
        return history_[historyPosition_];
    }
    const auto selected = chooseNext();
    history_.push_back(selected);
    historyPosition_ = history_.size() - 1;
    if (history_.size() > 1000) {
        history_.erase(history_.begin(), history_.begin() + 100);
        historyPosition_ -= 100;
    }
    return selected;
}

std::optional<std::filesystem::path> PresetCatalog::previous() {
    if (history_.empty()) {
        return selectInitial();
    }
    if (historyPosition_ > 0) {
        --historyPosition_;
    }
    return history_[historyPosition_];
}

std::optional<std::filesystem::path> PresetCatalog::current() const {
    if (history_.empty()) {
        return std::nullopt;
    }
    return history_[historyPosition_];
}

} // namespace md3
