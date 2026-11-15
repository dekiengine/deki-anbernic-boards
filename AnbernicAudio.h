#pragma once

// The handheld's speaker as deki-audio's output: Linux's sound driver (ALSA)
// spoken to directly, through its device file and ioctls, since the game is a
// static program with no sound library. Built only when the project has
// deki-audio.

#if __has_include("IDekiAudio.h")
#define DEKI_ANBERNIC_HAS_AUDIO 1

#include "IDekiAudio.h"  // from deki-audio
#include "PumpWorker.h"  // from deki-audio
#include <deki/PackageConfig.h>
#include <deki/providers/Buffer.h>
#include <atomic>
#include <string>

namespace DekiAnbernic
{

/// Plays the mixer on an ALSA playback device (/dev/snd/pcmC<card>D<device>p),
/// from a thread that mixes a period and writes it, the write waiting for the
/// device to take it.
class AnbernicAudio : public DekiAudio::IDekiAudio
{
public:
    AnbernicAudio() = default;
    ~AnbernicAudio() override;

    const char* GetPackageId() const override { return "audio"; }
    const char* GetPackageName() const override { return "Anbernic Audio"; }
    /// Reads "card" (0), "device" (0), "sampleRate" (48000) and "channels" (2).
    void Configure(const Deki::PackageConfig& config) override;
    /// Mutes the chip, opens the device and sets its format. The rate and
    /// channels it takes may differ from those asked for; GetSampleRate and
    /// GetChannels say.
    bool Initialize() override;
    void Shutdown() override;
    void Update(float) override {}
    Deki::PackageState GetState() const override { return m_State; }
    const char* GetLastError() const override { return m_LastError.c_str(); }

    int GetSampleRate() const override { return m_SampleRate; }
    int GetChannels() const override { return m_Channels; }
    bool Start() override;
    void Stop() override;
    bool IsRunning() const override { return m_Pump.IsRunning(); }
    bool IsHardwareConnected() const override { return m_Fd >= 0; }

    /// Writes interleaved frames, waiting until the device takes them. What
    /// the pump thread calls.
    bool WriteFrames(const int16_t* samples, size_t frames);

private:
    bool SetFormat();
    bool WriteRaw(const int16_t* samples, size_t frames);
    bool WriteSilence(int periods);

    DekiAudio::PumpWorker m_Pump;
    int m_Fd = -1;
    int m_Card = 0;
    int m_Device = 0;
    int m_SampleRate = 48000;
    int m_Channels = 2;
    int m_PeriodFrames = 512;
    // The sound fades in over kFadeInMs once it starts and out over
    // kFadeOutMs before the device closes, and the chip's level changes only
    // in kSilentPeriods of silence around them: no pop and no click.
    enum class Phase
    {
        Starting,
        FadingIn,
        Playing,
        FadingOut,
        Done,
    };
    static constexpr int kFadeInMs = 150;
    static constexpr int kFadeOutMs = 120;
    static constexpr int kSilentPeriods = 2;
    Phase m_Phase = Phase::Starting;
    int m_FadeAt = 0;
    std::atomic<bool> m_FadingOut{ false };
    Deki::Buffer<int16_t> m_Faded;
    Deki::PackageState m_State = Deki::PackageState::Uninitialized;
    std::string m_LastError;
};

}  // namespace DekiAnbernic

#endif
