#include "PresetDocument.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace md3 {
namespace {

std::string trim(std::string value) {
    const auto notSpace = [](const unsigned char character) { return std::isspace(character) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool startsWith(const std::string_view value, const std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool isOneOf(const std::string_view key, const std::initializer_list<std::string_view> values) {
    return std::find(values.begin(), values.end(), key) != values.end();
}

bool hasNumberedPrefix(const std::string_view key, const std::string_view prefix) {
    if (!startsWith(key, prefix)) {
        return false;
    }
    auto suffix = key.substr(prefix.size());
    if (suffix.empty()) {
        return false;
    }
    return std::all_of(suffix.begin(), suffix.end(), [](const unsigned char character) {
        return std::isdigit(character) != 0;
    });
}

std::optional<std::pair<std::string, int>> numberedCodeKey(const std::string& key) {
    static const std::vector<std::string> prefixes{
        "per_frame_init_", "per_frame_", "per_pixel_", "warp_", "comp_",
    };
    for (const auto& prefix : prefixes) {
        if (hasNumberedPrefix(key, prefix)) {
            return std::pair{prefix, std::stoi(key.substr(prefix.size()))};
        }
    }

    const auto matchWaveOrShape = [&key](const std::string& marker) -> std::optional<std::pair<std::string, int>> {
        if (!startsWith(key, marker)) {
            return std::nullopt;
        }
        const auto finalUnderscore = key.rfind('_');
        if (finalUnderscore == std::string::npos || finalUnderscore + 1 >= key.size()) {
            return std::nullopt;
        }
        const auto suffix = key.substr(finalUnderscore + 1);
        if (!std::all_of(suffix.begin(), suffix.end(), [](const unsigned char character) {
                return std::isdigit(character) != 0;
            })) {
            return std::nullopt;
        }
        return std::pair{key.substr(0, finalUnderscore + 1), std::stoi(suffix)};
    };

    if (const auto wave = matchWaveOrShape("wave_"); wave.has_value()) {
        return wave;
    }
    return matchWaveOrShape("shape_");
}

} // namespace

PresetDocument PresetDocument::parse(const std::string_view source) {
    if (source.find('\0') != std::string_view::npos) {
        throw std::runtime_error("Preset data contains a NUL byte");
    }
    if (source.size() > 4U * 1024U * 1024U) {
        throw std::runtime_error("Preset data exceeds the 4 MiB safety limit");
    }

    PresetDocument document;
    std::istringstream stream{std::string(source)};
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (lineNumber == 1 && line.starts_with("\xEF\xBB\xBF")) {
            line.erase(0, 3);
        }

        PresetEntry entry;
        entry.line = lineNumber;
        entry.raw = line;
        const auto stripped = trim(line);
        if (stripped.empty()) {
            entry.kind = PresetEntry::Kind::Blank;
        } else if (stripped.front() == ';' || stripped.front() == '#' || stripped.starts_with("//")) {
            entry.kind = PresetEntry::Kind::Comment;
        } else if (stripped.front() == '[' && stripped.back() == ']') {
            entry.kind = PresetEntry::Kind::Section;
        } else if (const auto separator = line.find('='); separator != std::string::npos) {
            entry.key = trim(line.substr(0, separator));
            entry.value = line.substr(separator + 1);
            entry.kind = entry.key.empty() ? PresetEntry::Kind::Unknown : PresetEntry::Kind::Assignment;
        } else {
            entry.kind = PresetEntry::Kind::Unknown;
        }
        document.entries_.push_back(std::move(entry));
    }
    return document;
}

PresetDocument PresetDocument::load(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Unable to open preset: " + path.string());
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        throw std::runtime_error("Unable to read preset: " + path.string());
    }
    return parse(contents.str());
}

std::string PresetDocument::serialize() const {
    std::ostringstream output;
    for (const auto& entry : entries_) {
        if (entry.kind == PresetEntry::Kind::Assignment) {
            output << entry.key << '=' << entry.value;
        } else {
            output << entry.raw;
        }
        output << '\n';
    }
    return output.str();
}

void PresetDocument::save(const std::filesystem::path& path) const {
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("Unable to create preset: " + path.string());
    }
    stream << serialize();
    if (!stream) {
        throw std::runtime_error("Unable to write preset: " + path.string());
    }
}

const std::vector<PresetEntry>& PresetDocument::entries() const noexcept { return entries_; }

std::vector<PresetDiagnostic> PresetDocument::diagnostics() const {
    std::vector<PresetDiagnostic> result;
    std::unordered_set<std::string> seen;
    std::map<std::string, std::set<int>> sequences;
    bool hasPresetSection = false;

    for (const auto& entry : entries_) {
        if (entry.kind == PresetEntry::Kind::Unknown) {
            result.push_back({entry.line, DiagnosticSeverity::Error, "Expected a section, comment, or key=value"});
            continue;
        }
        if (entry.kind == PresetEntry::Kind::Section && lower(trim(entry.raw)) == "[preset00]") {
            hasPresetSection = true;
        }
        if (entry.kind != PresetEntry::Kind::Assignment) {
            continue;
        }
        const auto normalized = lower(entry.key);
        if (!seen.insert(normalized).second) {
            result.push_back({entry.line, DiagnosticSeverity::Warning,
                              "Duplicate key; MilkDrop-compatible parsers may use only the first value: " + entry.key});
        }
        if (const auto numbered = numberedCodeKey(normalized); numbered.has_value()) {
            sequences[numbered->first].insert(numbered->second);
        }
    }

    if (!hasPresetSection) {
        result.push_back({0, DiagnosticSeverity::Warning, "Preset has no [preset00] section"});
    }
    for (const auto& [prefix, numbers] : sequences) {
        if (numbers.empty()) {
            continue;
        }
        const int first = *numbers.begin();
        const int last = *numbers.rbegin();
        if (first != 1) {
            result.push_back({0, DiagnosticSeverity::Warning,
                              "Numbered code sequence should start at " + prefix + "1"});
        }
        for (int number = first; number <= last; ++number) {
            if (!numbers.contains(number)) {
                result.push_back({0, DiagnosticSeverity::Warning,
                                  "Numbered code sequence contains a gap at " + prefix + std::to_string(number)});
                break;
            }
        }
    }
    return result;
}

bool PresetDocument::valid() const {
    const auto issues = diagnostics();
    return std::none_of(issues.begin(), issues.end(), [](const PresetDiagnostic& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Error;
    });
}

std::optional<std::string> PresetDocument::value(const std::string_view key) const {
    const auto normalized = lower(std::string(key));
    const auto iterator = std::find_if(entries_.begin(), entries_.end(), [&normalized](const PresetEntry& entry) {
        return entry.kind == PresetEntry::Kind::Assignment && lower(entry.key) == normalized;
    });
    if (iterator == entries_.end()) {
        return std::nullopt;
    }
    return iterator->value;
}

void PresetDocument::set(std::string key, std::string value) {
    const auto normalized = lower(key);
    const auto iterator = std::find_if(entries_.begin(), entries_.end(), [&normalized](const PresetEntry& entry) {
        return entry.kind == PresetEntry::Kind::Assignment && lower(entry.key) == normalized;
    });
    if (iterator != entries_.end()) {
        iterator->value = std::move(value);
        return;
    }
    PresetEntry entry;
    entry.kind = PresetEntry::Kind::Assignment;
    entry.line = entries_.size() + 1;
    entry.key = std::move(key);
    entry.value = std::move(value);
    entries_.push_back(std::move(entry));
}

void PresetDocument::erasePart(const PresetPart part) {
    std::erase_if(entries_, [part](const PresetEntry& entry) {
        return entry.kind == PresetEntry::Kind::Assignment && classify(entry.key) == part;
    });
}

void PresetDocument::appendAssignment(const PresetEntry& source) {
    PresetEntry entry = source;
    entry.line = entries_.size() + 1;
    entries_.push_back(std::move(entry));
}

void PresetDocument::ensureModernHeader() {
    const auto parseVersion = [this](const std::string_view key) {
        try {
            return std::clamp(std::stoi(value(key).value_or("2")), 0, 3);
        } catch (const std::exception&) {
            return 2;
        }
    };
    const int warp = parseVersion("PSVERSION_WARP");
    const int composite = parseVersion("PSVERSION_COMP");
    const std::unordered_set<std::string> headerKeys{
        "milkdrop_preset_version", "psversion", "psversion_warp", "psversion_comp",
    };
    std::erase_if(entries_, [&headerKeys](const PresetEntry& entry) {
        return entry.kind == PresetEntry::Kind::Assignment && headerKeys.contains(lower(entry.key));
    });

    std::vector<PresetEntry> header;
    const auto add = [&header](std::string key, std::string value) {
        PresetEntry entry;
        entry.kind = PresetEntry::Kind::Assignment;
        entry.key = std::move(key);
        entry.value = std::move(value);
        header.push_back(std::move(entry));
    };
    add("MILKDROP_PRESET_VERSION", "201");
    add("PSVERSION", std::to_string(std::max(warp, composite)));
    add("PSVERSION_WARP", std::to_string(warp));
    add("PSVERSION_COMP", std::to_string(composite));
    entries_.insert(entries_.begin(), header.begin(), header.end());
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        entries_[index].line = index + 1;
    }
}

void PresetDocument::copyPartFrom(const PresetDocument& donor, const PresetPart part) {
    erasePart(part);
    for (const auto& entry : donor.entries_) {
        if (entry.kind == PresetEntry::Kind::Assignment && classify(entry.key) == part) {
            appendAssignment(entry);
        }
    }
    if (part == PresetPart::WarpShader || part == PresetPart::CompositeShader) {
        ensureModernHeader();
    }
}

PresetDocument PresetDocument::extractPart(const PresetPart part) const {
    PresetDocument result = parse("[preset00]\n");
    for (const auto& entry : entries_) {
        if (entry.kind == PresetEntry::Kind::Assignment && classify(entry.key) == part) {
            result.appendAssignment(entry);
        }
    }
    if (part == PresetPart::WarpShader || part == PresetPart::CompositeShader) {
        result.ensureModernHeader();
    }
    return result;
}

PresetPart PresetDocument::classify(const std::string_view rawKey) {
    const auto key = lower(std::string(rawKey));
    if (key == "psversion_warp" || hasNumberedPrefix(key, "warp_")) {
        return PresetPart::WarpShader;
    }
    if (key == "psversion_comp" || hasNumberedPrefix(key, "comp_")) {
        return PresetPart::CompositeShader;
    }
    if (startsWith(key, "shapecode_") || startsWith(key, "shape_")) {
        return PresetPart::Shapes;
    }
    if (startsWith(key, "wavecode_") || startsWith(key, "wave_")) {
        return PresetPart::Waves;
    }
    if (isOneOf(key, {"nwavemode", "badditivewaves", "bwavedots", "bwavethick",
                      "bmodwavealphabyvolume", "bmaximizewavecolor", "fwavealpha", "fwavescale",
                      "fwavesmoothing", "fwaveparam", "fmodwavealphastart", "fmodwavealphaend",
                      "nmotionvectorsx", "nmotionvectorsy", "mv_dx", "mv_dy", "mv_l", "mv_r",
                      "mv_g", "mv_b", "mv_a", "bmotionvectorson"})) {
        return PresetPart::Waves;
    }
    if (startsWith(key, "per_frame_init_") || startsWith(key, "per_frame_") ||
        startsWith(key, "per_pixel_") ||
        isOneOf(key, {"zoom", "rot", "cx", "cy", "dx", "dy", "warp", "sx", "sy", "btexwrap",
                      "bdarkencenter", "fwarpanimspeed", "fwarpscale", "fzoomexponent", "ob_size",
                      "ob_r", "ob_g", "ob_b", "ob_a", "ib_size", "ib_r", "ib_g", "ib_b", "ib_a"})) {
        return PresetPart::Motion;
    }
    if (isOneOf(key, {"milkdrop_preset_version", "psversion", "frating", "fgammaadj", "fdecay",
                      "fvideoechozoom", "fvideoechoalpha", "nvideoechoorientation", "bredbluestereo",
                      "bbrighten", "bdarken", "bsolarize", "binvert", "fshader", "b1n", "b2n",
                      "b3n", "b1x", "b2x", "b3x", "b1ed"})) {
        return PresetPart::General;
    }
    return PresetPart::Other;
}

} // namespace md3
