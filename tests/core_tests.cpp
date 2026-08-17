#include "Config.hpp"
#include "PresetCatalog.hpp"

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

} // namespace

int main() {
    try {
        testCatalogScanAndHistory();
        testNonRecursiveScan();
        testCommandLineConfiguration();
        testConfigFileAndCliOverride();
        std::cout << "All core tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}

