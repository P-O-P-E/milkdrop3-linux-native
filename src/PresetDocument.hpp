#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace md3 {

enum class PresetPart {
    General,
    Motion,
    Waves,
    Shapes,
    WarpShader,
    CompositeShader,
    Other,
};

enum class DiagnosticSeverity { Warning, Error };

struct PresetDiagnostic {
    std::size_t line{0};
    DiagnosticSeverity severity{DiagnosticSeverity::Warning};
    std::string message;
};

struct PresetEntry {
    enum class Kind { Blank, Comment, Section, Assignment, Unknown };

    Kind kind{Kind::Blank};
    std::size_t line{0};
    std::string raw;
    std::string key;
    std::string value;
};

class PresetDocument {
public:
    static PresetDocument parse(std::string_view source);
    static PresetDocument load(const std::filesystem::path& path);

    [[nodiscard]] std::string serialize() const;
    void save(const std::filesystem::path& path) const;

    [[nodiscard]] const std::vector<PresetEntry>& entries() const noexcept;
    [[nodiscard]] std::vector<PresetDiagnostic> diagnostics() const;
    [[nodiscard]] bool valid() const;

    [[nodiscard]] std::optional<std::string> value(std::string_view key) const;
    void set(std::string key, std::string value);
    void erasePart(PresetPart part);
    void copyPartFrom(const PresetDocument& donor, PresetPart part);
    [[nodiscard]] PresetDocument extractPart(PresetPart part) const;

    static PresetPart classify(std::string_view key);

private:
    void appendAssignment(const PresetEntry& entry);
    void ensureModernHeader();

    std::vector<PresetEntry> entries_;
};

} // namespace md3
