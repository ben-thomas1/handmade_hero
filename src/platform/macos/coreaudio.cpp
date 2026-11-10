#include "platform/macos/coreaudio.hpp"

#include "common/config.hpp"

#include <AudioToolbox/AudioToolbox.h>

#include <cassert>
#include <cstring>
#include <print>


static OSStatus
on_render(void *data, AudioUnitRenderActionFlags *, const AudioTimeStamp *, UInt32, UInt32 frame_count,
          AudioBufferList *buffers)
{
    Audio *audio = (Audio *)data;
    AudioBuffer *samples = &buffers->mBuffers[0];

    u32 stride = Config::AUDIO_CHANNELS * sizeof(f32);
    u32 frames = samples->mDataByteSize / stride;
    if (frame_count < frames) frames = frame_count;
    audio->lock();
    u32 copied = audio->read(audio->data, samples->mData, frames);
    if (copied > frames) copied = frames;
    audio->underrun_frames += frames - copied;
    audio->unlock();
    std::memset((u8 *)samples->mData + copied * stride, 0, (frames - copied) * stride);
    return noErr;
}

bool
Audio::init(AudioRead reader, void *userdata, u32 sample_rate)
{
    assert(sample_rate > 0);
    *this = {};
    if (reader == nullptr) return false;
    this->read = reader;
    this->data = userdata;

    AudioComponentDescription description = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0,
    };
    AudioComponent component = AudioComponentFindNext(nullptr, &description);
    if (component == nullptr) {
        std::println(stderr, "Could not find the default CoreAudio output");
        return false;
    }
    OSStatus result = AudioComponentInstanceNew(component, &this->unit);
    if (result != noErr) {
        std::println(stderr, "Could not create the CoreAudio output unit: {}", result);
        this->free();
        return false;
    }

    // The output unit converts to the device's own rate and layout.
    AudioStreamBasicDescription format = {
        .mSampleRate = (Float64)sample_rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
        .mBytesPerPacket = Config::AUDIO_CHANNELS * sizeof(f32),
        .mFramesPerPacket = 1,
        .mBytesPerFrame = Config::AUDIO_CHANNELS * sizeof(f32),
        .mChannelsPerFrame = Config::AUDIO_CHANNELS,
        .mBitsPerChannel = 32,
        .mReserved = 0,
    };
    result = AudioUnitSetProperty(this->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                                  &format, sizeof(format));
    if (result != noErr) {
        std::println(stderr, "Could not set the CoreAudio stream format: {}", result);
        this->free();
        return false;
    }

    AURenderCallbackStruct callback = { .inputProc = on_render, .inputProcRefCon = this };
    result = AudioUnitSetProperty(this->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                                  &callback, sizeof(callback));
    if (result != noErr) {
        std::println(stderr, "Could not set the CoreAudio render callback: {}", result);
        this->free();
        return false;
    }

    result = AudioUnitInitialize(this->unit);
    if (result != noErr) {
        std::println(stderr, "Could not initialize CoreAudio: {}", result);
        this->free();
        return false;
    }
    this->initialized = true;

    result = AudioOutputUnitStart(this->unit);
    if (result != noErr) {
        std::println(stderr, "Could not start CoreAudio: {}", result);
        this->free();
        return false;
    }
    this->started = true;
    return true;
}

void
Audio::lock()
{
    os_unfair_lock_lock(&this->ring_lock);
}

void
Audio::unlock()
{
    os_unfair_lock_unlock(&this->ring_lock);
}

void
Audio::free()
{
    // Stopping waits for any render callback in flight.
    if (this->started) AudioOutputUnitStop(this->unit);
    if (this->initialized) AudioUnitUninitialize(this->unit);
    if (this->unit != nullptr) AudioComponentInstanceDispose(this->unit);
    #ifndef NDEBUG
    if (this->underrun_frames > 0) {
        std::println(stderr, "Audio ring underrun: {} frames filled with silence", this->underrun_frames);
    }
    #endif
    *this = {};
}
