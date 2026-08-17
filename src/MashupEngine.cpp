#include "MashupEngine.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace md3 {

bool MashupSelection::any() const noexcept {
    return general || motion || waves || shapes || warpShader || compositeShader;
}

PresetDocument MashupEngine::combine(const PresetDocument& base, const PresetDocument& donor,
                                     const MashupSelection& selection) {
    if (!selection.any()) {
        throw std::runtime_error("Select at least one mashup component");
    }
    if (!base.valid()) {
        throw std::runtime_error("Base preset contains syntax errors");
    }
    if (!donor.valid()) {
        throw std::runtime_error("Donor preset contains syntax errors");
    }

    auto result = base;
    const std::array<std::pair<bool, PresetPart>, 6> parts{{
        {selection.general, PresetPart::General},
        {selection.motion, PresetPart::Motion},
        {selection.waves, PresetPart::Waves},
        {selection.shapes, PresetPart::Shapes},
        {selection.warpShader, PresetPart::WarpShader},
        {selection.compositeShader, PresetPart::CompositeShader},
    }};
    for (const auto& [enabled, part] : parts) {
        if (enabled) {
            result.copyPartFrom(donor, part);
        }
    }
    return result;
}

std::string MashupEngine::safeName(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        if (std::isalnum(character) != 0 || character == '-' || character == '_') {
            return static_cast<char>(character);
        }
        return '-';
    });
    value.erase(std::unique(value.begin(), value.end(), [](const char left, const char right) {
                    return left == '-' && right == '-';
                }),
                value.end());
    while (!value.empty() && value.front() == '-') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == '-') {
        value.pop_back();
    }
    return value.empty() ? "preset" : value;
}

std::filesystem::path MashupEngine::uniqueOutputPath(const std::filesystem::path& directory,
                                                     const std::string& baseName,
                                                     const std::string& suffix) {
    const auto stem = safeName(baseName) + suffix;
    auto candidate = directory / (stem + ".milk");
    for (int number = 2; std::filesystem::exists(candidate); ++number) {
        candidate = directory / (stem + '-' + std::to_string(number) + ".milk");
    }
    return candidate;
}

} // namespace md3
