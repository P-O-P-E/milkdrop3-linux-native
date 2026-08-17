#pragma once

#include <filesystem>
#include <string>

namespace md3 {

class GeneratedPresetStore {
public:
    explicit GeneratedPresetStore(std::filesystem::path directory);

    [[nodiscard]] bool owns(const std::filesystem::path& path) const;
    [[nodiscard]] std::filesystem::path renamePreset(const std::filesystem::path& path,
                                                     const std::string& requestedName) const;
    [[nodiscard]] std::filesystem::path moveToTrash(const std::filesystem::path& path) const;
    [[nodiscard]] const std::filesystem::path& directory() const noexcept;

private:
    static std::filesystem::path normalized(const std::filesystem::path& path);
    static std::filesystem::path uniquePath(const std::filesystem::path& directory,
                                            const std::string& stem,
                                            const std::string& extension);

    std::filesystem::path directory_;
};

} // namespace md3
