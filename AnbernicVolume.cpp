#include "AnbernicVolume.h"

#include <cstdint>

namespace DekiAnbernic
{

long AnbernicVolume::ControlValueForLevel(int level, long controlMax)
{
    if (level <= 0)
    {
        return 0;
    }
    if (level >= kMaxLevel)
    {
        return controlMax;
    }
    const long value = controlMax - static_cast<long>(kMaxLevel - level) * 3;
    return value < 1 ? 1 : value;
}

int AnbernicVolume::LevelFromLauncherSettings(const unsigned char* data, unsigned long size)
{
    constexpr unsigned long kFileBytes = 140;
    constexpr unsigned long kLevelAt = 36;
    if (!data || size != kFileBytes)
    {
        return -1;
    }
    const long level = static_cast<long>(data[kLevelAt]) | (static_cast<long>(data[kLevelAt + 1]) << 8) |
                       (static_cast<long>(data[kLevelAt + 2]) << 16) | (static_cast<long>(data[kLevelAt + 3]) << 24);
    return (level >= 0 && level <= kMaxLevel) ? static_cast<int>(level) : -1;
}

unsigned long AnbernicVolume::LauncherChecksum(const unsigned char* data, unsigned long size)
{
    uint32_t crc = 0;
    for (unsigned long i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
        }
    }
    return crc;
}

bool AnbernicVolume::SetLevelInLauncherSettings(unsigned char* data, unsigned long size, int level)
{
    if (LevelFromLauncherSettings(data, size) < 0 || level < 0 || level > kMaxLevel)
    {
        return false;
    }
    data[36] = static_cast<unsigned char>(level);
    data[37] = data[38] = data[39] = 0;
    const unsigned long crc = LauncherChecksum(data, size - 4);
    for (int i = 0; i < 4; ++i)
    {
        data[size - 4 + i] = static_cast<unsigned char>(crc >> (8 * i));
    }
    return true;
}

}  // namespace DekiAnbernic

#if defined(__linux__) && !defined(DEKI_EDITOR)

#include <deki/LogSystem.h>

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
// 31 steps of 1.5 dB, before the speaker and the headphones.
constexpr const char* kControlName = "lineout volume";
// Where the stock launcher keeps its settings, and where it keeps its volume
// for the next start: it writes both as it opens an app, and reads the volume
// back from the battery driver.
constexpr const char* kLauncherSettings = "/mnt/data/dmenu/dmenu_attr.ini";
constexpr const char* kLauncherVolume = "/sys/class/power_supply/axp2202-battery/openbor_volume";

int s_Fd = -1;
bool s_Tried = false;
snd_ctl_elem_id s_Id{};
long s_Max = 0;
long s_Original = -1;
int s_Level = AnbernicVolume::kMaxLevel;

bool OpenControl()
{
    if (s_Tried)
    {
        return s_Fd >= 0;
    }
    s_Tried = true;
    s_Fd = open("/dev/snd/controlC0", O_RDWR | O_CLOEXEC);
    if (s_Fd < 0)
    {
        return false;
    }
    std::memset(&s_Id, 0, sizeof(s_Id));
    s_Id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
    std::strncpy(reinterpret_cast<char*>(s_Id.name), kControlName, sizeof(s_Id.name) - 1);
    snd_ctl_elem_info info{};
    info.id = s_Id;
    if (ioctl(s_Fd, SNDRV_CTL_IOCTL_ELEM_INFO, &info) != 0 || info.type != SNDRV_CTL_ELEM_TYPE_INTEGER)
    {
        DEKI_LOG_WARNING("AnbernicVolume: the sound chip has no '%s'; the volume stays as it is", kControlName);
        close(s_Fd);
        s_Fd = -1;
        return false;
    }
    s_Id = info.id;
    s_Max = info.value.integer.max;

    snd_ctl_elem_value v{};
    v.id = s_Id;
    if (ioctl(s_Fd, SNDRV_CTL_IOCTL_ELEM_READ, &v) == 0)
    {
        s_Original = v.value.integer.value[0];
    }
    return true;
}

bool Write(long value)
{
    snd_ctl_elem_value v{};
    v.id = s_Id;
    v.value.integer.value[0] = value;
    v.value.integer.value[1] = value;
    return ioctl(s_Fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &v) == 0;
}

// The launcher reads its volume when it starts again, after the game. Its
// settings file gets a copy written and renamed over it, so it never sees
// half of one.
void WriteLauncherLevel(int level)
{
    if (std::FILE* v = std::fopen(kLauncherVolume, "w"))
    {
        std::fprintf(v, "%d", level);
        std::fclose(v);
    }

    std::FILE* f = std::fopen(kLauncherSettings, "rb");
    if (!f)
    {
        return;
    }
    unsigned char data[256];
    const size_t got = std::fread(data, 1, sizeof(data), f);
    std::fclose(f);
    if (!AnbernicVolume::SetLevelInLauncherSettings(data, got, level))
    {
        return;
    }
    char temp[128];
    std::snprintf(temp, sizeof(temp), "%s.tmp", kLauncherSettings);
    std::FILE* out = std::fopen(temp, "wb");
    if (!out)
    {
        return;
    }
    const bool written = std::fwrite(data, 1, got, out) == got && std::fflush(out) == 0 && fsync(fileno(out)) == 0;
    std::fclose(out);
    if (!written || std::rename(temp, kLauncherSettings) != 0)
    {
        std::remove(temp);
    }
}

int ReadLauncherLevel()
{
    std::FILE* f = std::fopen(kLauncherSettings, "rb");
    if (!f)
    {
        return -1;
    }
    unsigned char data[256];
    const size_t got = std::fread(data, 1, sizeof(data), f);
    std::fclose(f);
    return AnbernicVolume::LevelFromLauncherSettings(data, got);
}
}  // namespace

void AnbernicVolume::ApplySystemLevel()
{
    const int level = ReadLauncherLevel();
    if (level < 0 || !OpenControl())
    {
        return;
    }
    s_Level = level;
    Write(ControlValueForLevel(s_Level, s_Max));
    DEKI_LOG_INFO("AnbernicVolume: the launcher's volume, %d of %d", s_Level, kMaxLevel);
}

bool AnbernicVolume::Step(int direction)
{
    if (!OpenControl())
    {
        return false;
    }
    s_Level += direction > 0 ? 1 : -1;
    s_Level = s_Level < 0 ? 0 : (s_Level > kMaxLevel ? kMaxLevel : s_Level);
    const bool set = Write(ControlValueForLevel(s_Level, s_Max));
    WriteLauncherLevel(s_Level);
    return set;
}

int AnbernicVolume::GetLevel()
{
    return s_Level;
}

void AnbernicVolume::Restore()
{
    if (s_Fd >= 0)
    {
        if (s_Original >= 0)
        {
            Write(s_Original);
        }
        close(s_Fd);
    }
    s_Fd = -1;
    s_Tried = false;
    s_Original = -1;
}

}  // namespace DekiAnbernic

#else

namespace DekiAnbernic
{

void AnbernicVolume::ApplySystemLevel()
{
}

bool AnbernicVolume::Step(int)
{
    return false;
}

int AnbernicVolume::GetLevel()
{
    return kMaxLevel;
}

void AnbernicVolume::Restore()
{
}

}  // namespace DekiAnbernic

#endif
