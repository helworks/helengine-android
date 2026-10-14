#pragma once
#include <cstdint>
#include <string>
class AudioAsset;
class AudioPlaybackRequest;
/// Mirrors only the generated backend ABI so native tests compile the real platform source without a generated engine.
class ISeekableAudioBackend {
public:
    /// Allows destruction through the audio backend interface.
    virtual ~ISeekableAudioBackend()=default;
    /// Begins authored playback at its requested offset.
    virtual int32_t Play(AudioAsset*,AudioPlaybackRequest*)=0;
    /// Stops one active voice.
    virtual void Stop(int32_t)=0;
    /// Changes one bus volume.
    virtual void SetBusGain(std::string,float)=0;
    /// Changes one bus pause state.
    virtual void SetBusPaused(std::string,bool)=0;
    /// Queries source playback status.
    virtual bool IsPlaying(int32_t)=0;
    /// Performs maintenance on the engine thread.
    virtual void Update()=0;
};
