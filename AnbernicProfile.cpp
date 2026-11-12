#include "AnbernicProfile.h"

// The report reads the device's clock and deki-rendering's profiler, which a
// game has; the editor and other hosts compile an empty one.
#if defined(__linux__) && !defined(DEKI_EDITOR)

#include <algorithm>
#include <ctime>
#include <vector>

#include <unistd.h>

#include <deki/Engine.h>
#include <deki/LogSystem.h>
#include <deki/reflection/ComponentRegistry.h>
#include "AnbernicDisplay.h"
#include "RenderProfiler.h"  // from deki-rendering

namespace DekiAnbernic
{

namespace
{
constexpr uint64_t kWindowUs = 2000000;
constexpr size_t kItemsShown = 10;
constexpr size_t kPathsShown = 6;
constexpr size_t kObjectsShown = 8;

AnbernicDisplay* s_Display = nullptr;
bool s_On = false;
uint64_t s_WindowStartUs = 0;
uint64_t s_LastFrameUs = 0;
uint32_t s_Frames = 0;
uint64_t s_FrameUs = 0;
uint64_t s_LongestUs = 0;
uint32_t s_UpdateMs = 0;
uint32_t s_RenderMs = 0;
uint32_t s_PresentMs = 0;

const char* ItemName(const DekiRendering::RenderProfiler::Entry& e)
{
    namespace Profiler = DekiRendering::RenderProfiler;
    if (e.typeId == Profiler::kClear)
    {
        return "Clear";
    }
    if (e.name)
    {
        return e.name;
    }
    if (const Deki::ComponentMeta* meta = Deki::ComponentRegistry::Instance().GetMeta(e.typeId))
    {
        return meta->name;
    }
    return "?";
}

void Report(uint64_t windowUs)
{
    namespace Profiler = DekiRendering::RenderProfiler;
    const std::vector<Profiler::PathEntry> paths = Profiler::TakePaths();
    const std::vector<Profiler::ObjectEntry> objects = Profiler::TakeObjects();
    uint32_t renderFrames = 0;
    const std::vector<Profiler::Entry> items = Profiler::Take(renderFrames);

    const double frames = s_Frames > 0 ? s_Frames : 1;
    DEKI_LOG_INFO("Profile: %u frames in %.2f s (%.1f a second), %.2f ms apart on average, the longest %.2f ms; "
                  "the engine's update %.1f, render %.1f, present %.1f ms (in whole ms, averaged)",
                  static_cast<unsigned>(s_Frames), windowUs / 1e6, s_Frames * 1e6 / windowUs,
                  s_FrameUs / frames / 1000.0, s_LongestUs / 1000.0, s_UpdateMs / frames, s_RenderMs / frames,
                  s_PresentMs / frames);
    if (s_Display)
    {
        uint32_t draws = 0;
        const uint64_t drawUs = s_Display->TakeDrawUs(&draws);
        DEKI_LOG_INFO("Profile: the display's thread copied %u frames to the panel, %.2f ms each", draws,
                      draws > 0 ? drawUs / 1000.0 / draws : 0.0);
    }

    const double perFrame = renderFrames > 0 ? 1.0 / renderFrames : 0.0;
    for (size_t i = 0; i < items.size() && i < kItemsShown; ++i)
    {
        const Profiler::Entry& e = items[i];
        DEKI_LOG_INFO("Profile:   %-22s %.2f ms a frame (content %.2f, blit %.2f), %.1f draws, %.0f pixels",
                      ItemName(e), (e.contentUs + e.blitUs) * perFrame / 1000.0, e.contentUs * perFrame / 1000.0,
                      e.blitUs * perFrame / 1000.0, e.draws * perFrame, e.pixels * perFrame);
    }
    for (size_t i = 0; i < paths.size() && i < kPathsShown; ++i)
    {
        const Profiler::PathEntry& p = paths[i];
        DEKI_LOG_INFO("Profile:   path %-17s %.2f ms a frame, %.1f blits, %.0f pixels, %.1f ns a pixel",
                      p.name ? p.name : "?", p.us * perFrame / 1000.0, p.blits * perFrame, p.pixels * perFrame,
                      p.pixels ? 1000.0 * static_cast<double>(p.us) / static_cast<double>(p.pixels) : 0.0);
    }
    for (size_t i = 0; i < objects.size() && i < kObjectsShown; ++i)
    {
        const Profiler::ObjectEntry& o = objects[i];
        DEKI_LOG_INFO("Profile:   object %s (%s): %.2f ms a frame (changes %.2f, content %.2f, blit %.2f), "
                      "%.0f pixels",
                      o.path.c_str(), o.type ? o.type : "?", (o.measureUs + o.contentUs + o.blitUs) * perFrame / 1000.0,
                      o.measureUs * perFrame / 1000.0, o.contentUs * perFrame / 1000.0, o.blitUs * perFrame / 1000.0,
                      o.pixels * perFrame);
    }
}
}  // namespace

uint64_t ProfileNowUs()
{
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<uint64_t>(t.tv_sec) * 1000000u + static_cast<uint64_t>(t.tv_nsec) / 1000u;
}

bool ProfileRequested()
{
    return access("profile", F_OK) == 0;
}

void StartProfile(AnbernicDisplay* display)
{
    s_Display = display;
    s_On = true;
    DekiRendering::RenderProfiler::Enable(&ProfileNowUs);
    DEKI_LOG_INFO("Profile: on, as the file \"profile\" is beside the game; a report every 2 s");
}

bool Profiling()
{
    return s_On;
}

void ProfileFrame()
{
    if (!s_On)
    {
        return;
    }
    const uint64_t now = ProfileNowUs();
    if (s_LastFrameUs != 0)
    {
        const uint64_t gap = now - s_LastFrameUs;
        const Deki::Engine::FrameTimings t = Deki::Engine::GetInstance().GetLastFrameTimings();
        ++s_Frames;
        s_FrameUs += gap;
        s_LongestUs = std::max(s_LongestUs, gap);
        s_UpdateMs += t.update;
        s_RenderMs += t.render;
        s_PresentMs += t.present;
    }
    else
    {
        s_WindowStartUs = now;
    }
    s_LastFrameUs = now;
    if (now - s_WindowStartUs >= kWindowUs)
    {
        Report(now - s_WindowStartUs);
        s_Frames = 0;
        s_FrameUs = 0;
        s_LongestUs = 0;
        s_UpdateMs = 0;
        s_RenderMs = 0;
        s_PresentMs = 0;
        s_WindowStartUs = now;
    }
}

}  // namespace DekiAnbernic

#else  // not on the device

namespace DekiAnbernic
{

bool ProfileRequested()
{
    return false;
}
void StartProfile(AnbernicDisplay*)
{
}
bool Profiling()
{
    return false;
}
void ProfileFrame()
{
}
uint64_t ProfileNowUs()
{
    return 0;
}

}  // namespace DekiAnbernic

#endif
