#pragma once

namespace DekiAnbernic
{

/// The handheld's volume while a game runs. The stock launcher keeps its own
/// level, 0 to 10, but does not set the sound chip for an app, so the game
/// starts at the launcher's level, and the volume buttons move it a step at a
/// time on the sound chip's line-out control and in the launcher's settings,
/// so the launcher shows the new level too. On quit the chip goes back to how
/// the game found it.
class AnbernicVolume
{
public:
    /// The launcher's scale.
    static constexpr int kMaxLevel = 10;

    /// A fade's full value: the level as it is.
    static constexpr int kFadeFull = 256;

    /// Reads the launcher's level and mutes the chip, before the sound device
    /// opens: the amplifier then powers up silent, and SetFade brings the
    /// level in. Does nothing where the chip has no such control.
    static void BeginMuted();

    /// Scales the level by `fade` out of kFadeFull: the sound thread fades in
    /// after the device starts and out before it closes, so neither pops.
    static void SetFade(int fade);

    /// One step up (+1) or down (-1). False when the chip has no control.
    static bool Step(int direction);

    /// The level now, 0..kMaxLevel.
    static int GetLevel();

    /// Puts the chip back as the game found it. Called after the device is
    /// closed.
    static void Restore();

    /// The chip's line-out value for `level`: 0 is silent, kMaxLevel full, and
    /// each step 4.5 dB, the control going in steps of 1.5 dB.
    static long ControlValueForLevel(int level, long controlMax);

    /// The level in the launcher's settings file (an int at byte 36 of its 140
    /// bytes), or -1 when the file is not one this reads.
    static int LevelFromLauncherSettings(const unsigned char* data, unsigned long size);

    /// Puts `level` in a copy of the launcher's settings file, with the
    /// checksum its last 4 bytes hold: a CRC-32 (reflected, starting at 0,
    /// with no final XOR) of the 136 bytes before it. False when the file is
    /// not one this reads.
    static bool SetLevelInLauncherSettings(unsigned char* data, unsigned long size, int level);

    /// The launcher's checksum of `size` bytes.
    static unsigned long LauncherChecksum(const unsigned char* data, unsigned long size);
};

}  // namespace DekiAnbernic
