#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace md3 {

struct Config {
    std::vector<std::filesystem::path> presetPaths;
    std::vector<std::filesystem::path> texturePaths;
    std::string audioDevice;

    int width{1280};
    int height{720};
    int fps{60};
    int meshWidth{96};
    int meshHeight{54};

    double presetDuration{30.0};
    double transitionDuration{3.0};
    double hardCutDuration{10.0};
    float beatSensitivity{1.0F};
    float hardCutSensitivity{1.0F};

    bool fullscreen{false};
    bool shuffle{true};
    bool recursive{true};
    bool hardCuts{true};
    bool vsync{true};
    bool listAudioDevices{false};
    bool showHelp{false};
    bool showVersion{false};

    static Config fromCommandLine(int argc, char** argv);
    static std::filesystem::path defaultConfigPath();
    static std::string helpText();
};

} // namespace md3

