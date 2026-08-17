#include "ProjectMEngine.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace md3 {

ProjectMEngine::ProjectMEngine(const Config& config, const std::size_t width, const std::size_t height) {
    handle_ = projectm_create();
    if (handle_ == nullptr) {
        throw std::runtime_error("projectM initialization failed; verify OpenGL 3.3 support and GPU drivers");
    }

    projectm_set_window_size(handle_, width, height);
    projectm_set_fps(handle_, config.fps);
    projectm_set_mesh_size(handle_, static_cast<std::size_t>(config.meshWidth),
                           static_cast<std::size_t>(config.meshHeight));
    projectm_set_aspect_correction(handle_, true);
    projectm_set_preset_duration(handle_, config.presetDuration);
    projectm_set_soft_cut_duration(handle_, config.transitionDuration);
    projectm_set_hard_cut_enabled(handle_, config.hardCuts);
    projectm_set_hard_cut_duration(handle_, config.hardCutDuration);
    projectm_set_hard_cut_sensitivity(handle_, config.hardCutSensitivity);
    projectm_set_beat_sensitivity(handle_, config.beatSensitivity);

    std::vector<std::string> textureStrings;
    textureStrings.reserve(config.texturePaths.size());
    for (const auto& path : config.texturePaths) {
        if (std::filesystem::is_directory(path)) {
            textureStrings.push_back(path.string());
        }
    }
    std::vector<const char*> texturePointers;
    texturePointers.reserve(textureStrings.size());
    for (const auto& path : textureStrings) {
        texturePointers.push_back(path.c_str());
    }
    if (!texturePointers.empty()) {
        projectm_set_texture_search_paths(handle_, texturePointers.data(), texturePointers.size());
    }

    projectm_set_preset_switch_requested_event_callback(handle_, &ProjectMEngine::switchRequested, this);
    projectm_set_preset_switch_failed_event_callback(handle_, &ProjectMEngine::switchFailed, this);
}

ProjectMEngine::~ProjectMEngine() {
    if (handle_ != nullptr) {
        projectm_set_preset_switch_requested_event_callback(handle_, nullptr, nullptr);
        projectm_set_preset_switch_failed_event_callback(handle_, nullptr, nullptr);
        projectm_destroy(handle_);
    }
}

void ProjectMEngine::render() const { projectm_opengl_render_frame(handle_); }

void ProjectMEngine::resize(const std::size_t width, const std::size_t height) const {
    if (width > 0 && height > 0) {
        projectm_set_window_size(handle_, width, height);
    }
}

void ProjectMEngine::loadPreset(const std::filesystem::path& path, const bool smooth) const {
    clearError();
    const auto filename = path.string();
    projectm_load_preset_file(handle_, filename.c_str(), smooth);
}

void ProjectMEngine::loadPresetData(const std::string& data, const bool smooth) const {
    clearError();
    projectm_load_preset_data(handle_, data.c_str(), smooth);
}

void ProjectMEngine::loadIdle() const { projectm_load_preset_file(handle_, "idle://", false); }

void ProjectMEngine::addAudio(const float* samples, const unsigned int frames) const {
    if (samples != nullptr && frames > 0) {
        projectm_pcm_add_float(handle_, samples, frames, PROJECTM_STEREO);
    }
}

void ProjectMEngine::updateFps(const int fps) const { projectm_set_fps(handle_, std::max(1, fps)); }

bool ProjectMEngine::locked() const { return projectm_get_preset_locked(handle_); }

void ProjectMEngine::setLocked(const bool value) const { projectm_set_preset_locked(handle_, value); }

bool ProjectMEngine::toggleLocked() const {
    const bool value = !locked();
    projectm_set_preset_locked(handle_, value);
    return value;
}

float ProjectMEngine::adjustBeatSensitivity(const float delta) const {
    const float value = std::max(0.0F, projectm_get_beat_sensitivity(handle_) + delta);
    projectm_set_beat_sensitivity(handle_, value);
    return value;
}

void ProjectMEngine::addTouch(const float x, const float y) const {
    projectm_touch(handle_, std::clamp(x, 0.0F, 1.0F), std::clamp(y, 0.0F, 1.0F), 0,
                   PROJECTM_TOUCH_TYPE_RANDOM);
}

void ProjectMEngine::clearTouches() const { projectm_touch_destroy_all(handle_); }

std::string ProjectMEngine::lastError() const {
    const std::scoped_lock lock(errorMutex_);
    return lastError_;
}

void ProjectMEngine::clearError() const {
    const std::scoped_lock lock(errorMutex_);
    lastError_.clear();
}

void ProjectMEngine::setError(std::string message) const {
    const std::scoped_lock lock(errorMutex_);
    lastError_ = std::move(message);
}

void ProjectMEngine::setSwitchRequestedCallback(std::function<void(bool)> callback) {
    switchRequestedCallback_ = std::move(callback);
}

void ProjectMEngine::switchRequested(const bool hardCut, void* context) {
    auto* engine = static_cast<ProjectMEngine*>(context);
    if (engine != nullptr && engine->switchRequestedCallback_) {
        engine->switchRequestedCallback_(hardCut);
    }
}

void ProjectMEngine::switchFailed(const char* filename, const char* message, void* context) {
    std::string error = "Preset load failed: ";
    error += filename != nullptr && *filename != '\0' ? filename : "<memory>";
    error += ": ";
    error += message != nullptr ? message : "unknown error";
    if (auto* engine = static_cast<ProjectMEngine*>(context); engine != nullptr) {
        engine->setError(error);
    }
    std::cerr << error << '\n';
}

} // namespace md3
