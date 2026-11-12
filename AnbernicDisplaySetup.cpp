#include "AnbernicDisplaySetup.h"
#include <deki/LogSystem.h>

#if !defined(DEKI_EDITOR) && defined(DEKI_PACKAGE_ANBERNIC)
#include "AnbernicDisplay.h"
#include <deki/Engine.h>
#include <memory>
#endif

namespace DekiAnbernic
{

#if !defined(DEKI_EDITOR) && defined(DEKI_PACKAGE_ANBERNIC)

// The package owns the display and keeps it for the whole program; the engine
// only holds a pointer to it.
static std::unique_ptr<AnbernicDisplay> s_AnbernicDisplay;

#if !defined(DEKI_SCREEN_WIDTH) || !defined(DEKI_SCREEN_HEIGHT) || !defined(DEKI_SCREEN_COLOR_FORMAT)
#error                                                                                                                 \
    "An Anbernic build must define DEKI_SCREEN_WIDTH, DEKI_SCREEN_HEIGHT and DEKI_SCREEN_COLOR_FORMAT (the platform's screenWidth, screenHeight and colorFormat)."
#endif

void AnbernicDisplaySetup::Setup(SetupCallback onComplete)
{
    DEKI_LOG_INFO("AnbernicDisplaySetup: Opening the framebuffer (%dx%d screen, scale %d)",
                  static_cast<int>(DEKI_SCREEN_WIDTH), static_cast<int>(DEKI_SCREEN_HEIGHT), static_cast<int>(scale));

    s_AnbernicDisplay = std::make_unique<AnbernicDisplay>();
    s_AnbernicDisplay->SetScale(scale);
    s_AnbernicDisplay->SetPanelSize(panelWidth, panelHeight);
    s_AnbernicDisplay->SetColorFormat(DEKI_SCREEN_COLOR_FORMAT);
    if (s_AnbernicDisplay->Initialize(DEKI_SCREEN_WIDTH, DEKI_SCREEN_HEIGHT))
    {
        Deki::Engine::GetInstance().SetDisplay(s_AnbernicDisplay.get(), "Anbernic");
        onComplete(true);
    }
    else
    {
        DEKI_LOG_ERROR("AnbernicDisplaySetup: Failed to open the framebuffer");
        s_AnbernicDisplay.reset();
        onComplete(false);
    }
}

#else

void AnbernicDisplaySetup::Setup(SetupCallback onComplete)
{
    // The editor and other targets have no Anbernic screen to open.
    onComplete(true);
}

#endif

}  // namespace DekiAnbernic
