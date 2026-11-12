// The handheld's buttons as Deki keys: which key each evdev code presses,
// which button quits, and when the d-pad's hat or the stick counts as pushed.
//
// A mistake here shows up on the handheld as a button that does nothing, the
// wrong one, or a game that quits on its own.

#include <gtest/gtest.h>

#include "AnbernicInput.h"
#include "Keys.h"  // from deki-input

using DekiAnbernic::AnbernicInput;
namespace Keys = DekiInput::Keys;

TEST(AnbernicInput, EachButtonPressesItsKey)
{
    EXPECT_EQ(AnbernicInput::KeyForCode(304), Keys::Enter);      // A
    EXPECT_EQ(AnbernicInput::KeyForCode(311), Keys::Enter);      // Start
    EXPECT_EQ(AnbernicInput::KeyForCode(305), Keys::Esc);        // B
    EXPECT_EQ(AnbernicInput::KeyForCode(307), Keys::Space);      // X
    EXPECT_EQ(AnbernicInput::KeyForCode(306), Keys::Tab);        // Y
    EXPECT_EQ(AnbernicInput::KeyForCode(310), Keys::Backspace);  // Select
    EXPECT_EQ(AnbernicInput::KeyForCode(308), Keys::Q);          // L1
    EXPECT_EQ(AnbernicInput::KeyForCode(309), Keys::E);          // R1
    EXPECT_EQ(AnbernicInput::KeyForCode(314), Keys::Z);          // L2
    EXPECT_EQ(AnbernicInput::KeyForCode(315), Keys::C);          // R2
}

TEST(AnbernicInput, TheDPadButtonsPressTheArrows)
{
    EXPECT_EQ(AnbernicInput::KeyForCode(103), Keys::Up);
    EXPECT_EQ(AnbernicInput::KeyForCode(108), Keys::Down);
    EXPECT_EQ(AnbernicInput::KeyForCode(105), Keys::Left);
    EXPECT_EQ(AnbernicInput::KeyForCode(106), Keys::Right);
}

// Menu goes back to the firmware's menu and presses no key; the 354 a short
// press of it also sends presses nothing either.
TEST(AnbernicInput, MenuQuitsAndPressesNoKey)
{
    EXPECT_TRUE(AnbernicInput::IsQuitCode(312));
    EXPECT_EQ(AnbernicInput::KeyForCode(312), 0u);
    EXPECT_FALSE(AnbernicInput::IsQuitCode(354));
    EXPECT_EQ(AnbernicInput::KeyForCode(354), 0u);
    for (uint16_t code : { 103, 304, 305, 311 })
    {
        EXPECT_FALSE(AnbernicInput::IsQuitCode(code)) << code;
    }
}

TEST(AnbernicInput, TheHatAndTheStickPressTheArrows)
{
    // ABS_X, ABS_Z (the stock firmware's stick) and ABS_HAT0X across;
    // ABS_Y, ABS_RX and ABS_HAT0Y up and down.
    for (uint16_t code : { 0, 2, 16 })
    {
        uint32_t low = 0;
        uint32_t high = 0;
        ASSERT_TRUE(AnbernicInput::KeysForAxis(code, &low, &high)) << code;
        EXPECT_EQ(low, Keys::Left) << code;
        EXPECT_EQ(high, Keys::Right) << code;
    }
    for (uint16_t code : { 1, 3, 17 })
    {
        uint32_t low = 0;
        uint32_t high = 0;
        ASSERT_TRUE(AnbernicInput::KeysForAxis(code, &low, &high)) << code;
        EXPECT_EQ(low, Keys::Up) << code;
        EXPECT_EQ(high, Keys::Down) << code;
    }
    uint32_t low = 0;
    uint32_t high = 0;
    EXPECT_FALSE(AnbernicInput::KeysForAxis(40, &low, &high));
}

TEST(AnbernicInput, AHatCountsAnyPush)
{
    EXPECT_EQ(AnbernicInput::DirectionOf(-1, -1, 1), -1);
    EXPECT_EQ(AnbernicInput::DirectionOf(1, -1, 1), 1);
    EXPECT_EQ(AnbernicInput::DirectionOf(0, -1, 1), 0);
}

// A stick points somewhere only past half of either half of its range, so a
// stick resting a little off centre presses nothing.
TEST(AnbernicInput, AStickCountsOnlyPastHalfWay)
{
    EXPECT_EQ(AnbernicInput::DirectionOf(0, -4096, 4096), 0);
    EXPECT_EQ(AnbernicInput::DirectionOf(1000, -4096, 4096), 0);
    EXPECT_EQ(AnbernicInput::DirectionOf(-1000, -4096, 4096), 0);
    EXPECT_EQ(AnbernicInput::DirectionOf(3000, -4096, 4096), 1);
    EXPECT_EQ(AnbernicInput::DirectionOf(-3000, -4096, 4096), -1);
}

// An axis whose range the device did not report points nowhere.
TEST(AnbernicInput, AnAxisWithNoRangePointsNowhere)
{
    EXPECT_EQ(AnbernicInput::DirectionOf(5, 5, 5), 0);
    EXPECT_EQ(AnbernicInput::DirectionOf(100, 0, 0), 0);
    EXPECT_EQ(AnbernicInput::DirectionOf(3, 9, 1), 0);
}
