#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "IDekiInput.h"  // from deki-input

namespace DekiAnbernic
{

/// The handheld's buttons and left stick, read from their evdev device (named
/// "ANBERNIC-keys" by the stock firmware). Each button reports a Deki key, so
/// games written for a keyboard work unchanged:
///
///   D-pad    Up / Down / Left / Right     (the left stick too)
///   A        Enter          B       Esc
///   X        Space          Y       Tab
///   Start    Enter          Select  Backspace
///   L1       q              R1      e
///   L2       z              R2      c
///   Menu     quits the game, back to the firmware's menu
class AnbernicInput : public DekiInput::IDekiInput
{
public:
    /// The name the stock firmware gives the buttons' device.
    static constexpr const char* kDeviceName = "ANBERNIC-keys";

    // A direction a hat or a stick axis is pushed: -1, 0 or 1.
    struct Axis
    {
        int8_t direction = 0;
        int32_t low = 0;  // the axis' range, as the device reports it
        int32_t high = 0;
    };

private:
    struct Device
    {
        int fd = -1;
        std::string name;
        Axis axes[64];  // by ABS_ code
    };
    std::vector<Device> m_Devices;
    bool m_Initialized = false;
    std::vector<DekiInput::InputEventCallback> m_EventCallbacks;

    // How many buttons now hold each key down: Start and A both press Enter,
    // the d-pad and the stick both press the arrows.
    std::unordered_map<uint32_t, int> m_KeyDownCount;

    void Notify(const DekiInput::InputEvent& event);
    // A button or direction holding `key` went down or up.
    void Press(uint32_t key, bool down);
    // An axis moved: presses or releases the keys for its two ends.
    void MoveAxis(Axis& axis, uint16_t code, int32_t value);

public:
    AnbernicInput() = default;
    ~AnbernicInput() override;

    bool Initialize() override;
    void Shutdown() override;
    void Update() override;
    void RegisterEventCallback(const DekiInput::InputEventCallback& callback) override;
    bool IsInitialized() const override { return m_Initialized; }
    bool GetPointerPosition(int32_t* x, int32_t* y) const override;
    bool IsKeyPressed(uint32_t key) const override;

    /// The Deki key a button's evdev code reports; 0 for none.
    static uint32_t KeyForCode(uint16_t code);
    /// True for the button that quits the game (Menu).
    static bool IsQuitCode(uint16_t code);
    /// The Deki keys at the low and high ends of an axis (ABS_ code): the
    /// d-pad's hat and the left stick. False for an axis that presses nothing.
    static bool KeysForAxis(uint16_t code, uint32_t* low, uint32_t* high);
    /// Where an axis at `value` points, out of its range [low, high]: -1 past
    /// the first half towards low, 1 past the first half towards high, else 0.
    /// A hat's range is -1..1, so any push counts.
    static int8_t DirectionOf(int32_t value, int32_t low, int32_t high);
};

}  // namespace DekiAnbernic
