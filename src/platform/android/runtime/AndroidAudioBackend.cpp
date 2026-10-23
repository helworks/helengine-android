#include "AndroidAudioBackend.hpp"
#include "AudioAsset.hpp"
#include "AudioPlaybackRequest.hpp"
#include <android/log.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace helengine::android {
    void AndroidAudioBackend::Initialize() {
        if (Output.IsInitialized()) return;
        Output.Initialize(WriteAudio,this);
        Stopping=false;
        Worker=std::thread(&AndroidAudioBackend::Mix,this);
    }
    AndroidAudioBackend::~AndroidAudioBackend() {
        { std::lock_guard<std::mutex> lock(VoicesMutex); Stopping.store(true,std::memory_order_release); }
        MixerWake.notify_all();
        if (Worker.joinable()) Worker.join();
    }
    void AndroidAudioBackend::SetHostPaused(bool paused) {
        if (!paused) Output.SetPaused(FocusPaused.load(std::memory_order_acquire));
        { std::lock_guard<std::mutex> lock(VoicesMutex); HostPaused.store(paused,std::memory_order_release); }
        MixerWake.notify_all();
        if (paused) Output.SetPaused(true);
    }
    void AndroidAudioBackend::SetFocusPaused(bool paused) {
        { std::lock_guard<std::mutex> lock(VoicesMutex); FocusPaused.store(paused,std::memory_order_release); }
        MixerWake.notify_all();
    }
    int32_t AndroidAudioBackend::Play(::AudioAsset* asset,::AudioPlaybackRequest* request) {
        if (!asset || !request || !Output.IsInitialized()) throw std::invalid_argument("Android audio playback requires an initialized backend, asset and request.");
        const auto channels=asset->get_Channels(); const auto rate=asset->get_SampleRate(); auto* bytes=asset->get_EncodedBytes();
        if ((channels!=1 && channels!=2) || rate<=0 || !bytes || bytes->Length<=0 || bytes->Length%(channels*2)) {
            throw std::runtime_error("Android audio requires complete mono or stereo PCM16 sample frames.");
        }
        if (!std::isfinite(request->get_Gain()) || request->get_Gain()<0 || !std::isfinite(request->get_StartOffsetSeconds()) || request->get_StartOffsetSeconds()<0) {
            throw std::runtime_error("Android audio gain and start offset must be finite and nonnegative.");
        }
        AndroidAudioVoice voice;
        voice.Channels=channels; voice.SampleRate=rate; voice.Gain=request->get_Gain(); voice.Loop=request->get_Loop();
        voice.Bus=request->get_BusId().empty()?"master":request->get_BusId().raw();
        voice.Samples.resize(bytes->Length/2);
        for (std::size_t i=0;i<voice.Samples.size();++i) voice.Samples[i]=static_cast<std::int16_t>((*bytes)[i*2]|(static_cast<std::uint16_t>((*bytes)[i*2+1])<<8));
        const auto frames=voice.Samples.size()/channels;
        voice.Position=std::min(static_cast<double>(frames),static_cast<double>(request->get_StartOffsetSeconds())*rate);
        voice.Playing=voice.Loop || voice.Position<frames;
        if (voice.Loop) voice.Position=std::fmod(voice.Position,static_cast<double>(frames));
        std::lock_guard<std::mutex> lock(VoicesMutex);
        const int id=NextVoice++;
        Voices.emplace(id,std::move(voice));
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Authored audio playback: voice=%d asset=%s sourceRate=%d channels=%d loop=%d.",id,asset->get_Id().c_str(),rate,channels,request->get_Loop());
        return id;
    }
    void AndroidAudioBackend::Stop(int32_t id) { std::lock_guard<std::mutex> lock(VoicesMutex); Voices.erase(id); }
    void AndroidAudioBackend::SetBusGain(HeCppString bus,float gain) {
        if (bus.empty() || !std::isfinite(gain) || gain<0) throw std::invalid_argument("Android audio bus gain must be finite and nonnegative on a named bus.");
        std::lock_guard<std::mutex> lock(VoicesMutex); BusGains[bus.raw()]=gain;
    }
    void AndroidAudioBackend::SetBusPaused(HeCppString bus,bool paused) {
        if (bus.empty()) throw std::invalid_argument("Android audio bus must have a name.");
        std::lock_guard<std::mutex> lock(VoicesMutex); if (paused) PausedBuses.insert(bus.raw()); else PausedBuses.erase(bus.raw());
    }
    bool AndroidAudioBackend::IsPlaying(int32_t id) {
        std::lock_guard<std::mutex> lock(VoicesMutex); auto found=Voices.find(id); return found!=Voices.end() && found->second.Playing;
    }
    void AndroidAudioBackend::Update() {
        Output.SetPaused(HostPaused.load(std::memory_order_acquire) || FocusPaused.load(std::memory_order_acquire));
        std::lock_guard<std::mutex> lock(VoicesMutex);
        for (auto voice=Voices.begin();voice!=Voices.end();) {
            if (!voice->second.Playing) voice=Voices.erase(voice); else ++voice;
        }
    }
    aaudio_data_callback_result_t AndroidAudioBackend::WriteAudio(AAudioStream*,void* state,void* data,int32_t frames) {
        auto& backend=*static_cast<AndroidAudioBackend*>(state); auto* output=static_cast<float*>(data);
        std::fill(output,output+frames*2,0.0f);
        if (backend.HostPaused.load(std::memory_order_acquire) || backend.FocusPaused.load(std::memory_order_acquire)) return AAUDIO_CALLBACK_RESULT_CONTINUE;
        auto read=backend.Read.load(std::memory_order_relaxed); const auto written=backend.Written.load(std::memory_order_acquire);
        const auto available=std::min<std::uint64_t>(written-read,frames);
        for (std::uint64_t i=0;i<available;++i) {
            const auto index=((read+i)%RingFrames)*2; output[i*2]=backend.Ring[index]; output[i*2+1]=backend.Ring[index+1];
        }
        backend.Read.store(read+available,std::memory_order_release);
        return backend.Stopping.load(std::memory_order_acquire)?AAUDIO_CALLBACK_RESULT_STOP:AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    void AndroidAudioBackend::Mix() {
        std::array<float,512*2> chunk{};
        while (!Stopping.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(VoicesMutex);
            MixerWake.wait(lock,[this] {
                return Stopping.load(std::memory_order_acquire)
                    || (!HostPaused.load(std::memory_order_acquire) && !FocusPaused.load(std::memory_order_acquire));
            });
            if (Stopping.load(std::memory_order_acquire)) break;
            const auto written=Written.load(std::memory_order_relaxed); const auto read=Read.load(std::memory_order_acquire);
            if (written-read>=4096) { lock.unlock(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }
            std::fill(chunk.begin(),chunk.end(),0.0f);
            {
                const auto master=BusGains.find("master"); const float masterGain=master==BusGains.end()?1.0f:master->second;
                for (auto& item : Voices) {
                    auto& voice=item.second;
                    if (!voice.Playing || PausedBuses.count("master") || PausedBuses.count(voice.Bus)) continue;
                    const auto bus=BusGains.find(voice.Bus); const float busGain=voice.Bus=="master" || bus==BusGains.end()?1.0f:bus->second;
                    const float gain=voice.Gain*busGain*masterGain/32768.0f;
                    const auto frameCount=voice.Samples.size()/voice.Channels; const double step=static_cast<double>(voice.SampleRate)/AndroidAudioOutput::SampleRate;
                    for (std::size_t frame=0;frame<512;++frame) {
                        if (voice.Position>=frameCount) {
                            if (!voice.Loop) { voice.Playing=false; break; }
                            voice.Position=std::fmod(voice.Position,static_cast<double>(frameCount));
                        }
                        const auto first=static_cast<std::size_t>(voice.Position);
                        const auto second=first+1<frameCount?first+1:(voice.Loop?0:first); const double fraction=voice.Position-first;
                        for (int channel=0;channel<2;++channel) {
                            const auto sourceChannel=voice.Channels==1?0:channel;
                            const auto a=voice.Samples[first*voice.Channels+sourceChannel]; const auto b=voice.Samples[second*voice.Channels+sourceChannel];
                            chunk[frame*2+channel]+=static_cast<float>(a+(b-a)*fraction)*gain;
                        }
                        voice.Position+=step;
                    }
                }
            }
            for (std::size_t frame=0;frame<512;++frame) {
                const auto index=((written+frame)%RingFrames)*2;
                Ring[index]=std::clamp(chunk[frame*2],-1.0f,1.0f); Ring[index+1]=std::clamp(chunk[frame*2+1],-1.0f,1.0f);
            }
            Written.store(written+512,std::memory_order_release);
        }
    }
}
