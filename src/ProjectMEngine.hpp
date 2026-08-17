#pragma once

#include "Config.hpp"

#include <projectM-4/projectM.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>

namespace md3 {

class ProjectMEngine {
public:
    ProjectMEngine(const Config& config, std::size_t width, std::size_t height);
    ~ProjectMEngine();

    ProjectMEngine(const ProjectMEngine&) = delete;
    ProjectMEngine& operator=(const ProjectMEngine&) = delete;

    void render() const;
    void resize(std::size_t width, std::size_t height) const;
    void loadPreset(const std::filesystem::path& path, bool smooth) const;
    void loadPresetData(const std::string& data, bool smooth) const;
    void loadIdle() const;
    void addAudio(const float* samples, unsigned int frames) const;
    void updateFps(int fps) const;

    [[nodiscard]] bool locked() const;
    void setLocked(bool value) const;
    bool toggleLocked() const;
    float adjustBeatSensitivity(float delta) const;

    void addTouch(float x, float y) const;
    void clearTouches() const;
    [[nodiscard]] std::string lastError() const;

    void setSwitchRequestedCallback(std::function<void(bool)> callback);

private:
    static void switchRequested(bool hardCut, void* context);
    static void switchFailed(const char* filename, const char* message, void* context);
    void clearError() const;
    void setError(std::string message) const;

    projectm_handle handle_{nullptr};
    std::function<void(bool)> switchRequestedCallback_;
    mutable std::mutex errorMutex_;
    mutable std::string lastError_;
};

} // namespace md3
