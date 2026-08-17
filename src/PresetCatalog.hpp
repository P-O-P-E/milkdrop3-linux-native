#pragma once

#include <filesystem>
#include <functional>
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
    void setWeightProvider(std::function<double(const std::filesystem::path&)> provider);

    std::optional<std::filesystem::path> selectInitial();
    std::optional<std::filesystem::path> next();
    std::optional<std::filesystem::path> previous();
    std::optional<std::filesystem::path> select(const std::filesystem::path& path);
    bool remove(const std::filesystem::path& path);
    [[nodiscard]] std::optional<std::filesystem::path> current() const;
    [[nodiscard]] const std::vector<std::filesystem::path>& presets() const noexcept;

    std::size_t importPlaylist(const std::filesystem::path& path);
    void exportPlaylist(const std::filesystem::path& path) const;

    static bool isPresetFile(const std::filesystem::path& path);

private:
    std::filesystem::path chooseNext();
    void addCandidate(const std::filesystem::path& path);

    std::vector<std::filesystem::path> presets_;
    std::vector<std::filesystem::path> history_;
    std::size_t historyPosition_{0};
    bool shuffle_{true};
    std::function<double(const std::filesystem::path&)> weightProvider_;
    std::mt19937 random_{std::random_device{}()};
};

} // namespace md3
