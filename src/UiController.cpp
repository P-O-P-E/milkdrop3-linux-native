#include "UiController.hpp"

#include "GeneratedPresetStore.hpp"
#include "MashupEngine.hpp"
#include "OverlayManager.hpp"
#include "PresetCatalog.hpp"
#include "PresetDocument.hpp"
#include "PresetLibrary.hpp"
#include "ProjectMEngine.hpp"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace md3 {
namespace {

struct ImageOverlay {
    GLuint texture{0};
    int width{0};
    int height{0};
    float x{0.5F};
    float y{0.5F};
    float scale{1.0F};
    float alpha{1.0F};
    std::optional<std::chrono::steady_clock::time_point> expiresAt;
};

ImVec4 severityColor(const OverlaySeverity severity) {
    switch (severity) {
    case OverlaySeverity::Success:
        return {0.45F, 1.0F, 0.55F, 1.0F};
    case OverlaySeverity::Warning:
        return {1.0F, 0.8F, 0.3F, 1.0F};
    case OverlaySeverity::Error:
        return {1.0F, 0.38F, 0.38F, 1.0F};
    case OverlaySeverity::Information:
    default:
        return {0.85F, 0.9F, 1.0F, 1.0F};
    }
}

std::string partName(const PresetPart part) {
    switch (part) {
    case PresetPart::Waves:
        return "waves";
    case PresetPart::Shapes:
        return "shapes";
    default:
        return "part";
    }
}

} // namespace

struct UiController::Impl {
    Impl(SDL_Window* windowValue, SDL_GLContext contextValue, PresetCatalog& catalogValue,
         PresetLibrary& libraryValue, ProjectMEngine& engineValue, OverlayManager& overlaysValue,
         std::filesystem::path generatedDirectoryValue, std::filesystem::path stateFileValue,
         const bool statusOverlayValue, UiCallbacks callbacksValue)
        : window(windowValue), context(contextValue), catalog(catalogValue), library(libraryValue),
          engine(engineValue), overlays(overlaysValue), generatedDirectory(std::move(generatedDirectoryValue)),
          generatedStore(generatedDirectory), stateFile(std::move(stateFileValue)),
          statusOverlay(statusOverlayValue), callbacks(std::move(callbacksValue)) {
        if (window == nullptr || context == nullptr) {
            throw std::runtime_error("UI requires an active SDL/OpenGL window");
        }
        std::filesystem::create_directories(generatedDirectory);
        if (const auto parent = stateFile.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        stateFileString = stateFile.string();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = stateFileString.c_str();
        ImGui::StyleColorsDark();
        ImGui::GetStyle().WindowRounding = 5.0F;
        ImGui::GetStyle().FrameRounding = 3.0F;
        if (!ImGui_ImplSDL2_InitForOpenGL(window, context)) {
            ImGui::DestroyContext();
            throw std::runtime_error("Unable to initialize the ImGui SDL backend");
        }
        if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();
            throw std::runtime_error("Unable to initialize the ImGui OpenGL backend");
        }
        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
        playlistPath = (generatedDirectory / "playlist.m3u").string();
    }

    ~Impl() {
        for (const auto& sprite : sprites) {
            if (sprite.texture != 0) {
                glDeleteTextures(1, &sprite.texture);
            }
        }
        IMG_Quit();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void saveLibrary() {
        try {
            library.save();
        } catch (const std::exception& error) {
            overlays.push(error.what(), OverlaySeverity::Error);
        }
    }

    void loadEditor(const std::filesystem::path& path) {
        try {
            const auto document = PresetDocument::load(path);
            editorSource = document.serialize();
            editorPath = path;
            editorDiagnostics = document.diagnostics();
            editorStatus = "Loaded " + path.filename().string();
        } catch (const std::exception& error) {
            editorStatus = error.what();
            overlays.push(editorStatus, OverlaySeverity::Error);
        }
    }

    std::optional<PresetDocument> parseEditor() {
        try {
            auto document = PresetDocument::parse(editorSource);
            editorDiagnostics = document.diagnostics();
            if (!document.valid()) {
                editorStatus = "Preset contains syntax errors";
                return std::nullopt;
            }
            editorStatus = editorDiagnostics.empty() ? "Preset syntax is valid" : "Valid with warnings";
            return document;
        } catch (const std::exception& error) {
            editorDiagnostics.clear();
            editorStatus = error.what();
            return std::nullopt;
        }
    }

    void previewEditor() {
        if (const auto document = parseEditor(); document.has_value()) {
            engine.loadPresetData(document->serialize(), false);
            if (const auto error = engine.lastError(); !error.empty()) {
                editorStatus = error;
                overlays.push(error, OverlaySeverity::Error);
            } else {
                editorStatus = "Preview compiled and loaded";
                overlays.push(editorStatus, OverlaySeverity::Success);
            }
        }
    }

    void saveEditorCopy(const std::string& suffix = "-edited") {
        const auto document = parseEditor();
        if (!document.has_value()) {
            return;
        }
        const auto name = editorPath.empty() ? "preset" : editorPath.stem().string();
        const auto output = MashupEngine::uniqueOutputPath(generatedDirectory, name, suffix);
        try {
            document->save(output);
            catalog.addPath(output, false);
            editorPath = output;
            editorStatus = "Saved " + output.string();
            overlays.push(editorStatus, OverlaySeverity::Success);
            if (callbacks.select) {
                callbacks.select(output, false);
            }
        } catch (const std::exception& error) {
            editorStatus = error.what();
            overlays.push(editorStatus, OverlaySeverity::Error);
        }
    }

    void generateMashup() {
        const auto base = parseEditor();
        if (!base.has_value()) {
            return;
        }
        if (donorPath.empty()) {
            editorStatus = "Choose a donor preset in the browser or enter its path";
            return;
        }
        try {
            const auto donor = PresetDocument::load(donorPath);
            editorSource = MashupEngine::combine(*base, donor, mashupSelection).serialize();
            editorDiagnostics = PresetDocument::parse(editorSource).diagnostics();
            editorStatus = "Mashup generated in memory; preview or save a copy";
            overlays.push(editorStatus, OverlaySeverity::Success);
        } catch (const std::exception& error) {
            editorStatus = error.what();
            overlays.push(editorStatus, OverlaySeverity::Error);
        }
    }

    void exportFragment(const PresetPart part) {
        const auto document = parseEditor();
        if (!document.has_value()) {
            return;
        }
        const auto name = editorPath.empty() ? "preset" : editorPath.stem().string();
        const auto stem = MashupEngine::safeName(name) + '-' + partName(part);
        auto output = generatedDirectory / (stem + ".milkpart");
        for (int number = 2; std::filesystem::exists(output); ++number) {
            output = generatedDirectory / (stem + '-' + std::to_string(number) + ".milkpart");
        }
        try {
            document->extractPart(part).save(output);
            overlays.push("Exported " + output.string(), OverlaySeverity::Success);
        } catch (const std::exception& error) {
            overlays.push(error.what(), OverlaySeverity::Error);
        }
    }

    void replaceAll() {
        if (findText.empty()) {
            editorStatus = "Find text cannot be empty";
            return;
        }
        std::size_t replacements = 0;
        std::size_t position = 0;
        while ((position = editorSource.find(findText, position)) != std::string::npos) {
            editorSource.replace(position, findText.size(), replaceText);
            position += replaceText.size();
            ++replacements;
        }
        editorStatus = "Replaced " + std::to_string(replacements) + " occurrence(s)";
    }

    void drawStatusAndMessages() {
        if (!statusOverlay && overlays.active().empty()) {
            return;
        }
        const auto messages = overlays.active();
        ImGui::SetNextWindowPos({12.0F, 12.0F}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.42F);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                           ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("##status-overlay", nullptr, flags)) {
            if (statusOverlay) {
                if (const auto current = catalog.current(); current.has_value()) {
                    ImGui::TextUnformatted(current->stem().string().c_str());
                    const auto metadata = library.metadata(*current);
                    ImGui::TextDisabled("Rating %d/5%s  |  Plays %llu%s", metadata.rating,
                                        metadata.favorite ? "  [favorite]" : "",
                                        static_cast<unsigned long long>(metadata.playCount),
                                        engine.locked() ? "  |  LOCKED" : "");
                } else {
                    ImGui::TextUnformatted("No preset loaded");
                }
            }
            for (const auto& message : messages) {
                ImGui::TextColored(severityColor(message.severity), "%s", message.text.c_str());
            }
        }
        ImGui::End();
    }

    void drawSprites() {
        const auto now = std::chrono::steady_clock::now();
        std::erase_if(sprites, [now](const ImageOverlay& sprite) {
            if (!sprite.expiresAt.has_value() || *sprite.expiresAt > now) {
                return false;
            }
            if (sprite.texture != 0) {
                glDeleteTextures(1, &sprite.texture);
            }
            return true;
        });
        const auto display = ImGui::GetIO().DisplaySize;
        auto* drawList = ImGui::GetBackgroundDrawList();
        for (const auto& sprite : sprites) {
            const float width = static_cast<float>(sprite.width) * sprite.scale;
            const float height = static_cast<float>(sprite.height) * sprite.scale;
            const ImVec2 center{display.x * sprite.x, display.y * sprite.y};
            const ImVec2 minimum{center.x - width * 0.5F, center.y - height * 0.5F};
            const ImVec2 maximum{center.x + width * 0.5F, center.y + height * 0.5F};
            const auto tint = IM_COL32(255, 255, 255, static_cast<int>(std::clamp(sprite.alpha, 0.0F, 1.0F) * 255.0F));
            drawList->AddImage((ImTextureID)(intptr_t)sprite.texture, minimum, maximum, {0.0F, 0.0F}, {1.0F, 1.0F}, tint);
        }
    }

    void drawBrowser() {
        ImGui::SetNextWindowSize({510.0F, 620.0F}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Preset Library", &showBrowser)) {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Previous") && callbacks.previous) {
            callbacks.previous(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Next") && callbacks.next) {
            callbacks.next(true);
        }
        ImGui::SameLine();
        bool shuffle = catalog.shuffle();
        if (ImGui::Checkbox("Shuffle", &shuffle)) {
            catalog.setShuffle(shuffle);
            if (callbacks.updateTitle) {
                callbacks.updateTitle();
            }
        }
        ImGui::SameLine();
        bool locked = engine.locked();
        if (ImGui::Checkbox("Locked", &locked)) {
            engine.setLocked(locked);
            if (callbacks.updateTitle) {
                callbacks.updateTitle();
            }
        }

        if (const auto current = catalog.current(); current.has_value()) {
            auto metadata = library.metadata(*current);
            ImGui::SeparatorText(current->filename().string().c_str());
            int rating = metadata.rating;
            if (ImGui::SliderInt("Rating", &rating, 0, 5)) {
                library.setRating(*current, rating);
                saveLibrary();
            }
            bool favorite = metadata.favorite;
            if (ImGui::Checkbox("Favorite", &favorite)) {
                library.toggleFavorite(*current);
                saveLibrary();
            }
            ImGui::SameLine();
            if (ImGui::Button("Edit current")) {
                loadEditor(*current);
                showEditor = true;
            }
            if (generatedStore.owns(*current)) {
                if (renameSource != *current) {
                    renameSource = *current;
                    renameName = current->stem().string();
                }
                ImGui::InputText("Generated preset name", &renameName);
                if (ImGui::Button("Rename generated copy")) {
                    try {
                        const auto previous = *current;
                        const auto renamed = generatedStore.renamePreset(previous, renameName);
                        catalog.remove(previous);
                        catalog.addPath(renamed, false);
                        if (editorPath == previous) {
                            editorPath = renamed;
                        }
                        if (callbacks.select) {
                            callbacks.select(renamed, false);
                        }
                        overlays.push("Renamed to " + renamed.filename().string(), OverlaySeverity::Success);
                    } catch (const std::exception& error) {
                        overlays.push(error.what(), OverlaySeverity::Error);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Move to trash")) {
                    ImGui::OpenPopup("Move generated preset to trash?");
                }
                if (ImGui::BeginPopupModal("Move generated preset to trash?", nullptr,
                                           ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped("Move %s to the recoverable .trash directory?",
                                       current->filename().string().c_str());
                    if (ImGui::Button("Move to trash")) {
                        try {
                            const auto removed = *current;
                            const auto trashPath = generatedStore.moveToTrash(removed);
                            catalog.remove(removed);
                            if (editorPath == removed) {
                                editorPath.clear();
                            }
                            overlays.push("Moved to " + trashPath.string(), OverlaySeverity::Success);
                            if (const auto replacement = catalog.current(); replacement.has_value() && callbacks.select) {
                                callbacks.select(*replacement, false);
                            } else {
                                engine.loadIdle();
                                if (callbacks.updateTitle) {
                                    callbacks.updateTitle();
                                }
                            }
                        } catch (const std::exception& error) {
                            overlays.push(error.what(), OverlaySeverity::Error);
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
        }

        filter.Draw("Search presets");
        ImGui::BeginChild("preset-list", {0.0F, 300.0F}, ImGuiChildFlags_Borders);
        for (const auto& preset : catalog.presets()) {
            const auto label = preset.stem().string();
            if (!filter.PassFilter(label.c_str())) {
                continue;
            }
            const bool selected = catalog.current().has_value() && *catalog.current() == preset;
            if (ImGui::Selectable(label.c_str(), selected) && callbacks.select) {
                callbacks.select(preset, true);
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Use as mashup donor")) {
                    donorPath = preset.string();
                    showEditor = true;
                }
                if (ImGui::MenuItem("Load in editor")) {
                    loadEditor(preset);
                    showEditor = true;
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();

        ImGui::SeparatorText("Playlist");
        ImGui::InputText("M3U path", &playlistPath);
        if (ImGui::Button("Import")) {
            try {
                const auto count = catalog.importPlaylist(playlistPath);
                overlays.push("Imported " + std::to_string(count) + " new preset(s)", OverlaySeverity::Success);
            } catch (const std::exception& error) {
                overlays.push(error.what(), OverlaySeverity::Error);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Export all")) {
            try {
                catalog.exportPlaylist(playlistPath);
                overlays.push("Exported playlist", OverlaySeverity::Success);
            } catch (const std::exception& error) {
                overlays.push(error.what(), OverlaySeverity::Error);
            }
        }
        ImGui::End();
    }

    void drawEditor() {
        ImGui::SetNextWindowSize({760.0F, 720.0F}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Preset Authoring", &showEditor)) {
            ImGui::End();
            return;
        }
        ImGui::TextWrapped("Source: %s", editorPath.empty() ? "<in-memory>" : editorPath.string().c_str());
        if (ImGui::Button("Load current")) {
            if (const auto current = catalog.current(); current.has_value()) {
                loadEditor(*current);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Validate")) {
            parseEditor();
        }
        ImGui::SameLine();
        if (ImGui::Button("Preview")) {
            previewEditor();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save copy")) {
            saveEditorCopy();
        }

        ImGui::InputText("Find", &findText);
        ImGui::SameLine();
        ImGui::InputText("Replace", &replaceText);
        ImGui::SameLine();
        if (ImGui::Button("Replace all")) {
            replaceAll();
        }

        ImGui::InputTextMultiline("##preset-source", &editorSource, {-1.0F, 310.0F},
                                  ImGuiInputTextFlags_AllowTabInput);
        if (!editorStatus.empty()) {
            ImGui::TextWrapped("%s", editorStatus.c_str());
        }
        for (const auto& diagnostic : editorDiagnostics) {
            const auto color = diagnostic.severity == DiagnosticSeverity::Error
                                   ? ImVec4{1.0F, 0.35F, 0.35F, 1.0F}
                                   : ImVec4{1.0F, 0.8F, 0.3F, 1.0F};
            ImGui::TextColored(color, "Line %zu: %s", diagnostic.line, diagnostic.message.c_str());
        }

        if (ImGui::CollapsingHeader("Mashup", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("Donor preset/fragment", &donorPath);
            ImGui::Checkbox("General/post-processing", &mashupSelection.general);
            ImGui::SameLine();
            ImGui::Checkbox("Motion/equations", &mashupSelection.motion);
            ImGui::Checkbox("Waves", &mashupSelection.waves);
            ImGui::SameLine();
            ImGui::Checkbox("Shapes", &mashupSelection.shapes);
            ImGui::Checkbox("Warp shader", &mashupSelection.warpShader);
            ImGui::SameLine();
            ImGui::Checkbox("Composite shader", &mashupSelection.compositeShader);
            if (ImGui::Button("Generate mashup")) {
                generateMashup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save mashup copy")) {
                saveEditorCopy("-mashup");
            }
            ImGui::SameLine();
            if (ImGui::Button("Export waves")) {
                exportFragment(PresetPart::Waves);
            }
            ImGui::SameLine();
            if (ImGui::Button("Export shapes")) {
                exportFragment(PresetPart::Shapes);
            }
        }

        if (ImGui::CollapsingHeader("Image overlays")) {
            ImGui::InputText("Image path", &spritePath);
            ImGui::SliderFloat("X", &spriteX, 0.0F, 1.0F);
            ImGui::SliderFloat("Y", &spriteY, 0.0F, 1.0F);
            ImGui::SliderFloat("Scale", &spriteScale, 0.05F, 4.0F);
            ImGui::SliderFloat("Alpha", &spriteAlpha, 0.0F, 1.0F);
            ImGui::SliderFloat("Duration (0 = persistent)", &spriteDuration, 0.0F, 60.0F);
            if (ImGui::Button("Add image")) {
                addImage(spritePath);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear images")) {
                for (const auto& sprite : sprites) {
                    if (sprite.texture != 0) {
                        glDeleteTextures(1, &sprite.texture);
                    }
                }
                sprites.clear();
            }
        }
        ImGui::End();
    }

    bool addImage(const std::filesystem::path& path) {
        SDL_Surface* loaded = IMG_Load(path.string().c_str());
        if (loaded == nullptr) {
            overlays.push("Unable to load image: " + std::string(IMG_GetError()), OverlaySeverity::Error);
            return false;
        }
        SDL_Surface* surface = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(loaded);
        if (surface == nullptr) {
            overlays.push("Unable to convert image: " + std::string(SDL_GetError()), OverlaySeverity::Error);
            return false;
        }

        ImageOverlay sprite;
        sprite.width = surface->w;
        sprite.height = surface->h;
        sprite.x = spriteX;
        sprite.y = spriteY;
        sprite.scale = spriteScale;
        sprite.alpha = spriteAlpha;
        if (spriteDuration > 0.0F) {
            sprite.expiresAt = std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(static_cast<int>(spriteDuration * 1000.0F));
        }
        glGenTextures(1, &sprite.texture);
        glBindTexture(GL_TEXTURE_2D, sprite.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, surface->pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
        SDL_FreeSurface(surface);
        sprites.push_back(sprite);
        overlays.push("Added image overlay: " + path.filename().string(), OverlaySeverity::Success);
        return true;
    }

    SDL_Window* window{};
    SDL_GLContext context{};
    PresetCatalog& catalog;
    PresetLibrary& library;
    ProjectMEngine& engine;
    OverlayManager& overlays;
    std::filesystem::path generatedDirectory;
    GeneratedPresetStore generatedStore;
    std::filesystem::path stateFile;
    bool statusOverlay{true};
    UiCallbacks callbacks;
    std::string stateFileString;

    bool showBrowser{true};
    bool showEditor{false};
    ImGuiTextFilter filter;
    std::string playlistPath;
    std::filesystem::path renameSource;
    std::string renameName;

    std::string editorSource;
    std::filesystem::path editorPath;
    std::vector<PresetDiagnostic> editorDiagnostics;
    std::string editorStatus;
    std::string findText;
    std::string replaceText;
    std::string donorPath;
    MashupSelection mashupSelection;

    std::string spritePath;
    float spriteX{0.5F};
    float spriteY{0.5F};
    float spriteScale{1.0F};
    float spriteAlpha{1.0F};
    float spriteDuration{8.0F};
    std::vector<ImageOverlay> sprites;
};

UiController::UiController(SDL_Window* window, SDL_GLContext context, PresetCatalog& catalog,
                           PresetLibrary& library, ProjectMEngine& engine, OverlayManager& overlays,
                           std::filesystem::path generatedDirectory, std::filesystem::path stateFile,
                           const bool statusOverlay, UiCallbacks callbacks)
    : impl_(std::make_unique<Impl>(window, context, catalog, library, engine, overlays,
                                   std::move(generatedDirectory), std::move(stateFile), statusOverlay,
                                   std::move(callbacks))) {}

UiController::~UiController() = default;

void UiController::processEvent(const SDL_Event& event) { ImGui_ImplSDL2_ProcessEvent(&event); }

void UiController::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void UiController::draw() {
    impl_->drawSprites();
    impl_->drawStatusAndMessages();
    if (impl_->showBrowser) {
        impl_->drawBrowser();
    }
    if (impl_->showEditor) {
        impl_->drawEditor();
    }
}

void UiController::render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UiController::toggle() { impl_->showBrowser = !impl_->showBrowser; }
bool UiController::visible() const { return impl_->showBrowser || impl_->showEditor; }
bool UiController::wantsKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }
bool UiController::wantsMouse() const { return ImGui::GetIO().WantCaptureMouse; }

bool UiController::addImageOverlay(const std::filesystem::path& path) {
    impl_->spritePath = path.string();
    return impl_->addImage(path);
}

bool UiController::isImageFile(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
           extension == ".webp";
}

} // namespace md3
