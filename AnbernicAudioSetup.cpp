#include "AnbernicAudioSetup.h"
#include "AnbernicAudio.h"
#include <deki/LogSystem.h>

#if DEKI_ANBERNIC_HAS_AUDIO && !defined(DEKI_EDITOR) && defined(DEKI_PACKAGE_ANBERNIC)
#include "AudioOutput.h"    // from deki-audio
#include "DekiAudioInit.h"  // DekiAudioInitSystem (from deki-audio)
#include "Mixer.h"          // from deki-audio
#include <string>
#endif

namespace DekiAnbernic
{

#if DEKI_ANBERNIC_HAS_AUDIO && !defined(DEKI_EDITOR) && defined(DEKI_PACKAGE_ANBERNIC)

void AnbernicAudioSetup::Setup(SetupCallback onComplete)
{
    // The static build links the engine's empty DekiInitPackageSystems(), so
    // deki-audio starts here. Safe to repeat.
    DekiAudioInitSystem();

    static AnbernicAudio s_Audio;
    Deki::PackageConfig config;
    config.packageId = "audio";
    config.enabled = true;
    config.settings["card"] = std::to_string(card);
    config.settings["device"] = std::to_string(device);
    config.settings["sampleRate"] = std::to_string(sampleRate);
    config.settings["channels"] = "2";
    s_Audio.Configure(config);

    const int volume = initialVolume < 0 ? 0 : (initialVolume > 100 ? 100 : static_cast<int>(initialVolume));
    DekiAudio::Mixer::Instance().SetMasterVolume(static_cast<float>(volume) * 0.01f);
    if (s_Audio.Initialize())
    {
        DekiAudio::AudioOutput::SetCurrent(&s_Audio);
    }
    else
    {
        DEKI_LOG_WARNING("AnbernicAudioSetup: no sound: %s", s_Audio.GetLastError());
    }
    // A game without sound still starts.
    onComplete(true);
}

#else

// The editor and other targets play sound elsewhere; a project without
// deki-audio has nothing to play.
void AnbernicAudioSetup::Setup(SetupCallback onComplete)
{
    onComplete(true);
}

#endif

}  // namespace DekiAnbernic
