#include "PresetLibrary.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace md3 {

PresetLibrary::PresetLibrary(std::filesystem::path storagePath) : storagePath_(std::move(storagePath)) {}

std::string PresetLibrary::keyFor(const std::filesystem::path& path) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    if (error) {
        normalized = std::filesystem::absolute(path, error).lexically_normal();
    }
    return normalized.generic_string();
}

void PresetLibrary::load() {
    entries_.clear();
    if (storagePath_.empty() || !std::filesystem::exists(storagePath_)) {
        return;
    }
    std::ifstream stream(storagePath_);
    if (!stream) {
        throw std::runtime_error("Unable to open preset library: " + storagePath_.string());
    }

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::istringstream row(line);
        std::string path;
        PresetMetadata metadata;
        if (!(row >> std::quoted(path) >> metadata.rating >> metadata.favorite >> metadata.playCount >> metadata.lastPlayed)) {
            throw std::runtime_error("Preset library is malformed at line " + std::to_string(lineNumber) +
                                     ": " + storagePath_.string());
        }
        metadata.rating = std::clamp(metadata.rating, 0, 5);
        entries_[keyFor(path)] = metadata;
    }
}

void PresetLibrary::save() const {
    if (storagePath_.empty()) {
        return;
    }
    if (const auto parent = storagePath_.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    auto temporary = storagePath_;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::trunc);
        if (!stream) {
            throw std::runtime_error("Unable to create preset library: " + temporary.string());
        }
        stream << "# milkdrop3-linux-native library v1\n";
        for (const auto& [path, metadata] : entries_) {
            stream << std::quoted(path) << ' ' << metadata.rating << ' ' << metadata.favorite << ' '
                   << metadata.playCount << ' ' << metadata.lastPlayed << '\n';
        }
        if (!stream) {
            throw std::runtime_error("Unable to write preset library: " + temporary.string());
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, storagePath_, error);
    if (error) {
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        throw std::runtime_error("Unable to replace preset library: " + error.message());
    }
}

PresetMetadata PresetLibrary::metadata(const std::filesystem::path& preset) const {
    const auto iterator = entries_.find(keyFor(preset));
    return iterator == entries_.end() ? PresetMetadata{} : iterator->second;
}

PresetMetadata& PresetLibrary::mutableMetadata(const std::filesystem::path& preset) {
    return entries_[keyFor(preset)];
}

void PresetLibrary::setRating(const std::filesystem::path& preset, const int rating) {
    mutableMetadata(preset).rating = std::clamp(rating, 0, 5);
}

bool PresetLibrary::toggleFavorite(const std::filesystem::path& preset) {
    auto& value = mutableMetadata(preset).favorite;
    value = !value;
    return value;
}

void PresetLibrary::recordPlayed(const std::filesystem::path& preset) {
    auto& metadata = mutableMetadata(preset);
    ++metadata.playCount;
    metadata.lastPlayed = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
}

double PresetLibrary::selectionWeight(const std::filesystem::path& preset) const {
    const auto entry = metadata(preset);
    const double ratingWeight = entry.rating > 0 ? static_cast<double>(entry.rating) : 1.0;
    return ratingWeight * (entry.favorite ? 2.0 : 1.0);
}

const std::filesystem::path& PresetLibrary::storagePath() const noexcept { return storagePath_; }

} // namespace md3
