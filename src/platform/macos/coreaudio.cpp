#include "platform/macos/coreaudio.hpp"
#include "game/sound.hpp"

#include <AudioToolbox/AudioToolbox.h>

#include <cstring>
#include <print>


static OSStatus
on_render(void *data, AudioUnitRenderActionFlags *, const AudioTimeStamp *, UInt32, UInt32 frame_count,
          AudioBufferList *buffers)
{
    Audio *audio = (Audio *)data;
    AudioBuffer *samples = &buffers->mBuffers[0];

    u32 stride = AUDIO_CHANNELS * sizeof(f32);
    u32 frames = samples->mDataByteSize / stride;
    if (frame_count < frames) frames = frame_count;
    audio_lock(audio);
    u32 copied = audio->read(audio->data, samples->mData, frames);
    if (copied > frames) copied = frames;
    audio->underrun_frames += frames - copied;
    audio_unlock(audio);
    std::memset((u8 *)samples->mData + copied * stride, 0, (frames - copied) * stride);
    return noErr;
}

bool
audio_init(Audio *audio, AudioRead read, void *data)
{
    *audio = {};
    if (read == nullptr) return false;
    audio->read = read;
    audio->data = data;

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
    OSStatus result = AudioComponentInstanceNew(component, &audio->unit);
    if (result != noErr) {
        std::println(stderr, "Could not create the CoreAudio output unit: {}", result);
        audio_shutdown(audio);
        return false;
    }

    // The output unit converts to the device's own rate and layout.
    AudioStreamBasicDescription format = {
        .mSampleRate = AUDIO_SAMPLE_RATE,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
        .mBytesPerPacket = AUDIO_CHANNELS * sizeof(f32),
        .mFramesPerPacket = 1,
        .mBytesPerFrame = AUDIO_CHANNELS * sizeof(f32),
        .mChannelsPerFrame = AUDIO_CHANNELS,
        .mBitsPerChannel = 32,
        .mReserved = 0,
    };
    result = AudioUnitSetProperty(audio->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                                  &format, sizeof(format));
    if (result != noErr) {
        std::println(stderr, "Could not set the CoreAudio stream format: {}", result);
        audio_shutdown(audio);
        return false;
    }

    AURenderCallbackStruct callback = { .inputProc = on_render, .inputProcRefCon = audio };
    result = AudioUnitSetProperty(audio->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                                  &callback, sizeof(callback));
    if (result != noErr) {
        std::println(stderr, "Could not set the CoreAudio render callback: {}", result);
        audio_shutdown(audio);
        return false;
    }

    result = AudioUnitInitialize(audio->unit);
    if (result != noErr) {
        std::println(stderr, "Could not initialize CoreAudio: {}", result);
        audio_shutdown(audio);
        return false;
    }
    audio->initialized = true;

    result = AudioOutputUnitStart(audio->unit);
    if (result != noErr) {
        std::println(stderr, "Could not start CoreAudio: {}", result);
        audio_shutdown(audio);
        return false;
    }
    audio->started = true;
    return true;
}

void
audio_lock(Audio *audio)
{
    os_unfair_lock_lock(&audio->lock);
}

void
audio_unlock(Audio *audio)
{
    os_unfair_lock_unlock(&audio->lock);
}

void
audio_shutdown(Audio *audio)
{
    // Stopping waits for any render callback in flight.
    if (audio->started) AudioOutputUnitStop(audio->unit);
    if (audio->initialized) AudioUnitUninitialize(audio->unit);
    if (audio->unit != nullptr) AudioComponentInstanceDispose(audio->unit);
    #ifndef NDEBUG
    if (audio->underrun_frames > 0) {
        std::println(stderr, "Audio ring underrun: {} frames filled with silence", audio->underrun_frames);
    }
    #endif
    *audio = {};
}
