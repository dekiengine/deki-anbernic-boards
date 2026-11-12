#include "AnbernicToolchain.h"

#include <deki-editor/Localization.h>
#include <deki-editor/ShellCommand.h>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace DekiEditor
{

bool AnbernicHostHasPinnedCompiler()
{
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    return true;
#else
    return false;
#endif
}

std::string AnbernicToolchainDefinitionJson()
{
    nlohmann::json compiler = {
        { "id", kAnbernicCompilerComponent },
        { "displayName", "Arm GNU Toolchain (aarch64-none-linux-gnu)" },
        { "required", true },
        { "installPath", kAnbernicCompilerInstallPath },
        { "versionCheck", { { "type", "static" }, { "version", kAnbernicCompilerVersion } } },
    };

    if (AnbernicHostHasPinnedCompiler())
    {
        // Arm's archive for a Windows x86_64 host. It has no folder of its own
        // inside (bin/, lib/ and the rest are at the top), so it unpacks
        // straight into installPath. The digest is Arm's published .sha256asc.
        compiler["canInstall"] = true;
        compiler["fallback"] = {
            { "version", kAnbernicCompilerVersion },
            { "url", "https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/"
                     "arm-gnu-toolchain-15.2.rel1-mingw-w64-x86_64-aarch64-none-linux-gnu.zip" },
            { "sha256", "43c374920d33a96232466b53f46c723c8ca260a86680c69f2ef8fc0c3cb5c28e" },
        };
        compiler["detection"] = { { "windows", "{installPath}/bin/aarch64-none-linux-gnu-g++.exe" } };
        compiler["tooltip"] = "The cross compiler for Anbernic handhelds (64-bit ARM, Linux), from Arm";
    }
    else
    {
        // Nothing to download and nothing to detect: no archive is pinned
        // for this host, and taking whatever compiler is on PATH would build
        // with a toolchain nobody checked.
        compiler["canInstall"] = false;
        compiler["detection"] = nlohmann::json::object();
        compiler["tooltip"] = "No Arm GNU Toolchain archive is pinned for this host yet; Anbernic builds need a "
                              "Windows x86_64 machine for now";
    }

    nlohmann::json def = {
        { "id", "anbernic" },
        { "name", "Anbernic" },
        { "family", "linux-arm64" },
        { "components", nlohmann::json::array({ compiler }) },
    };
    return def.dump();
}

// The tooltips above, marked so the translation table has them. The Platforms
// panel shows them through Tr().
[[maybe_unused]] static constexpr const char* kAnbernicToolchainTooltips[] = {
    TrMark("The cross compiler for Anbernic handhelds (64-bit ARM, Linux), from Arm"),
    TrMark("No Arm GNU Toolchain archive is pinned for this host yet; Anbernic builds need a Windows x86_64 machine "
           "for now"),
};

// ============================================================================
// Host tools
// ============================================================================

namespace
{
#ifdef _WIN32
constexpr const char* kAnbernicExe = ".exe";
#else
constexpr const char* kAnbernicExe = "";
#endif
}  // namespace

std::string AnbernicFindOnPath(const std::string& name)
{
    const char* path = std::getenv("PATH");
    if (path == nullptr)
    {
        return "";
    }
#ifdef _WIN32
    const char separator = ';';
#else
    const char separator = ':';
#endif
    const std::string file = name + kAnbernicExe;
    std::stringstream dirs(path);
    std::string dir;
    while (std::getline(dirs, dir, separator))
    {
        if (dir.empty())
        {
            continue;
        }
        std::error_code ec;
        const fs::path candidate = fs::path(dir) / file;
        if (fs::is_regular_file(candidate, ec))
        {
            return candidate.generic_string();
        }
    }
    return "";
}

namespace
{
// The first line a program prints for `args`, or empty when it cannot run.
std::string AnbernicFirstLine(const std::string& program, const std::string& args)
{
    std::string first;
    const int code = RunShellCommand("\"" + program + "\" " + args, "",
                                     [&first](const std::string& line)
                                     {
                                         if (first.empty())
                                         {
                                             first = line;
                                         }
                                     });
    return code == 0 ? first : std::string();
}

// Where a tool may be when it is not on PATH: an editor started from the
// Start menu does not see a shell's PATH.
std::vector<std::string> AnbernicKnownLocations(const std::string& name)
{
    std::vector<std::string> out;
#ifdef _WIN32
    out.push_back("C:/msys64/mingw64/bin/" + name + kAnbernicExe);
    if (name == "cmake")
    {
        out.push_back("C:/Program Files/CMake/bin/cmake.exe");
    }
#else
    out.push_back("/usr/local/bin/" + name);
    out.push_back("/usr/bin/" + name);
#endif
    return out;
}

std::string AnbernicLocate(const std::string& name)
{
    std::string found = AnbernicFindOnPath(name);
    if (!found.empty())
    {
        return found;
    }
    for (const auto& candidate : AnbernicKnownLocations(name))
    {
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec))
        {
            return candidate;
        }
    }
    return "";
}
}  // namespace

int AnbernicHostTools::GccMajor(const std::string& dumpVersion)
{
    size_t i = 0;
    int major = 0;
    while (i < dumpVersion.size() && dumpVersion[i] >= '0' && dumpVersion[i] <= '9')
    {
        major = major * 10 + (dumpVersion[i] - '0');
        ++i;
    }
    return i == 0 ? -1 : major;
}

std::vector<AnbernicHostTool> AnbernicHostTools::Scan()
{
    std::vector<AnbernicHostTool> tools;

    AnbernicHostTool cmake;
    cmake.id = "cmake";
    cmake.displayName = "CMake";
    cmake.path = AnbernicLocate("cmake");
    cmake.ok = !cmake.path.empty();
    if (cmake.ok)
    {
        // "cmake version 3.31.2"
        const std::string line = AnbernicFirstLine(cmake.path, "--version");
        cmake.version = line.rfind("cmake version ", 0) == 0 ? line.substr(14) : line;
    }
    else
    {
        cmake.problem = "CMake was not found. Install CMake 3.16 or newer (cmake.org, or in MSYS2: pacman -S "
                        "mingw-w64-x86_64-cmake), put it on PATH and restart the editor.";
    }
    tools.push_back(cmake);

    AnbernicHostTool ninja;
    ninja.id = "ninja";
    ninja.displayName = "Ninja";
    ninja.path = AnbernicLocate("ninja");
    if (ninja.path.empty() && !cmake.path.empty())
    {
        // Often installed beside CMake.
        std::error_code ec;
        const fs::path beside = fs::path(cmake.path).parent_path() / (std::string("ninja") + kAnbernicExe);
        if (fs::is_regular_file(beside, ec))
        {
            ninja.path = beside.generic_string();
        }
    }
    ninja.ok = !ninja.path.empty();
    if (ninja.ok)
    {
        ninja.version = AnbernicFirstLine(ninja.path, "--version");
    }
    else
    {
        ninja.problem = "Ninja was not found. Install it (ninja-build.org, or in MSYS2: pacman -S "
                        "mingw-w64-x86_64-ninja), put it on PATH and restart the editor.";
    }
    tools.push_back(ninja);

    // The reflection codegen is compiled with -freflection, which GCC has
    // from 16. The generated CMake looks in the same places, in this order,
    // when it is not told which compiler to use; the builder always tells it.
    AnbernicHostTool gcc;
    gcc.id = "host-gcc";
    gcc.displayName = "Host GCC 16+ (reflection codegen)";
    std::vector<std::string> candidates;
#ifdef _WIN32
    candidates.push_back("C:/msys64/mingw64/bin/g++.exe");
#endif
    for (const char* name : { "g++-16", "g++" })
    {
        const std::string found = AnbernicFindOnPath(name);
        if (!found.empty())
        {
            candidates.push_back(found);
        }
    }
    std::string tooOld;
    for (const auto& candidate : candidates)
    {
        std::error_code ec;
        if (!fs::is_regular_file(candidate, ec))
        {
            continue;
        }
        const std::string version = AnbernicFirstLine(candidate, "-dumpfullversion");
        const int major = GccMajor(version);
        if (major >= 16)
        {
            gcc.path = candidate;
            gcc.version = version;
            gcc.ok = true;
            break;
        }
        if (tooOld.empty() && major > 0)
        {
            tooOld = candidate + " is GCC " + version;
        }
    }
    if (!gcc.ok)
    {
        gcc.problem = std::string("The reflection step needs GCC 16 or newer on this machine") +
                      (tooOld.empty() ? " and none was found" : " (" + tooOld + ")") +
#ifdef _WIN32
                      ". Install MSYS2's GCC (pacman -S mingw-w64-x86_64-gcc) into C:/msys64, or put a GCC 16 "
                      "g++ on PATH, and restart the editor.";
#else
                      ". Install g++-16 and restart the editor.";
#endif
    }
    tools.push_back(gcc);

    return tools;
}

std::vector<AnbernicHostTool> AnbernicHostTools::Get() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Scanned)
    {
        m_Tools = Scan();
        m_Scanned = true;
    }
    return m_Tools;
}

void AnbernicHostTools::Rescan()
{
    std::vector<AnbernicHostTool> tools = Scan();
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Tools = std::move(tools);
    m_Scanned = true;
}

AnbernicHostTool AnbernicHostTools::Find(const std::string& id) const
{
    for (const auto& tool : Get())
    {
        if (tool.id == id)
        {
            return tool;
        }
    }
    return {};
}

}  // namespace DekiEditor
