#pragma once

#include "Config.hpp"
#include "OverlayManager.hpp"
#include "PresetCatalog.hpp"
#include "PresetLibrary.hpp"

#include <SDL2/SDL.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>

namespace md3 {

class AudioCapture;
class FadeOverlay;
class ProjectMEngine;
class UiController;

class Application {
public:
    explicit Application(Config config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    void initialize();
    void shutdown();
    void pollEvents();
    void handleKey(const SDL_KeyboardEvent& event);
    void handleDrop(const char* path);
    void resize();
    void loadNext(bool smooth);
    void loadPrevious(bool smooth);
    void loadCurrent(bool smooth);
    void beginPresetFade(const std::filesystem::path& preset);
    void updatePresetFade(std::chrono::steady_clock::time_point now);
    void applyPreset(const std::filesystem::path& preset, bool smooth);
    void selectAudio(int index);
    void updateTitle();
    void toggleFullscreen();
    void printControls() const;

    Config config_;
    PresetCatalog catalog_;
    PresetLibrary library_;
    OverlayManager overlays_;
    SDL_Window* window_{nullptr};
    SDL_GLContext glContext_{nullptr};
    std::unique_ptr<ProjectMEngine> engine_;
    std::unique_ptr<AudioCapture> audio_;
    std::unique_ptr<FadeOverlay> fadeOverlay_;
    std::unique_ptr<UiController> ui_;
    enum class FadePhase { Idle, Out, In };
    FadePhase fadePhase_{FadePhase::Idle};
    std::optional<std::filesystem::path> pendingPreset_;
    std::chrono::steady_clock::time_point fadePhaseStart_{};
    double fadePhaseDuration_{0.0};
    float fadePhaseStartOpacity_{0.0F};
    float fadeOpacity_{0.0F};
    bool running_{false};
    bool fullscreen_{false};
    std::chrono::steady_clock::time_point fpsWindowStart_{};
    unsigned int fpsFrameCount_{0};
};

} // namespace md3
