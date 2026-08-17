#include "AudioCapture.hpp"

#include "ProjectMEngine.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <stdexcept>
#include <string_view>

namespace md3 {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

AudioCapture::AudioCapture(ProjectMEngine& engine) : engine_(engine) {}
AudioCapture::~AudioCapture() { stop(); }

std::vector<AudioDeviceInfo> AudioCapture::devices() {
    std::vector<AudioDeviceInfo> result{{-1, "Default capture device"}};
    const int count = SDL_GetNumAudioDevices(SDL_TRUE);
    for (int index = 0; index < count; ++index) {
        const char* name = SDL_GetAudioDeviceName(index, SDL_TRUE);
        result.push_back({index, name != nullptr ? name : "Unknown device"});
    }
    return result;
}

int AudioCapture::resolveDevice(const std::string& selector) {
    if (selector.empty() || lower(selector) == "default") {
        return -1;
    }

    int numeric = 0;
    const auto* begin = selector.data();
    const auto* end = selector.data() + selector.size();
    const auto [pointer, error] = std::from_chars(begin, end, numeric);
    if (error == std::errc{} && pointer == end) {
        if (numeric < -1 || numeric >= SDL_GetNumAudioDevices(SDL_TRUE)) {
            throw std::runtime_error("Audio device index is out of range: " + selector);
        }
        return numeric;
    }

    const auto needle = lower(selector);
    for (const auto& device : devices()) {
        if (device.index >= 0 && lower(device.name).find(needle) != std::string::npos) {
            return device.index;
        }
    }
    throw std::runtime_error("No capture device matches: " + selector);
}

void AudioCapture::start(const std::string& selector) { open(resolveDevice(selector)); }

void AudioCapture::open(const int index) {
    stop();

    SDL_AudioSpec requested{};
    SDL_AudioSpec actual{};
    requested.freq = 48000;
    requested.format = AUDIO_F32SYS;
    requested.channels = 2;
    requested.samples = 1024;
    requested.callback = &AudioCapture::callback;
    requested.userdata = this;

    const char* selectedName = index >= 0 ? SDL_GetAudioDeviceName(index, SDL_TRUE) : nullptr;
    deviceId_ = SDL_OpenAudioDevice(selectedName, SDL_TRUE, &requested, &actual,
                                    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (deviceId_ == 0) {
        throw std::runtime_error("Unable to open audio capture device: " + std::string(SDL_GetError()));
    }
    if (actual.format != AUDIO_F32SYS || actual.channels != 2) {
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
        throw std::runtime_error("Audio backend did not provide stereo floating-point samples");
    }

    deviceIndex_ = index;
    deviceName_ = selectedName != nullptr ? selectedName : "Default capture device";
    SDL_PauseAudioDevice(deviceId_, SDL_FALSE);
}

void AudioCapture::stop() {
    if (deviceId_ != 0) {
        SDL_PauseAudioDevice(deviceId_, SDL_TRUE);
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
    }
}

void AudioCapture::cycle() {
    const int count = SDL_GetNumAudioDevices(SDL_TRUE);
    const int next = ((deviceIndex_ + 2) % (count + 1)) - 1;
    open(next);
}

std::string AudioCapture::deviceName() const { return deviceName_; }
int AudioCapture::deviceIndex() const noexcept { return deviceIndex_; }

void AudioCapture::callback(void* userdata, Uint8* stream, const int length) {
    auto* capture = static_cast<AudioCapture*>(userdata);
    if (capture == nullptr || stream == nullptr || length <= 0) {
        return;
    }
    const auto bytesPerFrame = static_cast<int>(sizeof(float) * 2U);
    const auto frames = static_cast<unsigned int>(length / bytesPerFrame);
    capture->engine_.addAudio(reinterpret_cast<const float*>(stream), frames);
}

} // namespace md3

