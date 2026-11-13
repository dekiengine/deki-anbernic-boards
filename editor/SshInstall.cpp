#include "SshInstall.h"

#include "AnbernicToolchain.h"
#include "StockApp.h"

#include <deki-editor/EditorHttpUtils.h>
#include <deki-editor/ShellCommand.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace DekiEditor
{

namespace
{
// Runs a command line; everything it printed, a line at a time, in `output`.
int RunCollecting(const std::string& command, std::string& output, const std::atomic<bool>* cancel)
{
    output.clear();
    return RunShellCommand(command, "", [&output](const std::string& line) { output += line + "\n"; }, cancel);
}

bool Contains(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

std::string QuoteLocal(const fs::path& path)
{
    std::string s = path.string();
#ifdef _WIN32
    std::replace(s.begin(), s.end(), '/', '\\');
#endif
    return "\"" + s + "\"";
}
}  // namespace

bool SshNameIsPlain(const std::string& name)
{
    if (name.empty() || name.size() > 253 || name[0] == '-')
    {
        return false;
    }
    for (unsigned char c : name)
    {
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-' || c == ':'))
        {
            return false;
        }
    }
    return true;
}

bool SshPathIsPlain(const std::string& path)
{
    if (path.empty() || path[0] != '/' || path.find("..") != std::string::npos)
    {
        return false;
    }
    for (unsigned char c : path)
    {
        if (!(std::isalnum(c) || c == '/' || c == '.' || c == '_' || c == '-'))
        {
            return false;
        }
    }
    return true;
}

bool ParseSshTarget(const std::string& id, SshTarget& out)
{
    const std::string prefix = kSshTargetPrefix;
    if (id.rfind(prefix, 0) != 0)
    {
        return false;
    }
    const std::string rest = id.substr(prefix.size());
    SshTarget target;
    const size_t at = rest.find('@');
    if (at == std::string::npos)
    {
        target.host = rest;
    }
    else
    {
        target.user = rest.substr(0, at);
        target.host = rest.substr(at + 1);
    }
    if (!SshNameIsPlain(target.user) || !SshNameIsPlain(target.host))
    {
        return false;
    }
    out = target;
    return true;
}

std::string SshTargetId(const SshTarget& target)
{
    return std::string(kSshTargetPrefix) + target.user + "@" + target.host;
}

std::string FindSshTool(const std::string& name)
{
#ifdef _WIN32
    // Windows' own OpenSSH reads the user's ~/.ssh keys and needs nothing else.
    std::error_code ec;
    const fs::path own = fs::path("C:/Windows/System32/OpenSSH") / (name + ".exe");
    if (fs::is_regular_file(own, ec))
    {
        return own.string();
    }
#endif
    return AnbernicFindOnPath(name);
}

std::string SshCommandLine(const std::string& ssh, const SshTarget& target, const std::string& remote)
{
    // BatchMode: a key or nothing, never a password prompt nobody can answer.
    // accept-new: the handheld is trusted the first time, and a different one
    // at the same address is refused after.
    return "\"" + ssh + "\" -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new " + target.user +
           "@" + target.host + " \"" + remote + "\"";
}

std::string ScpCommandLine(const std::string& scp, const SshTarget& target, const fs::path& local,
                           const std::string& remotePath, bool folder)
{
    return "\"" + scp + "\" -q -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new " +
           std::string(folder ? "-r " : "") + QuoteLocal(local) + " " + target.user + "@" + target.host + ":" +
           remotePath;
}

std::map<std::string, std::string> ParseSha256Sums(const std::string& output)
{
    std::map<std::string, std::string> sums;
    std::istringstream in(output);
    for (std::string line; std::getline(in, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.size() < 67 || line[64] != ' ')
        {
            continue;
        }
        std::string path = line.substr(66);
        if (path.rfind("./", 0) == 0)
        {
            path = path.substr(2);
        }
        std::string hash = line.substr(0, 64);
        std::transform(hash.begin(), hash.end(), hash.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        sums[path] = hash;
    }
    return sums;
}

std::string ExplainSshFailure(const std::string& output, const SshTarget& target)
{
    const std::string who = target.user + "@" + target.host;
    if (Contains(output, "Permission denied"))
    {
        return "the handheld refused the login as " + who +
               ": this computer's SSH key is not on it. Add the key once (the package README says how: copy "
               "~/.ssh/id_ed25519.pub or id_rsa.pub into ~/.ssh/authorized_keys on the handheld, and make its home "
               "folder writable by its owner only), then install again";
    }
    if (Contains(output, "timed out") || Contains(output, "No route") || Contains(output, "unreachable") ||
        Contains(output, "Connection refused") || Contains(output, "Could not resolve"))
    {
        return "the handheld did not answer at " + target.host +
               ". Is it on, awake (a locked screen can drop Wi-Fi) and on this network, with its SSH service on? "
               "On the stock firmware with the Modified System add-on: Apps > Modified System Settings > SSH "
               "Service";
    }
    if (Contains(output, "REMOTE HOST IDENTIFICATION HAS CHANGED"))
    {
        return "the device at " + target.host +
               " is not the handheld this computer logged into before there (its key changed). If the handheld "
               "was reinstalled, remove its old line from ~/.ssh/known_hosts";
    }
    std::string last = output;
    while (!last.empty() && last.back() == '\n')
    {
        last.pop_back();
    }
    return "ssh to " + who + " failed" + (last.empty() ? std::string() : ": " + last);
}

bool InstallStockAppOverSsh(const fs::path& appsDir, const SshTarget& target, const std::string& cardPath,
                            const std::string& folder, const BuildOutputCallback& log,
                            const std::function<void(float, const std::string&)>& onProgress,
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
    if (!SshNameIsPlain(target.user) || !SshNameIsPlain(target.host))
    {
        error = "'" + target.user + "@" + target.host + "' is not a plain user and address";
        return false;
    }
    if (!SshPathIsPlain(cardPath))
    {
        error = "'" + cardPath + "' is not a plain folder on the handheld (such as /mnt/mmc)";
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
    const std::string ssh = FindSshTool("ssh");
    const std::string scp = FindSshTool("scp");
    if (ssh.empty() || scp.empty())
    {
        error = "ssh and scp were not found. Windows has them as the optional feature OpenSSH Client (Settings > "
                "System > Optional features); elsewhere install OpenSSH";
        return false;
    }

    const std::string apps = cardPath + "/Roms/APPS";
    const std::string staging = "." + folder + ".deki-install";
    std::string output;

    // The handheld answers, takes the key, and has the stock firmware's
    // apps; and what is there under this name is the editor's to replace.
    progress(0.0f, "Connecting to " + target.host);
    const std::string check = "cd " + apps + " || exit 3; if [ -e " + folder + " ] && [ ! -e " + folder + "/" +
                              kStockAppMarker + " ]; then echo DEKI_FOREIGN_FOLDER; fi; if [ -e " + folder +
                              ".sh ] && ! grep -q 'Made with Deki' " + folder +
                              ".sh; then echo DEKI_FOREIGN_SCRIPT; fi; echo DEKI_READY";
    int code = RunCollecting(SshCommandLine(ssh, target, check), output, cancel);
    if (cancelled())
    {
        error = "cancelled";
        return false;
    }
    if (code == 3)
    {
        error = target.host + " has no " + apps + ": is it the handheld's stock firmware, with its card at " +
                cardPath + "?";
        return false;
    }
    if (code != 0 || !Contains(output, "DEKI_READY"))
    {
        error = ExplainSshFailure(output, target);
        return false;
    }
    if (Contains(output, "DEKI_FOREIGN_FOLDER") || Contains(output, "DEKI_FOREIGN_SCRIPT"))
    {
        error = "Roms/APPS/" + folder +
                " on the handheld was not installed by the Deki Editor, so it is left alone. Rename the project, "
                "or move it off the handheld yourself";
        return false;
    }

    // The folder, beside the old one.
    progress(0.1f, "Copying " + folder);
    say("Copying " + folder + " to " + target.host);
    code = RunCollecting(SshCommandLine(ssh, target, "rm -rf " + apps + "/" + staging), output, cancel);
    if (code == 0)
    {
        code = RunCollecting(ScpCommandLine(scp, target, app, apps + "/" + staging, true), output, cancel);
    }
    if (cancelled())
    {
        RunCollecting(SshCommandLine(ssh, target, "rm -rf " + apps + "/" + staging), output, nullptr);
        error = "cancelled";
        return false;
    }
    if (code != 0)
    {
        error = "copying failed: " + ExplainSshFailure(output, target);
        return false;
    }

    // Every file read back on the handheld and checked against the build.
    progress(0.8f, "Checking the copy");
    code = RunCollecting(
        SshCommandLine(ssh, target, "cd " + apps + "/" + staging + " && find . -type f -exec sha256sum {} +"), output,
        cancel);
    const std::map<std::string, std::string> there = ParseSha256Sums(output);
    size_t checked = 0;
    for (auto it = fs::recursive_directory_iterator(app, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        std::error_code fileError;
        if (!it->is_regular_file(fileError))
        {
            continue;
        }
        const std::string rel = fs::relative(it->path(), app, fileError).generic_string();
        const auto found = there.find(rel);
        if (code != 0 || found == there.end() || found->second != EditorHttpUtils::Sha256File(it->path().string()))
        {
            RunCollecting(SshCommandLine(ssh, target, "rm -rf " + apps + "/" + staging), output, nullptr);
            error = rel + " did not arrive whole on the handheld; the old app is as it was";
            return false;
        }
        ++checked;
    }
    say("Checked " + std::to_string(checked) + " files on the handheld");

    // Swapped in, then the script and the picture, each through a file beside
    // it; the game is made runnable, and the card's writes flushed.
    progress(0.9f, "Putting it in place");
    const std::string swap = "cd " + apps + " && rm -rf " + folder + " && mv " + staging + " " + folder +
                             " && chmod +x " + folder + "/DekiGame";
    code = RunCollecting(SshCommandLine(ssh, target, swap), output, cancel);
    if (code == 0)
    {
        code = RunCollecting(ScpCommandLine(scp, target, script, apps + "/" + folder + ".sh.deki-install", false),
                             output, cancel);
    }
    if (code == 0)
    {
        code = RunCollecting(SshCommandLine(ssh, target,
                                            "cd " + apps + " && mv -f " + folder + ".sh.deki-install " + folder +
                                                ".sh && chmod +x " + folder + ".sh"),
                             output, cancel);
    }
    if (code != 0)
    {
        error = "putting the app in place failed: " + ExplainSshFailure(output, target);
        return false;
    }
    if (fs::is_regular_file(picture, ec))
    {
        RunCollecting(SshCommandLine(ssh, target, "mkdir -p " + apps + "/Imgs"), output, cancel);
        if (RunCollecting(ScpCommandLine(scp, target, picture, apps + "/Imgs/" + folder + ".png", false), output,
                          cancel) != 0)
        {
            say("The game is installed, but its picture is not: " + ExplainSshFailure(output, target));
        }
    }
    RunCollecting(SshCommandLine(ssh, target, "sync"), output, cancel);
    progress(1.0f, "Installed");
    return true;
}

}  // namespace DekiEditor
