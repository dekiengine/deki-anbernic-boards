#pragma once

#include <cstdint>

namespace DekiAnbernic
{

class AnbernicDisplay;

/// Whether this run measures where its frames' time goes: when a file named
/// "profile" is in the game's folder on the card, beside DekiGame.
bool ProfileRequested();

/// Writes to the log, every 2 seconds, how long frames took and where the
/// time went: the engine's update, render and present, what each kind of
/// component and each blit path cost to draw (deki-rendering's
/// RenderProfiler), the objects that cost most, and how long `display`'s own
/// thread took to copy a frame to the panel. Timing every draw costs a
/// little, so it runs only when ProfileRequested.
void StartProfile(AnbernicDisplay* display);

/// Whether StartProfile turned the report on.
bool Profiling();

/// Once a frame, from the display's Present on the game's thread: counts the
/// frame and writes the report when 2 seconds have passed.
void ProfileFrame();

/// Microseconds, monotonic.
uint64_t ProfileNowUs();

}  // namespace DekiAnbernic
