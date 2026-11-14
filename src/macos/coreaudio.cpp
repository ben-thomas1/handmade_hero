#include "macos/coreaudio.hpp"

#include <AudioToolbox/AudioToolbox.h>

#include <cstdlib>
#include <cstring>


static OSStatus
on_render(void *data, AudioUnitRenderActionFlags *, const AudioTimeStamp *, UInt32, UInt32 frame_count,
          AudioBufferList *buffers)
{
    Audio *audio = (Audio *)data;
    i16 *out = (i16 *)buffers->mBuffers[0].mData;

    os_unfair_lock_lock(&audio->lock);
    u32 available = (u32)(audio->frames_written - audio->frames_played);
    u32 n = frame_count < available ? frame_count : available;
    u32 start = (u32)(audio->frames_played % audio->frame_count);
    u32 first = audio->frame_count - start < n ? audio->frame_count - start : n;
    std::memcpy(out, audio->samples + 2 * start, first * 2 * sizeof(i16));
    std::memcpy(out + 2 * first, audio->samples, (n - first) * 2 * sizeof(i16));
    audio->frames_played += n;
    os_unfair_lock_unlock(&audio->lock);

    // Silence if the game fell behind.
    std::memset(out + 2 * n, 0, (frame_count - n) * 2 * sizeof(i16));
    return noErr;
}

void
Audio::init(u32 sample_rate)
{
    this->sample_rate = sample_rate;
    this->frame_count = sample_rate;
    this->samples = (i16 *)std::malloc((size_t)this->frame_count * 2 * sizeof(i16));
    this->lock = OS_UNFAIR_LOCK_INIT;

    AudioComponentDescription description = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0,
    };
    AudioComponentInstanceNew(AudioComponentFindNext(nullptr, &description), &this->unit);

    AudioStreamBasicDescription format = {
        .mSampleRate = (Float64)sample_rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
        .mBytesPerPacket = 2 * sizeof(i16),
        .mFramesPerPacket = 1,
        .mBytesPerFrame = 2 * sizeof(i16),
        .mChannelsPerFrame = 2,
        .mBitsPerChannel = 16,
        .mReserved = 0,
    };
    AudioUnitSetProperty(this->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                         &format, sizeof(format));

    AURenderCallbackStruct callback = { .inputProc = on_render, .inputProcRefCon = this };
    AudioUnitSetProperty(this->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                         &callback, sizeof(callback));

    AudioUnitInitialize(this->unit);
    AudioOutputUnitStart(this->unit);
}

u32
Audio::frames_to_write()
{
    u32 latency = this->sample_rate / 15;
    os_unfair_lock_lock(&this->lock);
    u32 queued = (u32)(this->frames_written - this->frames_played);
    os_unfair_lock_unlock(&this->lock);
    return queued < latency ? latency - queued : 0;
}

void
Audio::write(SoundBuffer *sound)
{
    // The audio thread only reads frames before frames_written, so this region is ours.
    u32 n = sound->frame_count;
    u32 start = (u32)(this->frames_written % this->frame_count);
    u32 first = this->frame_count - start < n ? this->frame_count - start : n;
    std::memcpy(this->samples + 2 * start, sound->samples, first * 2 * sizeof(i16));
    std::memcpy(this->samples, sound->samples + 2 * first, (n - first) * 2 * sizeof(i16));

    os_unfair_lock_lock(&this->lock);
    this->frames_written += n;
    os_unfair_lock_unlock(&this->lock);
}
