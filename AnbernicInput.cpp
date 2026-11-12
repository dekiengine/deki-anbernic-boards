#include "AnbernicInput.h"
#include "Keys.h"  // from deki-input

#include <deki/Time.h>

#if defined(__linux__) && !defined(DEKI_EDITOR)
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <deki/LogSystem.h>
#endif

namespace DekiAnbernic
{

namespace
{
// The buttons of Anbernic's H700 handhelds, as the stock firmware's kernel
// reports them (linux/input-event-codes.h names them after other buttons).
enum AnbernicCode : uint16_t
{
    kCodeUp = 103,
    kCodeLeft = 105,
    kCodeRight = 106,
    kCodeDown = 108,
    kCodeA = 304,       // BTN_SOUTH
    kCodeB = 305,       // BTN_EAST
    kCodeY = 306,       // BTN_C
    kCodeX = 307,       // BTN_NORTH
    kCodeL1 = 308,      // BTN_WEST
    kCodeR1 = 309,      // BTN_Z
    kCodeSelect = 310,  // BTN_TL
    kCodeStart = 311,   // BTN_TR
    kCodeMenu = 312,    // BTN_TL2; a short press also sends 354 (KEY_GOTO), left alone
    kCodeL2 = 314,      // BTN_SELECT
    kCodeR2 = 315,      // BTN_START
};

// Their axes: the d-pad is a hat, the left stick two axes.
enum AnbernicAxis : uint16_t
{
    kAxisX = 0,       // ABS_X, the left stick where a firmware reports it so
    kAxisY = 1,       // ABS_Y
    kAxisStickX = 2,  // ABS_Z, the left stick on the stock firmware
    kAxisStickY = 3,  // ABS_RX
    kAxisHatX = 16,   // ABS_HAT0X, the d-pad
    kAxisHatY = 17,   // ABS_HAT0Y
};
}  // namespace

uint32_t AnbernicInput::KeyForCode(uint16_t code)
{
    namespace Keys = DekiInput::Keys;
    switch (code)
    {
        case kCodeUp: return Keys::Up;
        case kCodeDown: return Keys::Down;
        case kCodeLeft: return Keys::Left;
        case kCodeRight: return Keys::Right;
        case kCodeA:
        case kCodeStart: return Keys::Enter;
        case kCodeB: return Keys::Esc;
        case kCodeX: return Keys::Space;
        case kCodeY: return Keys::Tab;
        case kCodeSelect: return Keys::Backspace;
        case kCodeL1: return Keys::Q;
        case kCodeR1: return Keys::E;
        case kCodeL2: return Keys::Z;
        case kCodeR2: return Keys::C;
        default: return 0;
    }
}

bool AnbernicInput::IsQuitCode(uint16_t code)
{
    return code == kCodeMenu;
}

bool AnbernicInput::KeysForAxis(uint16_t code, uint32_t* low, uint32_t* high)
{
    namespace Keys = DekiInput::Keys;
    switch (code)
    {
        case kAxisX:
        case kAxisStickX:
        case kAxisHatX:
            *low = Keys::Left;
            *high = Keys::Right;
            return true;
        case kAxisY:
        case kAxisStickY:
        case kAxisHatY:
            *low = Keys::Up;
            *high = Keys::Down;
            return true;
        default: return false;
    }
}

int8_t AnbernicInput::DirectionOf(int32_t value, int32_t low, int32_t high)
{
    if (high <= low)
    {
        return 0;
    }
    // Twice the distance from the centre, against the range: past half of
    // either half, the axis points that way.
    const int64_t offset = 2 * static_cast<int64_t>(value) - low - high;
    const int64_t range = static_cast<int64_t>(high) - low;
    if (2 * offset > range)
    {
        return 1;
    }
    if (2 * offset < -range)
    {
        return -1;
    }
    return 0;
}

AnbernicInput::~AnbernicInput()
{
    Shutdown();
}

void AnbernicInput::RegisterEventCallback(const DekiInput::InputEventCallback& callback)
{
    m_EventCallbacks.push_back(callback);
}

void AnbernicInput::Notify(const DekiInput::InputEvent& event)
{
    for (const auto& callback : m_EventCallbacks)
    {
        callback(event);
    }
}

bool AnbernicInput::GetPointerPosition(int32_t* x, int32_t* y) const
{
    (void)x;
    (void)y;
    return false;  // no touch screen, no mouse
}

bool AnbernicInput::IsKeyPressed(uint32_t key) const
{
    auto it = m_KeyDownCount.find(key);
    return it != m_KeyDownCount.end() && it->second > 0;
}

void AnbernicInput::Press(uint32_t key, bool down)
{
    int& count = m_KeyDownCount[key];
    const bool wasDown = count > 0;
    count = down ? count + 1 : (count > 0 ? count - 1 : 0);
    const bool isDown = count > 0;
    if (wasDown == isDown)
    {
        return;  // another button already holds this key
    }
    DekiInput::InputEvent event{};
    event.timestamp = Deki::Time::GetTime();
    event.type = isDown ? DekiInput::InputEventType::KeyDown : DekiInput::InputEventType::KeyUp;
    event.key = key;
    event.pressed = isDown;
    Notify(event);
}

void AnbernicInput::MoveAxis(Axis& axis, uint16_t code, int32_t value)
{
    uint32_t low = 0;
    uint32_t high = 0;
    if (!KeysForAxis(code, &low, &high))
    {
        return;
    }
    const int8_t direction = DirectionOf(value, axis.low, axis.high);
    if (direction == axis.direction)
    {
        return;
    }
    if (axis.direction != 0)
    {
        Press(axis.direction < 0 ? low : high, false);
    }
    if (direction != 0)
    {
        Press(direction < 0 ? low : high, true);
    }
    axis.direction = direction;
}

#if defined(__linux__) && !defined(DEKI_EDITOR)

namespace
{
constexpr int kMaxEventDevices = 16;
}

bool AnbernicInput::Initialize()
{
    if (m_Initialized)
    {
        return true;
    }

    // Every input device, by name. The buttons are the one the firmware
    // names ANBERNIC-keys; the others (the power key, headphones) are the
    // firmware's.
    std::vector<Device> found;
    for (int i = 0; i < kMaxEventDevices; ++i)
    {
        char path[32];
        std::snprintf(path, sizeof(path), "/dev/input/event%d", i);
        const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
        {
            continue;
        }
        char name[128] = {};
        ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);
        DEKI_LOG_INFO("AnbernicInput: %s is \"%s\"", path, name);
        Device device;
        device.fd = fd;
        device.name = name;
        found.push_back(std::move(device));
    }

    for (Device& device : found)
    {
        if (device.name == kDeviceName)
        {
            m_Devices.push_back(std::move(device));
            device.fd = -1;
        }
    }
    if (m_Devices.empty())
    {
        // Another firmware may name it otherwise: every device with a gamepad
        // button, so the game still has its buttons.
        DEKI_LOG_WARNING("AnbernicInput: no device is named %s; reading every device with gamepad buttons",
                         kDeviceName);
        for (Device& device : found)
        {
            unsigned long keys[(KEY_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
            ioctl(device.fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
            const unsigned long bits = 8 * sizeof(unsigned long);
            if ((keys[kCodeA / bits] >> (kCodeA % bits)) & 1ul)
            {
                m_Devices.push_back(std::move(device));
                device.fd = -1;
            }
        }
    }
    for (Device& device : found)
    {
        if (device.fd >= 0)
        {
            close(device.fd);
        }
    }
    if (m_Devices.empty())
    {
        DEKI_LOG_ERROR("AnbernicInput: no input device has the handheld's buttons");
        return false;
    }

    // The range of each axis, so a stick counts as pushed half way out
    // whatever numbers its device uses.
    for (Device& device : m_Devices)
    {
        for (uint16_t code = 0; code < 64; ++code)
        {
            input_absinfo info{};
            if (ioctl(device.fd, EVIOCGABS(code), &info) == 0)
            {
                device.axes[code].low = info.minimum;
                device.axes[code].high = info.maximum;
            }
        }
        DEKI_LOG_INFO("AnbernicInput: reading the buttons from \"%s\"", device.name.c_str());
    }
    m_Initialized = true;
    return true;
}

void AnbernicInput::Shutdown()
{
    for (Device& device : m_Devices)
    {
        if (device.fd >= 0)
        {
            close(device.fd);
        }
    }
    m_Devices.clear();
    m_KeyDownCount.clear();
    m_Initialized = false;
}

void AnbernicInput::Update()
{
    if (!m_Initialized)
    {
        return;
    }
    for (Device& device : m_Devices)
    {
        input_event ev{};
        while (read(device.fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev)))
        {
            if (ev.type == EV_ABS && ev.code < 64)
            {
                MoveAxis(device.axes[ev.code], ev.code, ev.value);
                continue;
            }
            // value 2 is the kernel's auto-repeat; the engine repeats on its own.
            if (ev.type != EV_KEY || ev.value == 2)
            {
                continue;
            }
            const bool down = ev.value != 0;
            if (IsQuitCode(ev.code))
            {
                if (down)
                {
                    DekiInput::InputEvent event{};
                    event.timestamp = Deki::Time::GetTime();
                    event.type = DekiInput::InputEventType::AppQuit;
                    Notify(event);
                }
                continue;
            }
            const uint32_t key = KeyForCode(ev.code);
            if (key == 0)
            {
                DEKI_LOG_INTERNAL("AnbernicInput: button %u %s, which presses no key", static_cast<unsigned>(ev.code),
                                  down ? "down" : "up");
                continue;
            }
            Press(key, down);
        }
    }
}

#else  // not on the device

bool AnbernicInput::Initialize()
{
    return false;
}
void AnbernicInput::Shutdown()
{
}
void AnbernicInput::Update()
{
}

#endif

}  // namespace DekiAnbernic
