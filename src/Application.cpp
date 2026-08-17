#include "Application.hpp"

#include "AudioCapture.hpp"
#include "ProjectMEngine.hpp"
#include "UiController.hpp"

#include <SDL2/SDL_opengl.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace md3 {

Application::Application(Config config)
    : config_(std::move(config)), catalog_(config_.shuffle), library_(config_.libraryFile) {}
Application::~Application() { shutdown(); }

void Application::initialize() {
#ifdef SDL_HINT_AUDIO_INCLUDE_MONITORS
    SDL_SetHint(SDL_HINT_AUDIO_INCLUDE_MONITORS, "1");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        throw std::runtime_error("SDL initialization failed: " + std::string(SDL_GetError()));
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (config_.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    window_ = SDL_CreateWindow("MilkDrop3 Linux Native", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               config_.width, config_.height, flags);
    if (window_ == nullptr) {
        throw std::runtime_error("Window creation failed: " + std::string(SDL_GetError()));
    }
    glContext_ = SDL_GL_CreateContext(window_);
    if (glContext_ == nullptr) {
        throw std::runtime_error("OpenGL context creation failed: " + std::string(SDL_GetError()));
    }
    if (SDL_GL_MakeCurrent(window_, glContext_) != 0) {
        throw std::runtime_error("Unable to activate OpenGL context: " + std::string(SDL_GetError()));
    }
    SDL_GL_SetSwapInterval(config_.vsync ? 1 : 0);
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window_, &drawableWidth, &drawableHeight);
    engine_ = std::make_unique<ProjectMEngine>(config_, static_cast<std::size_t>(drawableWidth),
                                               static_cast<std::size_t>(drawableHeight));
    engine_->setSwitchRequestedCallback([this](const bool hardCut) { loadNext(!hardCut); });

    try {
        library_.load();
    } catch (const std::exception& error) {
        std::cerr << "Unable to load ratings/favorites: " << error.what() << '\n';
        overlays_.push(error.what(), OverlaySeverity::Warning);
    }
    catalog_.setWeightProvider([this](const std::filesystem::path& preset) {
        return library_.selectionWeight(preset);
    });

    for (const auto& path : config_.presetPaths) {
        const auto added = catalog_.addPath(path, config_.recursive);
        if (added > 0) {
            std::cout << "Added " << added << " presets from " << path << '\n';
        }
    }
    catalog_.addPath(config_.generatedPresetDirectory, true);
    if (catalog_.empty()) {
        std::cerr << "No .milk presets found; displaying the projectM idle preset.\n"
                     "Run scripts/get-presets.sh or pass --preset-dir PATH.\n";
        engine_->loadIdle();
    } else {
        catalog_.selectInitial();
        loadCurrent(false);
    }

    audio_ = std::make_unique<AudioCapture>(*engine_);
    try {
        audio_->start(config_.audioDevice);
        std::cout << "Capturing audio from: " << audio_->deviceName() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Audio capture is unavailable: " << error.what() << '\n'
                  << "The visualizer will continue without live audio.\n";
    }

    if (config_.uiEnabled || config_.statusOverlay) {
        UiCallbacks callbacks;
        callbacks.next = [this](const bool smooth) { loadNext(smooth); };
        callbacks.previous = [this](const bool smooth) { loadPrevious(smooth); };
        callbacks.select = [this](const std::filesystem::path& path, const bool smooth) {
            if (catalog_.select(path).has_value()) {
                loadCurrent(smooth);
            }
        };
        callbacks.updateTitle = [this] { updateTitle(); };
        ui_ = std::make_unique<UiController>(window_, glContext_, catalog_, library_, *engine_, overlays_,
                                             config_.generatedPresetDirectory, config_.uiStateFile,
                                             config_.statusOverlay, std::move(callbacks));
        if (!config_.uiEnabled) {
            ui_->toggle();
        }
    }

    fullscreen_ = config_.fullscreen;
    running_ = true;
    fpsWindowStart_ = std::chrono::steady_clock::now();
    printControls();
    updateTitle();
}

void Application::shutdown() {
    running_ = false;
    ui_.reset();
    audio_.reset();
    engine_.reset();
    if (glContext_ != nullptr) {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (SDL_WasInit(0) != 0) {
        SDL_Quit();
    }
}

int Application::run() {
    initialize();
    const auto frameDuration = std::chrono::duration<double>(1.0 / static_cast<double>(config_.fps));

    while (running_) {
        const auto frameStart = std::chrono::steady_clock::now();
        pollEvents();

        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        engine_->render();
        if (ui_ != nullptr) {
            ui_->beginFrame();
            ui_->draw();
            ui_->render();
        }
        SDL_GL_SwapWindow(window_);

        ++fpsFrameCount_;
        const auto now = std::chrono::steady_clock::now();
        const auto fpsElapsed = std::chrono::duration<double>(now - fpsWindowStart_).count();
        if (fpsElapsed >= 1.0) {
            engine_->updateFps(static_cast<int>(std::lround(static_cast<double>(fpsFrameCount_) / fpsElapsed)));
            fpsWindowStart_ = now;
            fpsFrameCount_ = 0;
        }

        const auto elapsed = std::chrono::steady_clock::now() - frameStart;
        if (elapsed < frameDuration) {
            std::this_thread::sleep_for(frameDuration - elapsed);
        }
    }
    return 0;
}

void Application::pollEvents() {
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        if (ui_ != nullptr) {
            ui_->processEvent(event);
        }
        switch (event.type) {
        case SDL_QUIT:
            running_ = false;
            break;
        case SDL_KEYDOWN:
            if (event.key.repeat == 0 && event.key.keysym.sym == SDLK_TAB && ui_ != nullptr &&
                config_.uiEnabled) {
                ui_->toggle();
            } else if (event.key.repeat == 0 && (ui_ == nullptr || !ui_->wantsKeyboard())) {
                handleKey(event.key);
            }
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                event.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
                resize();
            }
            break;
        case SDL_DROPFILE:
            handleDrop(event.drop.file);
            SDL_free(event.drop.file);
            break;
        case SDL_MOUSEWHEEL:
            if (ui_ != nullptr && ui_->wantsMouse()) {
                break;
            }
            if (event.wheel.y > 0) {
                loadNext(true);
            } else if (event.wheel.y < 0) {
                loadPrevious(true);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (ui_ != nullptr && ui_->wantsMouse()) {
                break;
            }
            if (event.button.button == SDL_BUTTON_LEFT && (SDL_GetModState() & KMOD_SHIFT) != 0) {
                int width = 0;
                int height = 0;
                SDL_GL_GetDrawableSize(window_, &width, &height);
                if (width > 0 && height > 0) {
                    const float x = static_cast<float>(event.button.x) / static_cast<float>(width);
                    const float y = 1.0F - static_cast<float>(event.button.y) / static_cast<float>(height);
                    engine_->addTouch(x, y);
                }
            } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                engine_->clearTouches();
            }
            break;
        default:
            break;
        }
    }
}

void Application::handleKey(const SDL_KeyboardEvent& event) {
    const auto key = event.keysym.sym;
    const auto modifiers = static_cast<SDL_Keymod>(event.keysym.mod);
    if (key == SDLK_ESCAPE || (key == SDLK_q && (modifiers & KMOD_CTRL) != 0)) {
        running_ = false;
    } else if (key == SDLK_SPACE || key == SDLK_RIGHT || key == SDLK_n) {
        loadNext(true);
    } else if (key == SDLK_LEFT || key == SDLK_BACKSPACE || key == SDLK_p) {
        loadPrevious(true);
    } else if (key == SDLK_r) {
        catalog_.setShuffle(true);
        loadNext(true);
    } else if (key == SDLK_s) {
        catalog_.setShuffle(!catalog_.shuffle());
        std::cout << "Shuffle: " << (catalog_.shuffle() ? "on" : "off") << '\n';
        updateTitle();
    } else if (key == SDLK_l) {
        std::cout << "Preset lock: " << (engine_->toggleLocked() ? "on" : "off") << '\n';
        updateTitle();
    } else if (key == SDLK_f || (key == SDLK_RETURN && (modifiers & KMOD_ALT) != 0)) {
        toggleFullscreen();
    } else if (key == SDLK_a) {
        try {
            audio_->cycle();
            std::cout << "Capturing audio from: " << audio_->deviceName() << '\n';
            updateTitle();
        } catch (const std::exception& error) {
            std::cerr << "Unable to change audio device: " << error.what() << '\n';
        }
    } else if (key == SDLK_LEFTBRACKET) {
        std::cout << "Beat sensitivity: " << engine_->adjustBeatSensitivity(-0.05F) << '\n';
    } else if (key == SDLK_RIGHTBRACKET) {
        std::cout << "Beat sensitivity: " << engine_->adjustBeatSensitivity(0.05F) << '\n';
    } else if (key == SDLK_F1 || key == SDLK_h) {
        printControls();
    }
}

void Application::handleDrop(const char* path) {
    if (path == nullptr) {
        return;
    }
    const std::filesystem::path dropped(path);
    if (ui_ != nullptr && UiController::isImageFile(dropped)) {
        ui_->addImageOverlay(dropped);
        return;
    }
    const auto added = catalog_.addPath(dropped, true);
    if (added == 0) {
        std::cerr << "No new MilkDrop presets found in: " << path << '\n';
        return;
    }
    std::cout << "Added " << added << " dropped presets from " << path << '\n';
    loadNext(false);
}

void Application::resize() {
    int width = 0;
    int height = 0;
    SDL_GL_GetDrawableSize(window_, &width, &height);
    engine_->resize(static_cast<std::size_t>(width), static_cast<std::size_t>(height));
}

void Application::loadNext(const bool smooth) {
    if (catalog_.next().has_value()) {
        loadCurrent(smooth);
    }
}

void Application::loadPrevious(const bool smooth) {
    if (catalog_.previous().has_value()) {
        loadCurrent(smooth);
    }
}

void Application::loadCurrent(const bool smooth) {
    if (const auto current = catalog_.current(); current.has_value()) {
        engine_->loadPreset(*current, smooth);
        if (const auto error = engine_->lastError(); !error.empty()) {
            overlays_.push(error, OverlaySeverity::Error);
            return;
        }
        library_.recordPlayed(*current);
        try {
            library_.save();
        } catch (const std::exception& error) {
            overlays_.push(error.what(), OverlaySeverity::Warning);
        }
        std::cout << "Preset: " << current->filename().string() << '\n';
        overlays_.push(current->stem().string(), OverlaySeverity::Information,
                       std::chrono::milliseconds(2200));
        updateTitle();
    }
}

void Application::updateTitle() {
    std::string title = "MilkDrop3 Linux Native";
    if (const auto current = catalog_.current(); current.has_value()) {
        title += " — " + current->stem().string();
    }
    if (engine_ != nullptr && engine_->locked()) {
        title += " [LOCKED]";
    }
    if (!catalog_.shuffle()) {
        title += " [ORDERED]";
    }
    if (audio_ != nullptr) {
        title += " — " + audio_->deviceName();
    }
    SDL_SetWindowTitle(window_, title.c_str());
}

void Application::toggleFullscreen() {
    fullscreen_ = !fullscreen_;
    if (SDL_SetWindowFullscreen(window_, fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
        fullscreen_ = !fullscreen_;
        std::cerr << "Unable to change fullscreen mode: " << SDL_GetError() << '\n';
    }
}

void Application::printControls() const {
    std::cout << R"(
Controls
  Space / Right / N   Next preset
  Left / Backspace/P  Previous preset
  R                   Random next preset
  S                   Toggle shuffle
  L                   Lock automatic preset changes
  A                   Cycle audio capture devices
  [ / ]               Adjust beat sensitivity
  F / Alt+Enter       Toggle fullscreen
  Shift+left click    Add a projectM touch waveform
  Middle click        Clear touch waveforms
  Mouse wheel         Change preset
  Drag and drop       Add a .milk file or preset directory
  Drag image          Add a timed PNG/JPEG/WebP overlay
  Tab                 Toggle in-window preset browser
  F1 / H              Print controls
  Escape / Ctrl+Q     Quit

)";
}

} // namespace md3
