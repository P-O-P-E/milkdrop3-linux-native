#pragma once

#include <filesystem>
#include <optional>
#include <random>
#include <vector>

namespace md3 {

class PresetCatalog {
public:
    explicit PresetCatalog(bool shuffle = true);

    std::size_t addPath(const std::filesystem::path& path, bool recursive = true);
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool shuffle() const noexcept;
    void setShuffle(bool enabled) noexcept;

    std::optional<std::filesystem::path> selectInitial();
    std::optional<std::filesystem::path> next();
    std::optional<std::filesystem::path> previous();
    [[nodiscard]] std::optional<std::filesystem::path> current() const;

    static bool isPresetFile(const std::filesystem::path& path);

private:
    std::filesystem::path chooseNext();
    void addCandidate(const std::filesystem::path& path);

    std::vector<std::filesystem::path> presets_;
    std::vector<std::filesystem::path> history_;
    std::size_t historyPosition_{0};
    bool shuffle_{true};
    std::mt19937 random_{std::random_device{}()};
};

} // namespace md3

