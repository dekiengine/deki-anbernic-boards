#pragma once

#include <cstdint>
#include <deki/SetupComponent.h>
#include <deki/reflection/Property.h>
#include "AnbernicPackage.h"

namespace DekiAnbernic
{

/// Opens the handheld's screen (the Linux framebuffer) at the panel's size.
/// The game renders at the platform's screen size; the display scales it up
/// by whole pixels to fill the panel. Runs as part of the boot sequence.
DEKI_CATEGORY("Anbernic")
DEKI_DESCRIPTION("Shows the game on an Anbernic handheld's screen.")
class DEKI_ANBERNIC_API AnbernicDisplaySetup : public Deki::SetupComponent
{
public:
    DEKI_EXPORT
    DEKI_TOOLTIP("Panel pixels per screen pixel. 0 scales as large as fits.")
    DEKI_RANGE(0, 8)
    int32_t scale = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("The panel's width in pixels. The RG40XX H's is 640.")
    DEKI_RANGE(1, 4096)
    int32_t panelWidth = 640;

    DEKI_EXPORT
    DEKI_TOOLTIP("The panel's height in pixels. The RG40XX H's is 480.")
    DEKI_RANGE(1, 4096)
    int32_t panelHeight = 480;

    void Setup(SetupCallback onComplete) override;
    const char* GetSetupName() const override { return "Anbernic Display"; }
};

}  // namespace DekiAnbernic
