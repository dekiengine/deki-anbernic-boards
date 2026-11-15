#pragma once

#include <deki/SetupComponent.h>
#include <deki/reflection/Property.h>
#include "AnbernicPackage.h"

namespace DekiAnbernic
{

/// Plays deki-audio's sound on the handheld's speaker and headphones. Add it
/// to the boot scene. Without deki-audio in the project it does nothing. A
/// game without sound still starts.
DEKI_CATEGORY("Anbernic")
DEKI_DISPLAY_NAME("Anbernic Audio")
DEKI_DESCRIPTION("Plays the game's audio on an Anbernic handheld's speaker.")
class DEKI_ANBERNIC_API AnbernicAudioSetup : public Deki::SetupComponent
{
public:
    DEKI_EXPORT
    DEKI_TOOLTIP("The sound card, as Linux numbers it. The handheld's own is 0.")
    DEKI_RANGE(0, 7)
    int32_t card = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("The playback device on that card. The speaker's is 0.")
    DEKI_RANGE(0, 7)
    int32_t device = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("Samples per second the game mixes at. When the device takes another rate, it uses that one.")
    DEKI_UNIT(Frequency)
    DEKI_RANGE(8000, 96000)
    int32_t sampleRate = 48000;

    DEKI_EXPORT
    DEKI_TOOLTIP("The game's own volume at startup, 0 to 100, on top of the handheld's. At 100 the game is as loud "
                 "as the handheld's volume is set.")
    DEKI_RANGE(0, 100)
    int32_t initialVolume = 100;

    void Setup(SetupCallback onComplete) override;
    const char* GetSetupName() const override { return "Anbernic Audio"; }
};

}  // namespace DekiAnbernic
