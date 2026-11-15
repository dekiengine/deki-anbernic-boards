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
    // The launcher's volume, which it does not set on the chip for an app.
    AnbernicVolume::ApplySystemLevel();
    m_State = Deki::PackageState::Running;
    return true;
}

void AnbernicAudio::Shutdown()
{
    Stop();
    AnbernicVolume::Restore();
    if (m_Fd >= 0)
    {
        ioctl(m_Fd, SNDRV_PCM_IOCTL_DROP);
        close(m_Fd);
        m_Fd = -1;
    }
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

bool AnbernicAudio::WriteFrames(const int16_t* samples, size_t frames)
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

}  // namespace DekiAnbernic

#endif
