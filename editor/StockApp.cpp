#include "StockApp.h"

#include <deki-editor/EditorHttpUtils.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace DekiEditor
{

namespace
{
bool WriteAppText(const fs::path& path, const std::string& text)
{
    // Binary, so a "\n" stays one byte on Windows.
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    return static_cast<bool>(out);
}

// Every regular file under `root`, relative to it.
std::vector<fs::path> AppFilesUnder(const fs::path& root, std::error_code& ec)
{
    std::vector<fs::path> files;
    for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        std::error_code fileError;
        if (it->is_regular_file(fileError))
        {
            files.push_back(fs::relative(it->path(), root, fileError));
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// Whether the file at `path` is a launch script the editor wrote.
bool IsOurScript(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    for (std::string line; std::getline(in, line);)
    {
        if (line.rfind(kStockAppScriptMark, 0) == 0)
        {
            return true;
        }
    }
    return false;
}

// Copies `from` to `to` through a file beside it and checks it, then puts it
// in place: the old file stays whole until the new one is known good.
bool CopyCheckedFile(const fs::path& from, const fs::path& to, std::string& error)
{
    std::error_code ec;
    const fs::path staging = to.string() + ".deki-install";
    fs::copy_file(from, staging, fs::copy_options::overwrite_existing, ec);
    std::error_code sizeError;
    const bool whole = !ec && fs::file_size(from, sizeError) == fs::file_size(staging, sizeError) && !sizeError &&
                       EditorHttpUtils::Sha256File(from.string()) == EditorHttpUtils::Sha256File(staging.string());
    if (!whole)
    {
        error = "cannot write " + to.filename().string() + " to the card" + (ec ? ": " + ec.message() : std::string());
        fs::remove(staging, ec);
        return false;
    }
    fs::remove(to, ec);
    fs::rename(staging, to, ec);
    if (ec)
    {
        error = "cannot put " + to.filename().string() + " in place on the card: " + ec.message();
        return false;
    }
    return true;
}
}  // namespace

std::string StockAppFolderName(const std::string& projectName)
{
    std::string out;
    for (unsigned char c : projectName)
    {
        const bool keep =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        out += keep ? static_cast<char>(c) : '_';
    }
    // Trimmed of the '_' that stood for spaces and the like at either end.
    const size_t first = out.find_first_not_of('_');
    if (first == std::string::npos)
    {
        return "DekiGame";
    }
    out = out.substr(first, out.find_last_not_of('_') - first + 1);
    if (out.size() > 48)
    {
        out.resize(48);
    }
    // Imgs/ beside the apps holds every app's picture, so no app may take
    // that name (the card's file system ignores case).
    std::string lower = out;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "imgs")
    {
        out += "_Game";
    }
    return out;
}

std::string StockAppLaunchScript(const std::string& label, const std::string& folder)
{
    // The label goes in a comment only; a newline in it would end the comment.
    std::string safeLabel = label;
    std::replace(safeLabel.begin(), safeLabel.end(), '\n', ' ');
    std::replace(safeLabel.begin(), safeLabel.end(), '\r', ' ');
    // The folder name is letters, digits, '-' and '_' (StockAppFolderName), so
    // it needs no quoting in the script.
    const std::string copy = "/tmp/" + folder + ".DekiGame";
    std::string s;
    s += "#!/bin/sh\n";
    s += std::string(kStockAppScriptMark) + " when it installs the game again.\n";
    s += "# Starts " + safeLabel + ". It runs from its folder, " + folder + "/ beside this script,\n";
    s += "# where it finds flash/ (its data) and writes its files; what it prints goes to dekigame.out.\n";
    s += "cd \"$(dirname \"$0\")/" + folder + "\" || exit 1\n";
    s += "chmod +x ./DekiGame 2>/dev/null\n";
    s += "./DekiGame > ./dekigame.out 2>&1\n";
    s += "code=$?\n";
    s += "# 126: the card does not let programs run from it, so from memory instead.\n";
    s += "if [ \"$code\" -eq 126 ]; then\n";
    s += "    cp ./DekiGame " + copy + " && chmod +x " + copy + "\n";
    s += "    " + copy + " > ./dekigame.out 2>&1\n";
    s += "    code=$?\n";
    s += "    rm -f " + copy + "\n";
    s += "fi\n";
    s += "sync\n";
    s += "exit $code\n";
    return s;
}

bool AssembleStockApp(const StockAppSources& sources, const fs::path& appsRoot, std::string& error)
{
    std::error_code ec;
    if (!fs::is_regular_file(sources.executable, ec))
    {
        error = "the game was not built: " + sources.executable.string() + " does not exist";
        return false;
    }
    if (!fs::is_regular_file(sources.payload / "project_data.bin", ec))
    {
        error = "the boot payload is missing (" + sources.payload.string() +
                " has no project_data.bin); build from the editor, which exports it";
        return false;
    }
    if (!fs::is_regular_file(sources.icon, ec))
    {
        error = "the app picture " + sources.icon.string() + " is missing; reinstall the deki-anbernic-boards package";
        return false;
    }

    // The build's own APPS/ folder: it holds the one app this build makes, so
    // an app left from before a project rename does not linger beside it.
    fs::remove_all(appsRoot, ec);
    const fs::path app = appsRoot / sources.folder;
    fs::create_directories(app, ec);
    if (!ec)
    {
        fs::create_directories(appsRoot / "Imgs", ec);
    }
    if (ec)
    {
        error = "cannot create " + app.string() + ": " + ec.message();
        return false;
    }

    fs::copy_file(sources.executable, app / "DekiGame", fs::copy_options::overwrite_existing, ec);
    if (!ec)
    {
        fs::copy(sources.payload, app / "flash", fs::copy_options::recursive, ec);
    }
    if (!ec)
    {
        fs::copy_file(sources.icon, appsRoot / "Imgs" / (sources.folder + ".png"), fs::copy_options::overwrite_existing,
                      ec);
    }
    if (ec)
    {
        error = "cannot copy the app's files into " + appsRoot.string() + ": " + ec.message();
        return false;
    }

    std::string marker =
        "Installed by the Deki Editor, which replaces this folder when it installs " + sources.label + " again.\n";
    if (!sources.projectId.empty())
    {
        marker += "project " + sources.projectId + "\n";
    }
    if (!WriteAppText(appsRoot / (sources.folder + ".sh"), StockAppLaunchScript(sources.label, sources.folder)) ||
        !WriteAppText(app / kStockAppMarker, marker))
    {
        error = "cannot write the app's files into " + appsRoot.string();
        return false;
    }
    return true;
}

std::string StockAppProjectId(const fs::path& projectPath)
{
    std::error_code ec;
    fs::path where = fs::weakly_canonical(projectPath, ec);
    if (ec)
    {
        where = fs::absolute(projectPath, ec);
    }
    std::string key = where.lexically_normal().generic_string();
    while (key.size() > 1 && key.back() == '/')
    {
        key.pop_back();
    }
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    uint64_t hash = 1469598103934665603ull;  // FNV-1a
    for (unsigned char c : key)
    {
        hash = (hash ^ c) * 1099511628211ull;
    }
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
    return text;
}

std::vector<std::string> EarlierAppsOfProject(const fs::path& cardRoot, const std::string& projectId,
                                              const std::string& folder)
{
    std::vector<std::string> earlier;
    if (projectId.empty())
    {
        return earlier;
    }
    const std::string line = "project " + projectId;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(cardRoot / "Roms" / "APPS", ec))
    {
        const std::string name = entry.path().filename().string();
        std::error_code inner;
        if (name == folder || name.empty() || name[0] == '.' || !entry.is_directory(inner))
        {
            continue;
        }
        std::ifstream in(entry.path() / kStockAppMarker, std::ios::binary);
        for (std::string text; std::getline(in, text);)
        {
            if (!text.empty() && text.back() == '\r')
            {
                text.pop_back();
            }
            if (text == line)
            {
                earlier.push_back(name);
                break;
            }
        }
    }
    std::sort(earlier.begin(), earlier.end());
    return earlier;
}

bool IsAnbernicCard(const fs::path& root)
{
    std::error_code ec;
    return fs::is_directory(root / "Roms" / "APPS", ec);
}

std::vector<std::string> AnbernicCardsAmong(const std::vector<std::string>& roots)
{
    std::vector<std::string> cards;
    for (const auto& root : roots)
    {
        if (IsAnbernicCard(root))
        {
            cards.push_back(root);
        }
    }
    return cards;
}

fs::path AnbernicCardRoot(const std::string& deployTargetId)
{
#ifdef _WIN32
    // "E:" on its own means the current directory on E:, not its root.
    if (deployTargetId.size() == 2 && deployTargetId[1] == ':')
    {
        return fs::path(deployTargetId + "\\");
    }
#endif
    return fs::path(deployTargetId);
}

namespace
{
// A card's directory written one way: forward slashes, no trailing one, and
// on Windows, where drive letters and paths ignore case, lower case.
std::string CardKey(const std::string& id)
{
    std::string key = AnbernicCardRoot(id).lexically_normal().generic_string();
    while (key.size() > 1 && key.back() == '/')
    {
        key.pop_back();
    }
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return key;
}
}  // namespace

bool NamesAnbernicCard(const std::string& given, const std::string& cardId)
{
    return !given.empty() && CardKey(given) == CardKey(cardId);
}

std::vector<DeployTarget> FindAnbernicCards()
{
    std::vector<DeployTarget> targets;
#ifdef _WIN32
    // Removable drives only: an SD card in a reader. A fixed disk that happens
    // to have the same folders is not a card.
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i)
    {
        if ((mask & (1u << i)) == 0)
        {
            continue;
        }
        const std::string id = std::string(1, static_cast<char>('A' + i)) + ":";
        const std::string root = id + "\\";
        if (GetDriveTypeA(root.c_str()) != DRIVE_REMOVABLE || !IsAnbernicCard(root))
        {
            continue;
        }
        char volume[MAX_PATH + 1] = {};
        std::string label = "Anbernic SD card (" + id;
        if (GetVolumeInformationA(root.c_str(), volume, sizeof(volume), nullptr, nullptr, nullptr, nullptr, 0) &&
            volume[0] != '\0')
        {
            label += " " + std::string(volume);
        }
        targets.push_back({ id, label + ")" });
    }
#else
    // Where desktops mount removable media: /media/<user>/<volume>,
    // /run/media/<user>/<volume>, /Volumes/<volume>.
    std::vector<std::string> roots;
    std::error_code ec;
    for (const char* parent : { "/Volumes", "/media", "/run/media" })
    {
        for (const auto& entry : fs::directory_iterator(parent, ec))
        {
            roots.push_back(entry.path().string());
            std::error_code inner;
            for (const auto& nested : fs::directory_iterator(entry.path(), inner))
            {
                roots.push_back(nested.path().string());
            }
        }
    }
    for (const auto& root : AnbernicCardsAmong(roots))
    {
        targets.push_back({ root, "Anbernic SD card (" + root + ")" });
    }
#endif
    return targets;
}

bool InstallStockApp(const fs::path& appsDir, const fs::path& cardRoot, const std::string& folder,
                     const BuildOutputCallback& log, const std::function<void(float, const std::string&)>& onProgress,
                     const std::atomic<bool>* cancel, std::string& error)
{
    auto say = [&](const std::string& line)
    {
        if (log)
        {
            log(line, false);
        }
    };
    auto progress = [&](float value, const std::string& text)
    {
        if (onProgress)
        {
            onProgress(value, text);
        }
    };
    auto cancelled = [&]() { return cancel != nullptr && cancel->load(); };

    std::error_code ec;
    if (folder.empty() || folder != StockAppFolderName(folder))
    {
        error = "'" + folder + "' is not an app name";
        return false;
    }
    const fs::path app = appsDir / folder;
    const fs::path script = appsDir / (folder + ".sh");
    const fs::path picture = appsDir / "Imgs" / (folder + ".png");
    if (!fs::is_regular_file(script, ec) || !fs::is_regular_file(app / "DekiGame", ec))
    {
        error = "there is no built app at " + appsDir.string() + "; build first";
        return false;
    }
    if (!IsAnbernicCard(cardRoot))
    {
        error = cardRoot.string() + " is not an Anbernic SD card (no Roms/APPS at its top)";
        return false;
    }

    const fs::path cardApps = cardRoot / "Roms" / "APPS";
    const fs::path dest = cardApps / folder;
    const fs::path destScript = cardApps / (folder + ".sh");
    // A hidden name of this app's own, beside it: only this install writes it.
    const fs::path staging = cardApps / ("." + folder + ".deki-install");
    if (fs::exists(dest, ec) && !fs::exists(dest / kStockAppMarker, ec))
    {
        error = "Roms/APPS/" + folder +
                " on the card was not installed by the Deki Editor, so it is left alone. "
                "Rename the project, or move that folder off the card yourself";
        return false;
    }
    if (fs::exists(destScript, ec) && !IsOurScript(destScript))
    {
        error = "Roms/APPS/" + folder +
                ".sh on the card was not written by the Deki Editor, so it is left alone. "
                "Rename the project, or move that script off the card yourself";
        return false;
    }

    const std::vector<fs::path> files = AppFilesUnder(app, ec);
    if (ec || files.empty())
    {
        error = "cannot read the built app at " + app.string() + (ec ? ": " + ec.message() : std::string());
        return false;
    }

    // A staging folder left by an install that was cut short.
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);
    if (ec)
    {
        error = "cannot write to the card (" + staging.string() + "): " + ec.message();
        return false;
    }

    auto fail = [&](const std::string& why)
    {
        error = why;
        std::error_code cleanup;
        fs::remove_all(staging, cleanup);
        return false;
    };

    // Copy, then check, file by file: the check reads back what landed on the
    // card, so a full or failing card is caught here rather than on the device.
    const float steps = static_cast<float>(files.size()) * 2.0f + 1.0f;
    float done = 0.0f;
    for (const auto& rel : files)
    {
        if (cancelled())
        {
            return fail("cancelled");
        }
        progress(done / steps, "Copying " + rel.generic_string());
        fs::create_directories((staging / rel).parent_path(), ec);
        fs::copy_file(app / rel, staging / rel, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            return fail("cannot copy " + rel.generic_string() + " to the card: " + ec.message());
        }
        done += 1.0f;
    }
    say("Copied " + std::to_string(files.size()) + " files; checking them");
    for (const auto& rel : files)
    {
        if (cancelled())
        {
            return fail("cancelled");
        }
        progress(done / steps, "Checking " + rel.generic_string());
        std::error_code sizeError;
        const auto want = fs::file_size(app / rel, sizeError);
        const auto have = fs::file_size(staging / rel, sizeError);
        if (sizeError || want != have)
        {
            return fail(rel.generic_string() + " did not copy whole (" + std::to_string(have) + " of " +
                        std::to_string(want) + " bytes); is the card full?");
        }
        if (EditorHttpUtils::Sha256File((app / rel).string()) != EditorHttpUtils::Sha256File((staging / rel).string()))
        {
            return fail(rel.generic_string() + " reads back different from what was written; the card may be failing");
        }
        done += 1.0f;
    }

    // Swap the checked copy in for the old app.
    if (fs::exists(dest, ec))
    {
        fs::remove_all(dest, ec);
        if (ec)
        {
            return fail("cannot remove the old Roms/APPS/" + folder + " from the card: " + ec.message());
        }
    }
    fs::rename(staging, dest, ec);
    if (ec)
    {
        // The old app is gone by now, so the checked copy stays where it is.
        error = "cannot move the new app into Roms/APPS/" + folder + ": " + ec.message() +
                ". The checked copy is Roms/APPS/" + staging.filename().string() + " on the card; rename it to " +
                folder + " by hand";
        return false;
    }

    // The script last, so the Apps list shows the game only once it is
    // whole; then its picture.
    progress(done / steps, "Writing " + folder + ".sh");
    if (!CopyCheckedFile(script, destScript, error))
    {
        return false;
    }
    if (fs::is_regular_file(picture, ec))
    {
        fs::create_directories(cardApps / "Imgs", ec);
        std::string pictureError;
        if (!CopyCheckedFile(picture, cardApps / "Imgs" / (folder + ".png"), pictureError))
        {
            say("The game is installed, but its picture is not: " + pictureError);
        }
    }
    progress(1.0f, "Installed");
    return true;
}

}  // namespace DekiEditor
