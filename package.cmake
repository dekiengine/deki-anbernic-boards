# Package descriptor for deki-engine auto-discovery
set(PACKAGE_DISPLAY_NAME "Anbernic")
set(PACKAGE_PREFIX "DekiAnbernic")
set(PACKAGE_UPPER "ANBERNIC")
set(PACKAGE_TARGET "deki-anbernic")
set(PACKAGE_FILE_PREFIX "Anbernic")
set(PACKAGE_SOURCES
    AnbernicDisplaySetup.cpp
    AnbernicInputSetup.cpp
    AnbernicDisplay.cpp
    AnbernicRows.cpp
    AnbernicInput.cpp
)
set(PACKAGE_ENTRY AnbernicPackage.cpp)
set(PACKAGE_LINK_DEPS deki-input)
