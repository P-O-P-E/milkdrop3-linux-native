#include "GeneratedPresetStore.hpp"

#include "MashupEngine.hpp"

#include <stdexcept>
#include <system_error>
#include <utility>

namespace md3 {

GeneratedPresetStore::GeneratedPresetStore(std::filesystem::path directory)
    : directory_(normalized(std::move(directory))) {}

std::filesystem::path GeneratedPresetStore::normalized(const std::filesystem::path& path) {
    std::error_code error;
    auto result = std::filesystem::weakly_canonical(path, error);
    if (error) {
        result = std::filesystem::absolute(path, error).lexically_normal();
    }
    return result;
}

bool GeneratedPresetStore::owns(const std::filesystem::path& path) const {
    const auto candidate = normalized(path);
    const auto relative = candidate.lexically_relative(directory_);
    if (relative.empty() || relative.is_absolute()) {
        return candidate == directory_;
    }
    const auto first = *relative.begin();
    return first != ".." && first != ".trash";
}

std::filesystem::path GeneratedPresetStore::uniquePath(const std::filesystem::path& directory,
                                                       const std::string& stem,
                                                       const std::string& extension) {
    auto candidate = directory / (stem + extension);
    for (int number = 2; std::filesystem::exists(candidate); ++number) {
        candidate = directory / (stem + '-' + std::to_string(number) + extension);
    }
    return candidate;
}

std::filesystem::path GeneratedPresetStore::renamePreset(const std::filesystem::path& path,
                                                         const std::string& requestedName) const {
    if (!owns(path)) {
        throw std::runtime_error("Only generated presets can be renamed from the application");
    }
    const auto source = normalized(path);
    if (!std::filesystem::is_regular_file(source)) {
        throw std::runtime_error("Preset does not exist: " + source.string());
    }
    const auto stem = MashupEngine::safeName(requestedName);
    auto destination = directory_ / (stem + source.extension().string());
    if (normalized(destination) == source) {
        return source;
    }
    if (std::filesystem::exists(destination)) {
        destination = uniquePath(directory_, stem, source.extension().string());
    }
    std::filesystem::rename(source, destination);
    return destination;
}

std::filesystem::path GeneratedPresetStore::moveToTrash(const std::filesystem::path& path) const {
    if (!owns(path)) {
        throw std::runtime_error("Only generated presets can be moved to the application trash");
    }
    const auto source = normalized(path);
    if (!std::filesystem::is_regular_file(source)) {
        throw std::runtime_error("Preset does not exist: " + source.string());
    }
    const auto trash = directory_ / ".trash";
    std::filesystem::create_directories(trash);
    const auto destination = uniquePath(trash, MashupEngine::safeName(source.stem().string()),
                                        source.extension().string());
    std::filesystem::rename(source, destination);
    return destination;
}

const std::filesystem::path& GeneratedPresetStore::directory() const noexcept { return directory_; }

} // namespace md3
