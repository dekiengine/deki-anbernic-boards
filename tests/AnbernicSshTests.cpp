// Install over Wi-Fi: the parts that need no handheld. Which deploy targets
// are SSH ones, what may go into a command line, the command lines
// themselves, reading the handheld's checksums back, and what a failed login
// is explained as.

#include <gtest/gtest.h>

#include "editor/SshInstall.h"

using namespace DekiEditor;

TEST(AnbernicSsh, ATargetIdNamesTheUserAndTheAddress)
{
    SshTarget t;
    ASSERT_TRUE(ParseSshTarget("ssh:root@192.168.0.133", t));
    EXPECT_EQ(t.user, "root");
    EXPECT_EQ(t.host, "192.168.0.133");
    EXPECT_EQ(SshTargetId(t), "ssh:root@192.168.0.133");

    ASSERT_TRUE(ParseSshTarget("ssh:rg40xxh.local", t));
    EXPECT_EQ(t.user, "root") << "the stock firmware's user when none is given";
    EXPECT_EQ(t.host, "rg40xxh.local");

    EXPECT_FALSE(ParseSshTarget("E:", t)) << "a card is not an SSH target";
    EXPECT_FALSE(ParseSshTarget("ssh:", t));
    EXPECT_FALSE(ParseSshTarget("ssh:root@host;rm -rf /", t));
    EXPECT_FALSE(ParseSshTarget("ssh:-oProxyCommand=x@host", t)) << "nothing ssh would read as an option";
}

TEST(AnbernicSsh, OnlyPlainNamesAndFoldersGoIntoACommand)
{
    EXPECT_TRUE(SshNameIsPlain("192.168.0.133"));
    EXPECT_TRUE(SshNameIsPlain("fe80::1"));
    EXPECT_TRUE(SshNameIsPlain("my-handheld.local"));
    EXPECT_FALSE(SshNameIsPlain(""));
    EXPECT_FALSE(SshNameIsPlain("a b"));
    EXPECT_FALSE(SshNameIsPlain("x\"y"));
    EXPECT_FALSE(SshNameIsPlain("-x"));

    EXPECT_TRUE(SshPathIsPlain("/mnt/mmc"));
    EXPECT_TRUE(SshPathIsPlain("/mnt/sdcard"));
    EXPECT_FALSE(SshPathIsPlain("mnt/mmc")) << "not absolute";
    EXPECT_FALSE(SshPathIsPlain("/mnt/../etc"));
    EXPECT_FALSE(SshPathIsPlain("/mnt/mmc; reboot"));
}

TEST(AnbernicSsh, TheCommandsLogInWithAKeyOnly)
{
    SshTarget t;
    t.host = "192.168.0.133";
    const std::string ssh = SshCommandLine("ssh", t, "sync");
    EXPECT_NE(ssh.find("BatchMode=yes"), std::string::npos) << "never stops for a password";
    EXPECT_NE(ssh.find("root@192.168.0.133 \"sync\""), std::string::npos);

    const std::string scp = ScpCommandLine("scp", t, "C:/b/APPS/Farm", "/mnt/mmc/Roms/APPS/.Farm.deki-install", true);
    EXPECT_NE(scp.find("BatchMode=yes"), std::string::npos);
    EXPECT_NE(scp.find(" -r "), std::string::npos);
    EXPECT_NE(scp.find("root@192.168.0.133:/mnt/mmc/Roms/APPS/.Farm.deki-install"), std::string::npos);
    EXPECT_EQ(ScpCommandLine("scp", t, "C:/b/APPS/Farm.sh", "/x", false).find(" -r "), std::string::npos);
}

TEST(AnbernicSsh, TheHandheldsChecksumsAreReadByPath)
{
    const std::string a(64, 'a');
    const std::string b = "0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef";
    const auto sums = ParseSha256Sums(a + "  ./DekiGame\r\n" + b + "  ./flash/assets/x.bin\nnot a line\n");
    ASSERT_EQ(sums.size(), 2u);
    EXPECT_EQ(sums.at("DekiGame"), a);
    EXPECT_EQ(sums.at("flash/assets/x.bin"), "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
        << "lower case, as the build's are";
}

TEST(AnbernicSsh, AFailedLoginSaysWhatToDo)
{
    SshTarget t;
    t.host = "192.168.0.133";
    EXPECT_NE(ExplainSshFailure("root@192.168.0.133: Permission denied (publickey,password).", t).find("key"),
              std::string::npos);
    EXPECT_NE(
        ExplainSshFailure("ssh: connect to host 192.168.0.133 port 22: Connection timed out", t).find("did not answer"),
        std::string::npos);
    EXPECT_NE(ExplainSshFailure("something else", t).find("something else"), std::string::npos);
}
