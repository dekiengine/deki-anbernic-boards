#pragma once

#include <cstdint>

namespace DekiAnbernic
{

class AnbernicDisplay;

/// Whether this run measures where its frames' time goes: when a file named
/// "profile" is in the game's folder on the card, beside DekiGame. When that
/// file says "trace", every frame is also written to trace.txt beside it:
/// when the game handed it over, its time step, where the camera was in
/// screen pixels, and when it went up on the panel.
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
/// frame (`id`, counted by the display) and writes the report when 2 seconds
/// have passed.
void ProfileFrame(uint64_t id);

/// From the display's own thread: frame `id` went up on the panel at `us`.
void ProfileShown(uint64_t id, uint64_t us);

/// Microseconds, monotonic.
uint64_t ProfileNowUs();

}  // namespace DekiAnbernic
