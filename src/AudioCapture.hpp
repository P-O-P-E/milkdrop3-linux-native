#pragma once

#include <SDL2/SDL.h>

#include <string>
#include <vector>

namespace md3 {

class ProjectMEngine;

struct AudioDeviceInfo {
    int index{-1};
    std::string name;
};

class AudioCapture {
public:
    explicit AudioCapture(ProjectMEngine& engine);
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    void start(const std::string& selector = {});
    void stop();
    void cycle();

    [[nodiscard]] std::string deviceName() const;
    [[nodiscard]] int deviceIndex() const noexcept;

    static std::vector<AudioDeviceInfo> devices();

private:
    static void callback(void* userdata, Uint8* stream, int length);
    static int resolveDevice(const std::string& selector);
    void open(int index);

    ProjectMEngine& engine_;
    SDL_AudioDeviceID deviceId_{0};
    int deviceIndex_{-1};
    std::string deviceName_{"Default capture device"};
};

} // namespace md3

