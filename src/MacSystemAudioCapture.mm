#include "MacSystemAudioCapture.hpp"

#import <CoreMedia/CoreMedia.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

@interface MD3SystemAudioOutput : NSObject <SCStreamOutput, SCStreamDelegate> {
@public
    std::function<void(CMSampleBufferRef)> samples_;
    std::function<void(NSError*)> stopped_;
}
@end

namespace md3 {
namespace {

std::string errorText(NSError* error) {
    if (error == nil) {
        return "Unknown ScreenCaptureKit error";
    }
    const char* text = error.localizedDescription.UTF8String;
    return text != nullptr ? text : "Unknown ScreenCaptureKit error";
}

void deliverAudio(CMSampleBufferRef sampleBuffer, const MacSystemAudioCapture::SampleCallback& callback) {
    if (sampleBuffer == nullptr || !CMSampleBufferIsValid(sampleBuffer) ||
        !CMSampleBufferDataIsReady(sampleBuffer)) {
        return;
    }

    const auto format = CMSampleBufferGetFormatDescription(sampleBuffer);
    const auto* description = format != nullptr
                                  ? CMAudioFormatDescriptionGetStreamBasicDescription(format)
                                  : nullptr;
    if (description == nullptr || description->mFormatID != kAudioFormatLinearPCM ||
        (description->mFormatFlags & kAudioFormatFlagIsFloat) == 0 ||
        description->mBitsPerChannel != 32) {
        return;
    }

    const auto sampleCount = CMSampleBufferGetNumSamples(sampleBuffer);
    if (sampleCount <= 0) {
        return;
    }

    std::size_t listSize = 0;
    auto status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer, &listSize, nullptr, 0, nullptr, nullptr,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, nullptr);
    if (status != noErr || listSize < sizeof(AudioBufferList)) {
        return;
    }

    auto* storage = static_cast<AudioBufferList*>(std::malloc(listSize));
    if (storage == nullptr) {
        return;
    }
    const std::unique_ptr<AudioBufferList, decltype(&std::free)> buffers(storage, &std::free);
    CMBlockBufferRef blockBuffer = nullptr;
    status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer, nullptr, buffers.get(), listSize, nullptr, nullptr,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &blockBuffer);
    if (status != noErr) {
        return;
    }
    const std::unique_ptr<std::remove_pointer_t<CMBlockBufferRef>, decltype(&CFRelease)> retainedBlock(
        blockBuffer, &CFRelease);

    const auto frames = static_cast<std::size_t>(sampleCount);
    if (frames > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max()) ||
        buffers->mNumberBuffers == 0) {
        return;
    }

    const auto channels = std::max<std::size_t>(1, description->mChannelsPerFrame);
    std::vector<float> stereo(frames * 2U);
    const bool nonInterleaved = (description->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
    if (nonInterleaved) {
        const auto& leftBuffer = buffers->mBuffers[0];
        const auto& rightBuffer = buffers->mBuffers[std::min<UInt32>(1, buffers->mNumberBuffers - 1)];
        const auto* left = static_cast<const float*>(leftBuffer.mData);
        const auto* right = static_cast<const float*>(rightBuffer.mData);
        if (left == nullptr || right == nullptr ||
            leftBuffer.mDataByteSize < frames * sizeof(float) ||
            rightBuffer.mDataByteSize < frames * sizeof(float)) {
            return;
        }
        for (std::size_t frame = 0; frame < frames; ++frame) {
            stereo[frame * 2U] = left[frame];
            stereo[frame * 2U + 1U] = right[frame];
        }
    } else {
        const auto& interleavedBuffer = buffers->mBuffers[0];
        const auto* input = static_cast<const float*>(interleavedBuffer.mData);
        if (input == nullptr ||
            interleavedBuffer.mDataByteSize < frames * channels * sizeof(float)) {
            return;
        }
        for (std::size_t frame = 0; frame < frames; ++frame) {
            stereo[frame * 2U] = input[frame * channels];
            stereo[frame * 2U + 1U] = channels > 1 ? input[frame * channels + 1U]
                                                       : input[frame * channels];
        }
    }

    callback(stereo.data(), static_cast<unsigned int>(frames));
}

struct CaptureState {
    MacSystemAudioCapture::SampleCallback samples;
    MacSystemAudioCapture::StatusCallback status;
    std::mutex mutex;
    bool stopped{false};
    SCStream* __strong stream{nil};
    MD3SystemAudioOutput* __strong output{nil};
    dispatch_queue_t queue{nullptr};
};

bool isStopped(const std::shared_ptr<CaptureState>& state) {
    const std::scoped_lock lock(state->mutex);
    return state->stopped;
}

void report(const std::shared_ptr<CaptureState>& state, std::string message, const bool isError) {
    if (isStopped(state)) {
        return;
    }
    state->status(message, isError);
}

} // namespace

struct MacSystemAudioCapture::Impl {
    Impl(SampleCallback samples, StatusCallback status) {
        state = std::make_shared<CaptureState>();
        state->samples = std::move(samples);
        state->status = std::move(status);
        state->queue = dispatch_queue_create("io.github.p-o-p-e.milkdrop3.system-audio",
                                             DISPATCH_QUEUE_SERIAL);
    }

    void start() {
        const auto active = state;
        std::weak_ptr<CaptureState> weakState(active);
        [SCShareableContent
            getShareableContentExcludingDesktopWindows:YES
                                   onScreenWindowsOnly:YES
                                     completionHandler:^(SCShareableContent* content, NSError* contentError) {
            if (isStopped(active)) {
                return;
            }
            if (contentError != nil) {
                report(active, "System audio permission/capture failed: " + errorText(contentError), true);
                return;
            }
            SCDisplay* display = content.displays.firstObject;
            if (display == nil) {
                report(active, "System audio capture requires an active display", true);
                return;
            }

            SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
            SCStreamConfiguration* configuration = [[SCStreamConfiguration alloc] init];
            configuration.width = 2;
            configuration.height = 2;
            configuration.minimumFrameInterval = CMTimeMake(1, 1);
            configuration.queueDepth = 3;
            configuration.showsCursor = NO;
            configuration.capturesAudio = YES;
            configuration.excludesCurrentProcessAudio = YES;
            configuration.sampleRate = 48000;
            configuration.channelCount = 2;

            MD3SystemAudioOutput* output = [[MD3SystemAudioOutput alloc] init];
            output->samples_ = [weakState](CMSampleBufferRef sampleBuffer) {
                const auto locked = weakState.lock();
                if (locked == nullptr || isStopped(locked)) {
                    return;
                }
                deliverAudio(sampleBuffer, locked->samples);
            };
            output->stopped_ = [weakState](NSError* streamError) {
                const auto locked = weakState.lock();
                if (locked != nullptr) {
                    report(locked, "System audio capture stopped: " + errorText(streamError), true);
                }
            };

            SCStream* stream = [[SCStream alloc] initWithFilter:filter
                                                  configuration:configuration
                                                       delegate:output];
            NSError* outputError = nil;
            if (![stream addStreamOutput:output
                                    type:SCStreamOutputTypeAudio
                      sampleHandlerQueue:active->queue
                                   error:&outputError]) {
                report(active, "Unable to attach the system audio stream: " + errorText(outputError), true);
                return;
            }

            {
                const std::scoped_lock lock(active->mutex);
                if (active->stopped) {
                    return;
                }
                active->stream = stream;
                active->output = output;
            }

            dispatch_async(active->queue, ^{
                if (isStopped(active)) {
                    return;
                }
                [stream startCaptureWithCompletionHandler:^(NSError* startError) {
                    if (startError != nil) {
                        report(active, "Unable to start system audio capture: " + errorText(startError), true);
                    } else {
                        report(active, "Capturing the native macOS system audio mix", false);
                    }
                }];
            });
        }];
    }

    void stop() {
        SCStream* stream = nil;
        dispatch_queue_t queue = nullptr;
        {
            const std::scoped_lock lock(state->mutex);
            if (state->stopped) {
                return;
            }
            state->stopped = true;
            stream = state->stream;
            state->stream = nil;
            state->output = nil;
            queue = state->queue;
        }
        if (stream != nil) {
            [stream stopCaptureWithCompletionHandler:nil];
        }
        if (queue != nullptr) {
            dispatch_sync(queue, ^{});
        }
    }

    std::shared_ptr<CaptureState> state;
};

MacSystemAudioCapture::MacSystemAudioCapture(SampleCallback samples, StatusCallback status)
    : impl_(std::make_unique<Impl>(std::move(samples), std::move(status))) {}

MacSystemAudioCapture::~MacSystemAudioCapture() { stop(); }

void MacSystemAudioCapture::start() { impl_->start(); }
void MacSystemAudioCapture::stop() { impl_->stop(); }

} // namespace md3

@implementation MD3SystemAudioOutput

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
    (void)stream;
    if (type == SCStreamOutputTypeAudio && samples_) {
        samples_(sampleBuffer);
    }
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
    (void)stream;
    if (stopped_) {
        stopped_(error);
    }
}

@end
