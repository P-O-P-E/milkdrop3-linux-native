#include "Config.hpp"
#include "GeneratedPresetStore.hpp"
#include "MashupEngine.hpp"
#include "OverlayManager.hpp"
#include "PresetCatalog.hpp"
#include "PresetDocument.hpp"
#include "PresetLibrary.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

md3::Config parse(std::vector<std::string> arguments) {
    std::vector<char*> pointers;
    pointers.reserve(arguments.size());
    for (auto& argument : arguments) {
        pointers.push_back(argument.data());
    }
    return md3::Config::fromCommandLine(static_cast<int>(pointers.size()), pointers.data());
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() /
                ("milkdrop3-linux-tests-" + std::to_string(std::filesystem::file_time_type::clock::now()
                                                                .time_since_epoch()
                                                                .count()));
        std::filesystem::create_directories(path_);
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void writeFile(const std::filesystem::path& path, const std::string& contents = "test") {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    stream << contents;
}

void testCatalogScanAndHistory() {
    TemporaryDirectory temporary;
    writeFile(temporary.path() / "one.milk");
    writeFile(temporary.path() / "nested" / "two.MILK");
    writeFile(temporary.path() / "nested" / "three.prjm");
    writeFile(temporary.path() / "ignored.txt");

    md3::PresetCatalog catalog(false);
    require(catalog.addPath(temporary.path(), true) == 3, "recursive scan should add three presets");
    require(catalog.size() == 3, "catalog should contain three presets");
    require(catalog.addPath(temporary.path(), true) == 0, "duplicate scan should add nothing");

    const auto first = catalog.selectInitial();
    const auto second = catalog.next();
    require(first.has_value() && second.has_value() && first != second, "ordered next should advance");
    require(catalog.previous() == first, "previous should return playback history");
    require(catalog.next() == second, "next should move forward through existing history");
}

void testNonRecursiveScan() {
    TemporaryDirectory temporary;
    writeFile(temporary.path() / "top.milk");
    writeFile(temporary.path() / "nested" / "hidden.milk");

    md3::PresetCatalog catalog;
    require(catalog.addPath(temporary.path(), false) == 1, "non-recursive scan should only add top-level preset");
}

void testCommandLineConfiguration() {
    const auto config = parse({"milkdrop3-linux", "--width", "1920", "--height", "1080", "--fps", "90",
                               "--preset-dir", "/tmp/presets", "--audio-device", "monitor", "--fullscreen",
                               "--no-shuffle", "--disable-hard-cuts"});
    require(config.width == 1920 && config.height == 1080, "window dimensions should be parsed");
    require(config.fps == 90, "fps should be parsed");
    require(config.fullscreen, "fullscreen should be enabled");
    require(!config.shuffle, "shuffle should be disabled");
    require(!config.hardCuts, "hard cuts should be disabled");
    require(config.audioDevice == "monitor", "audio device should be parsed");
}

void testConfigFileAndCliOverride() {
    TemporaryDirectory temporary;
    const auto configPath = temporary.path() / "config.ini";
    writeFile(configPath, "fps=75\nshuffle=false\npreset_duration=42\npreset_dir=~/visuals\n");

    const auto config = parse({"milkdrop3-linux", "--config", configPath.string(), "--fps", "120"});
    require(config.fps == 120, "command line should override the config file");
    require(!config.shuffle, "config file boolean should be loaded");
    require(config.presetDuration == 42.0, "config file number should be loaded");
}

void testPresetDocumentParsingAndDiagnostics() {
    const auto source = R"(MILKDROP_PRESET_VERSION=201
PSVERSION_WARP=3
PSVERSION_COMP=2
[preset00]
zoom=1.00000
per_frame_1=zoom=1.01;
per_frame_3=rot=rot+0.01;
warp_1=shader_body {
warp_2=ret=tex2D(sampler_main,uv).xyz;
warp_3=}
)";
    auto document = md3::PresetDocument::parse(source);
    require(document.valid(), "well-formed preset should be valid");
    require(document.value("zoom") == "1.00000", "preset value should be readable");
    require(md3::PresetDocument::classify("per_pixel_1") == md3::PresetPart::Motion,
            "per-pixel equations should be classified as motion");
    require(md3::PresetDocument::classify("warp_1") == md3::PresetPart::WarpShader,
            "warp shader should be classified");
    const auto diagnostics = document.diagnostics();
    require(!diagnostics.empty(), "numbered code gap should produce a warning");

    document.set("zoom", "1.25000");
    require(document.value("zoom") == "1.25000", "preset value should be replaceable");
    require(md3::PresetDocument::parse(document.serialize()).valid(), "serialized preset should remain valid");
}

void testMashupComposition() {
    const auto base = md3::PresetDocument::parse(R"([preset00]
zoom=1.0
wavecode_0_enabled=1
warp_1=shader_body { ret=1; }
comp_1=shader_body { ret=2; }
)");
    const auto donor = md3::PresetDocument::parse(R"([preset00]
zoom=2.0
wavecode_0_enabled=0
warp_1=shader_body { ret=3; }
comp_1=shader_body { ret=4; }
)");

    md3::MashupSelection selection;
    selection.warpShader = true;
    selection.compositeShader = false;
    selection.waves = true;
    const auto mashup = md3::MashupEngine::combine(base, donor, selection);
    require(mashup.value("zoom") == "1.0", "unselected motion should stay from base");
    require(mashup.value("wavecode_0_enabled") == "0", "selected waves should come from donor");
    require(mashup.value("warp_1") == "shader_body { ret=3; }", "selected warp should come from donor");
    require(mashup.value("comp_1") == "shader_body { ret=2; }", "base composite should be retained");
    require(mashup.value("MILKDROP_PRESET_VERSION") == "201", "shader mashup should set a modern header");
}

void testPresetLibraryPersistence() {
    TemporaryDirectory temporary;
    const auto preset = temporary.path() / "rated.milk";
    const auto database = temporary.path() / "library.db";
    writeFile(preset, "[preset00]\n");

    md3::PresetLibrary library(database);
    library.setRating(preset, 5);
    require(library.toggleFavorite(preset), "favorite should toggle on");
    library.recordPlayed(preset);
    library.save();

    md3::PresetLibrary reloaded(database);
    reloaded.load();
    const auto metadata = reloaded.metadata(preset);
    require(metadata.rating == 5, "rating should persist");
    require(metadata.favorite, "favorite should persist");
    require(metadata.playCount == 1, "play count should persist");
    require(reloaded.selectionWeight(preset) == 10.0, "favorite rating should affect shuffle weight");
}

void testPlaylistImportExportAndSelection() {
    TemporaryDirectory temporary;
    const auto first = temporary.path() / "one.milk";
    const auto second = temporary.path() / "two.milk";
    const auto playlist = temporary.path() / "presets.m3u";
    writeFile(first);
    writeFile(second);

    md3::PresetCatalog source(false);
    source.addPath(first);
    source.addPath(second);
    source.exportPlaylist(playlist);

    md3::PresetCatalog imported(false);
    require(imported.importPlaylist(playlist) == 2, "playlist should import two presets");
    require(imported.select(second).has_value(), "catalog should select an explicit preset");
    require(imported.current().has_value() && imported.current()->filename() == "two.milk",
            "explicit selection should become current");
}

void testOverlayExpiry() {
    md3::OverlayManager overlays;
    overlays.push("expired", md3::OverlaySeverity::Information, std::chrono::milliseconds(0));
    require(overlays.active().empty(), "zero-duration overlay should expire immediately");
}

void testGeneratedPresetStoreSafety() {
    TemporaryDirectory temporary;
    const auto generated = temporary.path() / "generated";
    const auto external = temporary.path() / "community" / "external.milk";
    const auto original = generated / "created.milk";
    writeFile(original, "[preset00]\n");
    writeFile(external, "[preset00]\n");

    md3::GeneratedPresetStore store(generated);
    require(store.owns(original), "generated preset should be owned by the workspace");
    require(!store.owns(external), "community preset should not be owned by the workspace");
    const auto renamed = store.renamePreset(original, "New name");
    require(renamed.filename() == "New-name.milk", "rename should sanitize the requested name");
    require(std::filesystem::exists(renamed), "renamed preset should exist");
    const auto trashed = store.moveToTrash(renamed);
    require(trashed.parent_path().filename() == ".trash", "deleted preset should move to recoverable trash");
    require(std::filesystem::exists(trashed), "trashed preset should remain recoverable");

    md3::PresetCatalog catalog;
    require(catalog.addPath(generated, true) == 0, "catalog scans should skip the generated trash directory");

    bool refusedExternalRename = false;
    try {
        static_cast<void>(store.renamePreset(external, "not-allowed"));
    } catch (const std::runtime_error&) {
        refusedExternalRename = true;
    }
    require(refusedExternalRename, "community presets must not be renamed by the generated store");
}

} // namespace

int main() {
    try {
        testCatalogScanAndHistory();
        testNonRecursiveScan();
        testCommandLineConfiguration();
        testConfigFileAndCliOverride();
        testPresetDocumentParsingAndDiagnostics();
        testMashupComposition();
        testPresetLibraryPersistence();
        testPlaylistImportExportAndSelection();
        testOverlayExpiry();
        testGeneratedPresetStoreSafety();
        std::cout << "All core tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
