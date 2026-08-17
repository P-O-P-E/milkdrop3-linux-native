#include "Config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace md3 {
namespace {

std::string trim(std::string value) {
    const auto notSpace = [](const unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::filesystem::path homeDirectory() {
    if (const auto* value = std::getenv("HOME"); value != nullptr && *value != '\0') {
        return value;
    }
    return std::filesystem::current_path();
}

std::filesystem::path dataHome() {
    if (const auto* value = std::getenv("XDG_DATA_HOME"); value != nullptr && *value != '\0') {
        return value;
    }
    return homeDirectory() / ".local" / "share";
}

std::filesystem::path expandPath(std::string value) {
    const auto home = homeDirectory().string();
    if (value == "~") {
        value = home;
    } else if (value.starts_with("~/")) {
        value = home + value.substr(1);
    }

    for (const auto& token : {std::string("${HOME}"), std::string("$HOME")}) {
        std::size_t position = 0;
        while ((position = value.find(token, position)) != std::string::npos) {
            value.replace(position, token.size(), home);
            position += home.size();
        }
    }

    return std::filesystem::path(value).lexically_normal();
}

bool parseBool(const std::string& value) {
    const auto normalized = lower(trim(value));
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    throw std::runtime_error("Invalid boolean value: " + value);
}

int parseInt(const std::string& value, const std::string& name) {
    std::size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    if (consumed != value.size()) {
        throw std::runtime_error("Invalid integer for " + name + ": " + value);
    }
    return parsed;
}

double parseDouble(const std::string& value, const std::string& name) {
    std::size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    if (consumed != value.size()) {
        throw std::runtime_error("Invalid number for " + name + ": " + value);
    }
    return parsed;
}

void applySetting(Config& config, const std::string& rawKey, const std::string& rawValue) {
    const auto key = lower(trim(rawKey));
    const auto value = trim(rawValue);

    if (key == "preset_dir") {
        config.presetPaths.push_back(expandPath(value));
    } else if (key == "texture_dir") {
        config.texturePaths.push_back(expandPath(value));
    } else if (key == "audio_device") {
        config.audioDevice = value;
    } else if (key == "library_file") {
        config.libraryFile = expandPath(value);
    } else if (key == "generated_preset_dir") {
        config.generatedPresetDirectory = expandPath(value);
    } else if (key == "ui_state_file") {
        config.uiStateFile = expandPath(value);
    } else if (key == "width") {
        config.width = parseInt(value, key);
    } else if (key == "height") {
        config.height = parseInt(value, key);
    } else if (key == "fps") {
        config.fps = parseInt(value, key);
    } else if (key == "mesh_width") {
        config.meshWidth = parseInt(value, key);
    } else if (key == "mesh_height") {
        config.meshHeight = parseInt(value, key);
    } else if (key == "preset_duration") {
        config.presetDuration = parseDouble(value, key);
    } else if (key == "transition_duration") {
        config.transitionDuration = parseDouble(value, key);
    } else if (key == "hard_cut_duration") {
        config.hardCutDuration = parseDouble(value, key);
    } else if (key == "beat_sensitivity") {
        config.beatSensitivity = static_cast<float>(parseDouble(value, key));
    } else if (key == "hard_cut_sensitivity") {
        config.hardCutSensitivity = static_cast<float>(parseDouble(value, key));
    } else if (key == "fullscreen") {
        config.fullscreen = parseBool(value);
    } else if (key == "shuffle") {
        config.shuffle = parseBool(value);
    } else if (key == "recursive") {
        config.recursive = parseBool(value);
    } else if (key == "hard_cuts") {
        config.hardCuts = parseBool(value);
    } else if (key == "vsync") {
        config.vsync = parseBool(value);
    } else if (key == "ui_enabled") {
        config.uiEnabled = parseBool(value);
    } else if (key == "status_overlay") {
        config.statusOverlay = parseBool(value);
    } else {
        throw std::runtime_error("Unknown configuration key: " + rawKey);
    }
}

void loadConfigFile(Config& config, const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Unable to open configuration file: " + path.string());
    }

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty() || (line.front() == '[' && line.back() == ']')) {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::runtime_error(path.string() + ":" + std::to_string(lineNumber) +
                                     ": expected key=value");
        }
        try {
            applySetting(config, line.substr(0, separator), line.substr(separator + 1));
        } catch (const std::exception& error) {
            throw std::runtime_error(path.string() + ":" + std::to_string(lineNumber) + ": " + error.what());
        }
    }
}

std::string requiredValue(int& index, const int argc, char** argv, const std::string& option) {
    if (index + 1 >= argc) {
        throw std::runtime_error(option + " requires a value");
    }
    ++index;
    return argv[index];
}

void validate(const Config& config) {
    if (config.width < 320 || config.height < 240) {
        throw std::runtime_error("Window dimensions must be at least 320x240");
    }
    if (config.fps < 1 || config.fps > 360) {
        throw std::runtime_error("FPS must be between 1 and 360");
    }
    if (config.meshWidth < 8 || config.meshWidth > 300 || config.meshHeight < 8 || config.meshHeight > 300) {
        throw std::runtime_error("Mesh dimensions must be between 8 and 300");
    }
    if (config.presetDuration < 0.0 || config.transitionDuration < 0.0 || config.hardCutDuration < 0.0) {
        throw std::runtime_error("Durations cannot be negative");
    }
}

} // namespace

std::filesystem::path Config::defaultConfigPath() {
    if (const auto* value = std::getenv("XDG_CONFIG_HOME"); value != nullptr && *value != '\0') {
        return std::filesystem::path(value) / "milkdrop3-linux" / "config.ini";
    }
    return homeDirectory() / ".config" / "milkdrop3-linux" / "config.ini";
}

Config Config::fromCommandLine(const int argc, char** argv) {
    Config config;
    config.presetPaths = {
        dataHome() / "milkdrop3-linux" / "presets",
        std::filesystem::current_path() / "presets",
    };
    config.texturePaths = {
        dataHome() / "milkdrop3-linux" / "textures",
        std::filesystem::current_path() / "textures",
    };
    config.libraryFile = dataHome() / "milkdrop3-linux" / "library.db";
    config.generatedPresetDirectory = dataHome() / "milkdrop3-linux" / "generated";
    config.uiStateFile = dataHome() / "milkdrop3-linux" / "ui.ini";

    std::optional<std::filesystem::path> explicitConfig;
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--config") {
            explicitConfig = expandPath(requiredValue(index, argc, argv, "--config"));
        }
    }

    if (explicitConfig.has_value()) {
        loadConfigFile(config, *explicitConfig);
    } else if (std::filesystem::exists(defaultConfigPath())) {
        loadConfigFile(config, defaultConfigPath());
    }

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--config") {
            ++index;
        } else if (argument == "--preset-dir") {
            config.presetPaths.push_back(expandPath(requiredValue(index, argc, argv, argument)));
        } else if (argument == "--texture-dir") {
            config.texturePaths.push_back(expandPath(requiredValue(index, argc, argv, argument)));
        } else if (argument == "--audio-device") {
            config.audioDevice = requiredValue(index, argc, argv, argument);
        } else if (argument == "--library-file") {
            config.libraryFile = expandPath(requiredValue(index, argc, argv, argument));
        } else if (argument == "--generated-preset-dir") {
            config.generatedPresetDirectory = expandPath(requiredValue(index, argc, argv, argument));
        } else if (argument == "--width") {
            config.width = parseInt(requiredValue(index, argc, argv, argument), argument);
        } else if (argument == "--height") {
            config.height = parseInt(requiredValue(index, argc, argv, argument), argument);
        } else if (argument == "--fps") {
            config.fps = parseInt(requiredValue(index, argc, argv, argument), argument);
        } else if (argument == "--preset-duration") {
            config.presetDuration = parseDouble(requiredValue(index, argc, argv, argument), argument);
        } else if (argument == "--transition-duration") {
            config.transitionDuration = parseDouble(requiredValue(index, argc, argv, argument), argument);
        } else if (argument == "--beat-sensitivity") {
            config.beatSensitivity = static_cast<float>(parseDouble(requiredValue(index, argc, argv, argument), argument));
        } else if (argument == "--fullscreen") {
            config.fullscreen = true;
        } else if (argument == "--no-shuffle") {
            config.shuffle = false;
        } else if (argument == "--no-recursive") {
            config.recursive = false;
        } else if (argument == "--disable-hard-cuts") {
            config.hardCuts = false;
        } else if (argument == "--no-vsync") {
            config.vsync = false;
        } else if (argument == "--no-ui") {
            config.uiEnabled = false;
        } else if (argument == "--no-status-overlay") {
            config.statusOverlay = false;
        } else if (argument == "--list-audio-devices") {
            config.listAudioDevices = true;
        } else if (argument == "--help" || argument == "-h") {
            config.showHelp = true;
        } else if (argument == "--version") {
            config.showVersion = true;
        } else {
            throw std::runtime_error("Unknown option: " + argument + "\n\n" + helpText());
        }
    }

    validate(config);
    return config;
}

std::string Config::helpText() {
    return R"(MilkDrop3 Linux Native

Usage: milkdrop3-linux [options]

  --preset-dir PATH           Add a MilkDrop preset directory
  --texture-dir PATH          Add a texture search directory
  --audio-device NAME|INDEX   Capture from a named or numbered input
  --list-audio-devices        Print capture devices and exit
  --library-file PATH         Ratings/favorites database path
  --generated-preset-dir PATH Edited and mashup preset output directory
  --config PATH               Use a specific key=value configuration file
  --width PIXELS              Initial window width (default: 1280)
  --height PIXELS             Initial window height (default: 720)
  --fps RATE                  Target frame rate (default: 60)
  --preset-duration SECONDS   Automatic preset duration
  --transition-duration SEC   Smooth transition duration
  --beat-sensitivity VALUE    Initial projectM beat sensitivity
  --fullscreen                Start in desktop fullscreen mode
  --no-shuffle                Use sorted preset order
  --no-recursive              Do not scan preset subdirectories
  --disable-hard-cuts         Disable beat-driven hard cuts
  --no-vsync                  Disable vertical synchronization
  --no-ui                     Disable the in-window browser and editor
  --no-status-overlay         Hide the always-on preset/status overlay
  --version                   Print the application version
  -h, --help                  Show this help
)";
}

} // namespace md3
