#pragma once

// Installing a built app on the handheld over Wi-Fi, with OpenSSH's ssh and
// scp, so the SD card can stay in the handheld.
//
// The handheld runs an SSH server: on the stock firmware, the Modified System
// add-on's SSH Service. The editor logs in with this computer's key, never a
// password (ssh runs with BatchMode, so it cannot stop to ask for one), and
// says how to set the key up when the login is refused.
//
// An install does what one onto the card does (StockApp.h): the app folder is
// copied beside the old one and every file is checked against the build
// before it replaces the old app, so a dropped connection leaves the old one
// whole; the script goes last; an app the editor did not install is left
// alone.

#include <deki-editor/build/TargetBuilder.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <string>

namespace DekiEditor
{

/// Who to log in as, and where.
struct SshTarget
{
    std::string user = "root";
    std::string host;
};

/// The deploy target id prefix for an install over SSH: "ssh:root@192.168.0.133".
inline constexpr const char* kSshTargetPrefix = "ssh:";

/// "ssh:user@host" (or "ssh:host", user root) as a target. False when it is
/// not one, or names a user or host that is not plain (SshNameIsPlain).
bool ParseSshTarget(const std::string& id, SshTarget& out);

/// The deploy target id for `target`.
std::string SshTargetId(const SshTarget& target);

/// Letters, digits and . _ -, and a host's : for IPv6: nothing a shell or
/// ssh reads as syntax.
bool SshNameIsPlain(const std::string& name);

/// A folder on the handheld such as "/mnt/mmc": absolute, and plain
/// (letters, digits, / . _ -, no "..").
bool SshPathIsPlain(const std::string& path);

/// ssh or scp on this machine (Windows' own OpenSSH first), or empty.
std::string FindSshTool(const std::string& name);

/// The command line that runs `remote` on the handheld.
std::string SshCommandLine(const std::string& ssh, const SshTarget& target, const std::string& remote);

/// The command line that copies `local` (a folder when `folder`) to
/// `remotePath` on the handheld.
std::string ScpCommandLine(const std::string& scp, const SshTarget& target, const std::filesystem::path& local,
                           const std::string& remotePath, bool folder);

/// `sha256sum` output ("<hash>  ./path" per line) as path -> hash, the paths
/// without "./".
std::map<std::string, std::string> ParseSha256Sums(const std::string& output);

/// What to tell the user when ssh failed with `output`: why, and what to do.
std::string ExplainSshFailure(const std::string& output, const SshTarget& target);

/// Installs the app in `appsDir` (the build's APPS/, as AssembleStockApp
/// made it) into <cardPath>/Roms/APPS on the handheld. `onProgress` gets
/// 0..1. False, with `error` set, on any problem.
bool InstallStockAppOverSsh(const std::filesystem::path& appsDir, const SshTarget& target, const std::string& cardPath,
                            const std::string& folder, const BuildOutputCallback& log,
                            const std::function<void(float, const std::string&)>& onProgress,
                            const std::atomic<bool>* cancel, std::string& error);

}  // namespace DekiEditor
