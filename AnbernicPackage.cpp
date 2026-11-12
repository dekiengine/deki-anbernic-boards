// Package entry point for the deki-anbernic DLL. Exports the standard Deki plugin
// interface, so the editor can load the DLL and find its components.
//
// Display and input are set up by their SetupComponents; the Anbernic time
// provider is registered in AnbernicDisplay.cpp. The program's entry (main), the
// memory and the file system are the engine's (deki-engine-hosted), as for any
// game that runs as a program.

#include "AnbernicPackage.h"
#include <deki/interop/Plugin.h>
#include <deki/Time.h>
#include <deki/providers/ITimeProvider.h>
#include <deki/reflection/ComponentRegistry.h>
#include <deki/reflection/ComponentFactory.h>

extern void DekiAnbernicRegisterComponents();
extern int DekiAnbernicGetAutoComponentCount();
extern const Deki::ComponentMeta* DekiAnbernicGetAutoComponentMeta(int index);

namespace DekiAnbernic
{

#ifdef DEKI_EDITOR

static bool s_AnbernicRegistered = false;

// The exports below are C symbols at global scope; the package's own
// registration helpers and statics live in its namespace.
using namespace DekiAnbernic;

extern "C"
{
    // Registers the package's components once. Returns how many there are.
    DEKI_ANBERNIC_API int DekiAnbernicEnsureRegistered(void)
    {
        if (s_AnbernicRegistered)
        {
            return ::DekiAnbernicGetAutoComponentCount();
        }
        s_AnbernicRegistered = true;

        // Generated: registers every Anbernic component with ComponentRegistry and ComponentFactory.
        ::DekiAnbernicRegisterComponents();

        return ::DekiAnbernicGetAutoComponentCount();
    }

    // =============================================================================
    // Plugin metadata
    // =============================================================================

    DEKI_PLUGIN_API const char* DekiPluginGetName(void)
    {
        return "Deki Anbernic Package";
    }

    DEKI_PLUGIN_API const char* DekiPluginGetVersion(void)
    {
#ifdef DEKI_PACKAGE_VERSION
        return DEKI_PACKAGE_VERSION;
#else
        return "0.0.0-dev";
#endif
    }

    DEKI_PLUGIN_API int DekiPluginInit(void)
    {
        return 0;
    }

    DEKI_PLUGIN_API void DekiPluginShutdown(void)
    {
        s_AnbernicRegistered = false;
        Deki::Time::SetTimeProvider(nullptr);
    }

    DEKI_PLUGIN_API int DekiPluginGetComponentCount(void)
    {
        return ::DekiAnbernicGetAutoComponentCount();
    }

    DEKI_PLUGIN_API const Deki::ComponentMeta* DekiPluginGetComponentMeta(int index)
    {
        return ::DekiAnbernicGetAutoComponentMeta(index);
    }

    DEKI_PLUGIN_API void DekiPluginRegisterComponents(void)
    {
        DekiAnbernicEnsureRegistered();
    }

    // =============================================================================
    // Package-specific API, with names that do not clash when DLLs link each other
    // =============================================================================

    DEKI_ANBERNIC_API const char* DekiAnbernicGetName(void)
    {
        return "Anbernic";
    }

}  // extern "C"

#else  // !DEKI_EDITOR - Runtime registration

// Components are registered by the generated ::DekiAnbernicRegisterComponents(),
// called from DekiRegisterProjectPackages(). Display and input run as boot
// SetupComponents, and a static initializer in AnbernicDisplay.cpp registers the
// Anbernic time provider.

#endif  // DEKI_EDITOR
}  // namespace DekiAnbernic
