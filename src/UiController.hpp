#pragma once

#include <SDL2/SDL.h>

#include <filesystem>
#include <functional>
#include <memory>

namespace md3 {

class OverlayManager;
class PresetCatalog;
class PresetLibrary;
class ProjectMEngine;

struct UiCallbacks {
    std::function<void(bool)> next;
    std::function<void(bool)> previous;
    std::function<void(const std::filesystem::path&, bool)> select;
    std::function<void()> updateTitle;
};

class UiController {
public:
    UiController(SDL_Window* window, SDL_GLContext context, PresetCatalog& catalog,
                 PresetLibrary& library, ProjectMEngine& engine, OverlayManager& overlays,
                 std::filesystem::path generatedDirectory, std::filesystem::path stateFile,
                 bool statusOverlay, UiCallbacks callbacks);
    ~UiController();

    UiController(const UiController&) = delete;
    UiController& operator=(const UiController&) = delete;

    void processEvent(const SDL_Event& event);
    void beginFrame();
    void draw();
    void render();

    void toggle();
    [[nodiscard]] bool visible() const;
    [[nodiscard]] bool wantsKeyboard() const;
    [[nodiscard]] bool wantsMouse() const;

    bool addImageOverlay(const std::filesystem::path& path);
    static bool isImageFile(const std::filesystem::path& path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace md3
