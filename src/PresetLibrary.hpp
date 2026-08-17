#pragma once

#include <cstdint>
#include <filesystem>
#include <map>

namespace md3 {

struct PresetMetadata {
    int rating{0};
    bool favorite{false};
    std::uint64_t playCount{0};
    std::int64_t lastPlayed{0};
};

class PresetLibrary {
public:
    explicit PresetLibrary(std::filesystem::path storagePath = {});

    void load();
    void save() const;

    [[nodiscard]] PresetMetadata metadata(const std::filesystem::path& preset) const;
    void setRating(const std::filesystem::path& preset, int rating);
    bool toggleFavorite(const std::filesystem::path& preset);
    void recordPlayed(const std::filesystem::path& preset);
    [[nodiscard]] double selectionWeight(const std::filesystem::path& preset) const;

    [[nodiscard]] const std::filesystem::path& storagePath() const noexcept;

private:
    static std::string keyFor(const std::filesystem::path& path);
    PresetMetadata& mutableMetadata(const std::filesystem::path& preset);

    std::filesystem::path storagePath_;
    std::map<std::string, PresetMetadata> entries_;
};

} // namespace md3
