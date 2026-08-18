#include "AudioCapture.hpp"

#include "ProjectMEngine.hpp"
#ifdef MILKDROP3_MACOS_ARM64
#include "MacSystemAudioCapture.hpp"
#endif

#include <algorithm>
#include <charconv>
#include <cctype>
#include <iostream>
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
#ifdef MILKDROP3_MACOS_ARM64
    result.push_back({-2, "System audio (native macOS mix)"});
#endif
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
        const auto available = devices();
        if (std::none_of(available.begin(), available.end(), [numeric](const AudioDeviceInfo& device) {
                return device.index == numeric;
            })) {
            throw std::runtime_error("Audio device index is out of range: " + selector);
        }
        return numeric;
    }

    const auto needle = lower(selector);
    for (const auto& device : devices()) {
        if (lower(device.name).find(needle) != std::string::npos) {
            return device.index;
        }
    }
    throw std::runtime_error("No capture device matches: " + selector);
}

void AudioCapture::start(const std::string& selector) { open(resolveDevice(selector)); }
void AudioCapture::select(const int index) { open(index); }

void AudioCapture::open(const int index) {
    stop();

#ifdef MILKDROP3_MACOS_ARM64
    if (index == -2) {
        systemAudio_ = std::make_unique<MacSystemAudioCapture>(
            [this](const float* samples, const unsigned int frames) {
                engine_.addAudio(samples, frames);
            },
            [](const std::string& message, const bool isError) {
                (isError ? std::cerr : std::cout) << message << '\n';
            });
        systemAudio_->start();
        deviceIndex_ = index;
        deviceName_ = "System audio (native macOS mix)";
        return;
    }
#endif

    const auto available = devices();
    if (std::none_of(available.begin(), available.end(), [index](const AudioDeviceInfo& device) {
            return device.index == index;
        })) {
        throw std::runtime_error("Audio device index is out of range: " + std::to_string(index));
    }

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
#ifdef MILKDROP3_MACOS_ARM64
    systemAudio_.reset();
#endif
    if (deviceId_ != 0) {
        SDL_PauseAudioDevice(deviceId_, SDL_TRUE);
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
    }
}

void AudioCapture::cycle() {
    const auto available = devices();
    const auto current = std::find_if(available.begin(), available.end(), [this](const AudioDeviceInfo& device) {
        return device.index == deviceIndex_;
    });
    const auto position = current == available.end()
                              ? std::size_t{0}
                              : static_cast<std::size_t>(std::distance(available.begin(), current) + 1) %
                                    available.size();
    open(available[position].index);
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
