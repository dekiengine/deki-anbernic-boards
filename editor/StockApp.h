#pragma once

// A Deki game as an app of Anbernic's stock firmware, and getting it onto the
// handheld's SD card.
//
// The stock menu's Apps list shows the shell scripts in Roms/APPS/ on the
// card and runs the one picked; folders there stay hidden. So a built game is
// a script and a folder beside it:
//
//   Roms/APPS/
//     <Folder>.sh        cd to <Folder>, run the game, its output to dekigame.out
//     <Folder>/
//       DekiGame         the game, a static AArch64 Linux executable
//       flash/           the boot payload and assets (F:/ on the device)
//       .deki-app        says the editor made this folder, so it may replace it
//     Imgs/<Folder>.png  the Apps list's picture of it
//
// The names come from the project's. Installing replaces those three, and
// only when the editor made them: an app of the same name installed some
// other way is left alone, and the install says so.

#include <deki-editor/build/TargetBuilder.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace DekiEditor
{

/// The file that marks an app folder as the editor's.
inline constexpr const char* kStockAppMarker = ".deki-app";

/// The line every launch script the editor writes has, so an install knows
/// the script is its own to replace.
inline constexpr const char* kStockAppScriptMark = "# Made with Deki: the Deki Editor replaces this script";

/// A project name as an app name: letters, digits, '-' and '_' kept, anything
/// else made '_', at most 48 characters. "DekiGame" when nothing is left.
std::string StockAppFolderName(const std::string& projectName);

/// <Folder>.sh, with LF line endings (the handheld's shell reads a CR as part
/// of the command).
std::string StockAppLaunchScript(const std::string& label, const std::string& folder);

/// What an app is made from.
struct StockAppSources
{
    std::filesystem::path executable;  // the built DekiGame
    std::filesystem::path payload;     // the boot payload directory, which becomes flash/
    std::filesystem::path icon;        // the Apps list's picture
    std::string label;                 // the project's name
    std::string folder;                // StockAppFolderName of the project
    std::string projectId;             // StockAppProjectId of the project, kept in the marker
};

/// An id for the project at `projectPath`, the same for as long as its folder
/// stays where it is (a hash of the path, so the card does not carry the
/// computer's folder names). The app's marker holds it, so an install can
/// tell the project's app under an earlier name from another project's.
std::string StockAppProjectId(const std::filesystem::path& projectPath);

/// The app folders in the card's Roms/APPS, other than `folder`, that the
/// editor installed for the project `projectId`: its app under an earlier
/// name, which a rename leaves behind. Sorted.
std::vector<std::string> EarlierAppsOfProject(const std::filesystem::path& cardRoot, const std::string& projectId,
                                              const std::string& folder);

/// Makes <appsRoot>/<folder>.sh, <appsRoot>/<folder>/ and
/// <appsRoot>/Imgs/<folder>.png from `sources`. appsRoot is the build's own
/// APPS/ directory, emptied first so it holds this one app. False, with
/// `error` set, when something is missing or cannot be written.
bool AssembleStockApp(const StockAppSources& sources, const std::filesystem::path& appsRoot, std::string& error);

/// Whether `root` is an Anbernic stock firmware's SD card: Roms/APPS/ at the
/// top, where the stock menu finds its apps.
bool IsAnbernicCard(const std::filesystem::path& root);

/// The roots among `roots` that are Anbernic cards, in the order given.
std::vector<std::string> AnbernicCardsAmong(const std::vector<std::string>& roots);

/// Removable drives holding an Anbernic card, as deploy targets: the id is
/// the drive ("E:" on Windows, a mount point elsewhere), the label says what
/// it is.
std::vector<DeployTarget> FindAnbernicCards();

/// The directory an Anbernic card's deploy target id names ("E:" is "E:\").
std::filesystem::path AnbernicCardRoot(const std::string& deployTargetId);

/// Whether `given`, a deploy target as someone wrote it, names the card whose
/// id is `cardId`: the same drive or mount point, with or without a trailing
/// separator ("E:", "E:\" and "e:/" are one drive on Windows).
bool NamesAnbernicCard(const std::string& given, const std::string& cardId);

/// Copies the app in `appsDir` (the build's APPS/, as AssembleStockApp made
/// it) to <cardRoot>/Roms/APPS, replacing what is there when the editor made
/// it. The folder is copied into a temporary one beside it first and every
/// file checked against the original (size and SHA-256), so a card pulled out
/// halfway keeps the old app instead of half of the new one; the script and
/// the picture go last. `onProgress` gets 0..1. False, with `error` set, on
/// any problem.
bool InstallStockApp(const std::filesystem::path& appsDir, const std::filesystem::path& cardRoot,
                     const std::string& folder, const BuildOutputCallback& log,
                     const std::function<void(float, const std::string&)>& onProgress, const std::atomic<bool>* cancel,
                     std::string& error);

}  // namespace DekiEditor
