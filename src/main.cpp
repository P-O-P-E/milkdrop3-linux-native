#include "Application.hpp"
#include "AudioCapture.hpp"
#include "Config.hpp"

#include <SDL2/SDL.h>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
#ifndef MILKDROP3_VERSION
#define MILKDROP3_VERSION "0.1.0-dev"
#endif
constexpr const char* version = MILKDROP3_VERSION;
}

int main(int argc, char** argv) {
    try {
        auto config = md3::Config::fromCommandLine(argc, argv);
        if (config.showHelp) {
            std::cout << md3::Config::helpText();
            return 0;
        }
        if (config.showVersion) {
            std::cout << "milkdrop3-linux " << version << '\n';
            return 0;
        }
        if (config.listAudioDevices) {
#ifdef SDL_HINT_AUDIO_INCLUDE_MONITORS
            SDL_SetHint(SDL_HINT_AUDIO_INCLUDE_MONITORS, "1");
#endif
            if (SDL_Init(SDL_INIT_AUDIO) != 0) {
                throw std::runtime_error("SDL audio initialization failed: " + std::string(SDL_GetError()));
            }
            std::cout << "Available capture devices:\n";
            for (const auto& device : md3::AudioCapture::devices()) {
                std::cout << "  " << device.index << ": " << device.name << '\n';
            }
            SDL_Quit();
            return 0;
        }

        md3::Application application(std::move(config));
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "milkdrop3-linux: " << error.what() << '\n';
        return 1;
    }
}
