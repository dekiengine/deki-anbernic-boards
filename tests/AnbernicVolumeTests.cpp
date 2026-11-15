// The handheld's volume: the stock launcher's level read from its settings
// file, and the sound chip's line-out value for each level.

#include <gtest/gtest.h>

#include "AnbernicVolume.h"

#include <vector>

using namespace DekiAnbernic;

TEST(AnbernicVolume, TheLevelsSpreadOverTheControl)
{
    // The line-out control: 31 steps of 1.5 dB, 0 silent.
    EXPECT_EQ(AnbernicVolume::ControlValueForLevel(10, 31), 31);
    EXPECT_EQ(AnbernicVolume::ControlValueForLevel(9, 31), 28);
    EXPECT_EQ(AnbernicVolume::ControlValueForLevel(1, 31), 4);
    EXPECT_EQ(AnbernicVolume::ControlValueForLevel(0, 31), 0);
    EXPECT_EQ(AnbernicVolume::ControlValueForLevel(-3, 31), 0);
    EXPECT_EQ(AnbernicVolume::ControlValueForLevel(14, 31), 31);
    // A smaller control never reaches silence before level 0.
    EXPECT_EQ(AnbernicVolume::ControlValueForLevel(1, 20), 1);
}

TEST(AnbernicVolume, TheLauncherLevelIsReadFromItsSettings)
{
    std::vector<unsigned char> file(140, 0);
    file[36] = 7;
    EXPECT_EQ(AnbernicVolume::LevelFromLauncherSettings(file.data(), file.size()), 7);

    file[36] = 11;  // past the launcher's scale: not a file this reads
    EXPECT_EQ(AnbernicVolume::LevelFromLauncherSettings(file.data(), file.size()), -1);
    file[36] = 3;
    file[39] = 0x80;  // negative
    EXPECT_EQ(AnbernicVolume::LevelFromLauncherSettings(file.data(), file.size()), -1);
    file[39] = 0;
    EXPECT_EQ(AnbernicVolume::LevelFromLauncherSettings(file.data(), 139), -1);
    EXPECT_EQ(AnbernicVolume::LevelFromLauncherSettings(nullptr, 140), -1);
}

TEST(AnbernicVolume, ANewLevelGetsTheLaunchersChecksum)
{
    // The launcher's own file from a handheld, at level 10.
    const unsigned char kFile[140] = {
        0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 1, 0, 0, 0, 5,  0,   0,   0,
        4, 0, 0, 0, 1,  0,  0,  0,  10, 0,  0,  0,  1,  0,  0,  0, 2, 0, 0, 0, 1, 0, 0, 0, 0,  0,   0,   0,
        1, 0, 0, 0, 77, 79, 79, 78, 67, 72, 73, 95, 50, 46, 52, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,   0,   0,
        0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,   0,   0,
        0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 1, 0, 0, 0, 93, 207, 174, 254,
    };
    std::vector<unsigned char> file(kFile, kFile + 140);
    ASSERT_EQ(AnbernicVolume::LevelFromLauncherSettings(file.data(), file.size()), 10);
    // Its checksum is the one this computes.
    EXPECT_EQ(AnbernicVolume::LauncherChecksum(file.data(), 136), 0xFEAECF5Du);

    // Level 2 gives the checksum the launcher itself wrote for level 2.
    ASSERT_TRUE(AnbernicVolume::SetLevelInLauncherSettings(file.data(), file.size(), 2));
    EXPECT_EQ(file[36], 2);
    EXPECT_EQ(file[136], 0x37);
    EXPECT_EQ(file[137], 0xF7);
    EXPECT_EQ(file[138], 0xB2);
    EXPECT_EQ(file[139], 0xE4);

    EXPECT_FALSE(AnbernicVolume::SetLevelInLauncherSettings(file.data(), file.size(), 11));
    EXPECT_FALSE(AnbernicVolume::SetLevelInLauncherSettings(file.data(), 100, 3));
}
