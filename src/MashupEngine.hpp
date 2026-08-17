#pragma once

#include "PresetDocument.hpp"

#include <array>
#include <filesystem>
#include <string>

namespace md3 {

struct MashupSelection {
    bool general{false};
    bool motion{false};
    bool waves{false};
    bool shapes{false};
    bool warpShader{true};
    bool compositeShader{false};

    [[nodiscard]] bool any() const noexcept;
};

class MashupEngine {
public:
    static PresetDocument combine(const PresetDocument& base, const PresetDocument& donor,
                                  const MashupSelection& selection);
    static std::filesystem::path uniqueOutputPath(const std::filesystem::path& directory,
                                                  const std::string& baseName,
                                                  const std::string& suffix);
    static std::string safeName(std::string value);
};

} // namespace md3
