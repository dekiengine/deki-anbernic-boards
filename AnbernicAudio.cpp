#include "AnbernicAudio.h"
#include "AnbernicVolume.h"

#if DEKI_ANBERNIC_HAS_AUDIO && defined(__linux__)

#include <deki/LogSystem.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sound/asound.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace DekiAnbernic
{

namespace
{

// ALSA's parameters are masks (a bit per allowed value) and intervals (a
// range). Every one starts open; each Set narrows one.
void OpenAll(snd_pcm_hw_params& p)
{
    std::memset(&p, 0, sizeof(p));
    for (auto& mask : p.masks)
    {
        std::memset(mask.bits, 0xFF, sizeof(mask.bits));
    }
    for (auto& interval : p.intervals)
    {
        interval.min = 0;
        interval.max = ~0u;
    }
    p.rmask = ~0u;
    p.cmask = 0;
    p.info = ~0u;
}

void SetMask(snd_pcm_hw_params& p, int param, unsigned value)
{
    snd_mask& mask = p.masks[param - SNDRV_PCM_HW_PARAM_FIRST_MASK];
    std::memset(mask.bits, 0, sizeof(mask.bits));
    mask.bits[value >> 5] |= 1u << (value & 31);
}

snd_interval& IntervalOf(snd_pcm_hw_params& p, int param)
{
    return p.intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
}

void SetValue(snd_pcm_hw_params& p, int param, unsigned value)
{
    snd_interval& i = IntervalOf(p, param);
    i.min = value;
    i.max = value;
    i.openmin = 0;
    i.openmax = 0;
    i.integer = 1;
}

void SetRange(snd_pcm_hw_params& p, int param, unsigned lo, unsigned hi)
{
    snd_interval& i = IntervalOf(p, param);
    i.min = lo;
    i.max = hi;
    i.integer = 1;
}

}  // namespace

AnbernicAudio::~AnbernicAudio()
{
    Shutdown();
}

void AnbernicAudio::Configure(const Deki::PackageConfig& config)
{
    m_Card = config.GetInt("card", 0);
    m_Device = config.GetInt("device", 0);
    m_SampleRate = config.GetInt("sampleRate", 48000);
    m_Channels = config.GetInt("channels", 2) == 1 ? 1 : 2;
}

bool AnbernicAudio::SetFormat()
{
    // A period of 512 frames, four of them: about 43 ms at 48 kHz. Where the
    // device wants other sizes, a range lets it choose.
    const unsigned rates[2] = { static_cast<unsigned>(m_SampleRate), 44100 };
    for (unsigned rate : rates)
    {
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            snd_pcm_hw_params p;
            OpenAll(p);
            SetMask(p, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
            SetMask(p, SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FORMAT_S16_LE);
            SetMask(p, SNDRV_PCM_HW_PARAM_SUBFORMAT, SNDRV_PCM_SUBFORMAT_STD);
            SetValue(p, SNDRV_PCM_HW_PARAM_CHANNELS, static_cast<unsigned>(m_Channels));
            SetValue(p, SNDRV_PCM_HW_PARAM_RATE, rate);
            if (attempt == 0)
            {
                SetValue(p, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, 512);
                SetValue(p, SNDRV_PCM_HW_PARAM_PERIODS, 4);
            }
            else
            {
                SetRange(p, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, 128, 2048);
                SetRange(p, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, 1024, 8192);
            }
            if (ioctl(m_Fd, SNDRV_PCM_IOCTL_HW_PARAMS, &p) == 0)
            {
                m_SampleRate = static_cast<int>(IntervalOf(p, SNDRV_PCM_HW_PARAM_RATE).min);
                m_PeriodFrames = static_cast<int>(IntervalOf(p, SNDRV_PCM_HW_PARAM_PERIOD_SIZE).min);
                return true;
            }
        }
    }
    m_LastError = std::string("AnbernicAudio: the device takes none of the formats asked for: ") + std::strerror(errno);
    return false;
}

bool AnbernicAudio::Initialize()
{
    Shutdown();
    // Silent before the device opens, so its amplifier powers up silent; the
    // launcher's volume fades in once sound flows (WriteFrames).
    AnbernicVolume::BeginMuted();
    char path[64];
    std::snprintf(path, sizeof(path), "/dev/snd/pcmC%dD%dp", m_Card, m_Device);
    m_Fd = open(path, O_RDWR | O_CLOEXEC);
    if (m_Fd < 0)
    {
        m_LastError = std::string("AnbernicAudio: cannot open ") + path + ": " + std::strerror(errno);
        m_State = Deki::PackageState::Error;
        return false;
    }
    if (!SetFormat() || ioctl(m_Fd, SNDRV_PCM_IOCTL_PREPARE) != 0)
    {
        if (m_LastError.empty())
        {
            m_LastError = std::string("AnbernicAudio: the device did not get ready: ") + std::strerror(errno);
        }
        close(m_Fd);
        m_Fd = -1;
        m_State = Deki::PackageState::Error;
        return false;
    }
    DEKI_LOG_INFO("AnbernicAudio: %s at %d Hz, %d channels, %d-frame periods", path, m_SampleRate, m_Channels,
                  m_PeriodFrames);
    m_State = Deki::PackageState::Running;
    return true;
}

void AnbernicAudio::Shutdown()
{
    if (m_Pump.IsRunning())
    {
        // Fades out while the sound still plays, so it does not stop on a
        // step; the pump thread does it (WriteFrames).
        m_FadingOut = true;
        for (int i = 0; i < 100 && m_FadingOut.load(); ++i)
        {
            usleep(5000);
        }
    }
    Stop();
    if (m_Fd >= 0)
    {
        // Plays what is queued (silent by now), then closes.
        ioctl(m_Fd, SNDRV_PCM_IOCTL_DRAIN);
        close(m_Fd);
        m_Fd = -1;
    }
    // The chip's level as the game found it, once the device is closed.
    AnbernicVolume::Restore();
    m_FadingOut = false;
    m_Phase = Phase::Starting;
    m_State = Deki::PackageState::Uninitialized;
}

bool AnbernicAudio::Start()
{
    if (m_Fd < 0)
    {
        m_LastError = "AnbernicAudio: no device";
        return false;
    }
    if (m_Pump.IsRunning())
    {
        return true;
    }
    if (!m_Faded.Allocate(static_cast<size_t>(m_PeriodFrames) * static_cast<size_t>(m_Channels),
                          Deki::Memory::Internal))
    {
        m_LastError = "AnbernicAudio: no memory for the audio";
        return false;
    }
    m_Phase = Phase::Starting;
    auto write = [](void* self, const int16_t* samples, size_t frames)
    { return static_cast<AnbernicAudio*>(self)->WriteFrames(samples, frames); };
    if (!m_Pump.Start("deki_audio", m_Channels, static_cast<size_t>(m_PeriodFrames), write, this))
    {
        m_LastError = "AnbernicAudio: no memory or thread for the audio";
        return false;
    }
    return true;
}

void AnbernicAudio::Stop()
{
    m_Pump.Stop();
}

bool AnbernicAudio::WriteRaw(const int16_t* samples, size_t frames)
{
    const int16_t* at = samples;
    size_t left = frames;
    while (left > 0)
    {
        snd_xferi xfer;
        std::memset(&xfer, 0, sizeof(xfer));
        xfer.buf = const_cast<int16_t*>(at);
        xfer.frames = left;
        if (ioctl(m_Fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &xfer) == 0)
        {
            const size_t done = static_cast<size_t>(xfer.result);
            at += done * static_cast<size_t>(m_Channels);
            left -= done;
            continue;
        }
        if (errno == EINTR || errno == EAGAIN)
        {
            continue;
        }
        if (errno == EPIPE || errno == ESTRPIPE)
        {
            // Ran dry (the game stalled) or the device was suspended: get it
            // ready again and go on.
            if (ioctl(m_Fd, SNDRV_PCM_IOCTL_PREPARE) == 0)
            {
                continue;
            }
        }
        return false;
    }
    return true;
}

bool AnbernicAudio::WriteSilence(int periods)
{
    std::memset(m_Faded.Data(), 0, m_Faded.Bytes());
    for (int i = 0; i < periods; ++i)
    {
        if (!WriteRaw(m_Faded.Data(), static_cast<size_t>(m_PeriodFrames)))
        {
            return false;
        }
    }
    return true;
}

bool AnbernicAudio::WriteFrames(const int16_t* samples, size_t frames)
{
    // The chip's level moves only while the sound is silent, since a step in
    // gain with sound playing clicks: silence, then the level, then the sound
    // fading in sample by sample; at the end the sound fades out, then
    // silence, then the chip goes quiet before the device closes.
    if (m_Phase == Phase::Starting)
    {
        if (!WriteSilence(kSilentPeriods))
        {
            return false;
        }
        AnbernicVolume::SetFade(AnbernicVolume::kFadeFull);
        m_Phase = Phase::FadingIn;
        m_FadeAt = 0;
    }

    const int fadeInFrames = m_SampleRate * kFadeInMs / 1000;
    const int fadeOutFrames = m_SampleRate * kFadeOutMs / 1000;
    if (m_FadingOut.load() && m_Phase != Phase::FadingOut && m_Phase != Phase::Done)
    {
        // From wherever a fade in got to, so the gain does not jump.
        const int gainNow = m_Phase == Phase::FadingIn ? m_FadeAt * fadeOutFrames / fadeInFrames : fadeOutFrames;
        m_Phase = Phase::FadingOut;
        m_FadeAt = gainNow;
    }

    if (m_Phase == Phase::Playing)
    {
        return WriteRaw(samples, frames);
    }
    if (m_Phase == Phase::Done)
    {
        return WriteSilence(1);
    }

    int16_t* out = m_Faded.Data();
    for (size_t f = 0; f < frames; ++f)
    {
        float gain = 0.0f;
        if (m_Phase == Phase::FadingIn)
        {
            gain = m_FadeAt < fadeInFrames ? static_cast<float>(m_FadeAt) / fadeInFrames : 1.0f;
            ++m_FadeAt;
        }
        else
        {
            gain = m_FadeAt > 0 ? static_cast<float>(m_FadeAt) / fadeOutFrames : 0.0f;
            --m_FadeAt;
        }
        for (int c = 0; c < m_Channels; ++c)
        {
            const size_t i = f * static_cast<size_t>(m_Channels) + static_cast<size_t>(c);
            out[i] = static_cast<int16_t>(static_cast<float>(samples[i]) * gain);
        }
    }
    if (!WriteRaw(out, frames))
    {
        return false;
    }

    if (m_Phase == Phase::FadingIn && m_FadeAt >= fadeInFrames)
    {
        m_Phase = Phase::Playing;
    }
    else if (m_Phase == Phase::FadingOut && m_FadeAt <= 0)
    {
        if (!WriteSilence(kSilentPeriods))
        {
            return false;
        }
        AnbernicVolume::SetFade(0);
        m_Phase = Phase::Done;
        m_FadingOut = false;
    }
    return true;
}

}  // namespace DekiAnbernic

#endif
