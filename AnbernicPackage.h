#pragma once

// Main header of the Anbernic package. The package provides display setup (the
// Linux framebuffer, /dev/fb0) and input setup (the buttons, an evdev device)
// for Anbernic handhelds.

#ifdef _WIN32
#ifdef DEKI_ANBERNIC_EXPORTS
#define DEKI_ANBERNIC_API __declspec(dllexport)
#else
#define DEKI_ANBERNIC_API __declspec(dllimport)
#endif
#else
#define DEKI_ANBERNIC_API __attribute__((visibility("default")))
#endif
