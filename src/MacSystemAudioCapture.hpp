#pragma once

#include <functional>
#include <memory>
#include <string>

namespace md3 {

class MacSystemAudioCapture {
public:
    using SampleCallback = std::function<void(const float*, unsigned int)>;
    using StatusCallback = std::function<void(const std::string&, bool)>;

    MacSystemAudioCapture(SampleCallback samples, StatusCallback status);
    ~MacSystemAudioCapture();

    MacSystemAudioCapture(const MacSystemAudioCapture&) = delete;
    MacSystemAudioCapture& operator=(const MacSystemAudioCapture&) = delete;

    void start();
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace md3
