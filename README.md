# Deki Anbernic Boards

Build target for the Anbernic RG40XX H. Install the package, pick **Anbernic RG40XX H** in the Build panel, and **Build** makes an app for the handheld's stock firmware; **Install** puts it on the handheld's SD card, where it shows in the Apps list.

| Target | Handheld | Screen | Input |
| --- | --- | --- | --- |
| `anbernic_rg40xx_h` | Anbernic RG40XX H | 640x480 ARGB8888, the panel's own size, through the Linux framebuffer | The buttons, the d-pad and the left stick, as keys, through `deki-input` |

The RG40XX H was provided by Anbernic. It has an Allwinner H700 (four Cortex-A53 cores) and 1 GB of memory, and its stock firmware is a 64-bit Linux. Other Anbernic handhelds with the same chip (RG35XX H, RG35XX Plus, RG40XX V, RG CubeXX and others) run the same firmware and are expected to follow as targets in this package.

[Deki](https://dekiengine.com) is a modular C++ engine with a visual editor, made first for embedded devices; it runs on the desktop too. Every capability is a package, and a handheld is one too: this package has the handheld's display and input, and the builder that makes and installs the app.

## Install

Package Manager in the Deki Editor, or `DekiEditor --packages-add deki-anbernic-boards <project>`. It brings `deki-input` with it. The game is a Linux program: its `main()`, its memory and its file system are the engine's own, as for any game built as a program (`F:/` is the app's `flash/` folder).

Then pick the target in the Build panel, or:

```
DekiEditor --tool set_active_platform --args '{"platform_id":"anbernic_rg40xx_h"}' <project>
```

## Toolchain

The Build panel lists what a build needs, with what to do for anything missing (`toolchain_status` says the same):

- **Arm GNU Toolchain 15.2.rel1** (`aarch64-none-linux-gnu`), Arm's cross compiler for 64-bit ARM Linux. **Install** downloads it from developer.arm.com (about 340 MB), checks it against Arm's published SHA-256 and puts it in `~/.deki-editor/toolchains/arm-gnu-toolchain/`. Only the Windows x86_64 archive is pinned so far; on another host the toolchain list says so and the build is refused.
- **CMake** and **Ninja**, found on PATH (or in `C:/msys64/mingw64/bin` and CMake's own install folder).
- **GCC 16 or newer for this machine**, which compiles the reflection step that runs before the game compiles (it needs `-freflection`). On Windows that is MSYS2's `mingw-w64-x86_64-gcc`.

The editor installs only the first; install the others yourself and restart the editor.

## What a build does

The game builds like the desktop build, as one executable, then cross-compiled:

- The toolchain file (`generated/build/anbernic_rg40xx_h/toolchain.cmake`) comes from the platform's `frameworkOptions`: `toolchainPrefix` (the compiler), `cpuFlags` (Cortex-A53) and `static`. The game is linked statically, so it does not depend on the libraries of the handheld's system.
- Only the packages the game uses and the platform can build are compiled, so nothing written for SDL3, ESP32 or Bluetooth reaches the ARM compiler.
- The platform's `cxxFlags` (`-O3`) are the optimisation, for the engine and the game alike; `linkFlags` has `-s`, which strips the symbols.
- Logging follows the Build panel's logging options.

The app is then in `generated/build/anbernic_rg40xx_h/APPS/`, named after the project:

| File | What it is |
| --- | --- |
| `<Name>.sh` | What the Apps list shows and runs: goes to `<Name>/` and starts the game, its output to `dekigame.out` there |
| `<Name>/DekiGame` | The game, a static AArch64 Linux executable |
| `<Name>/flash/` | The game's data: the project settings, the boot scene and the assets (`F:/` on the handheld) |
| `<Name>/.deki-app` | Says the editor made this folder, so a later Install may replace it |
| `Imgs/<Name>.png` | The Apps list's picture, the Deki cat, 240x180 like the stock apps' |

The game also writes `dekigame.log` (its own log) and the files it saves into its folder on the card.

## Install

**Install** copies the app to `Roms/APPS/` on the handheld's SD card: put the card in a card reader, and the target list shows each removable drive with `Roms/APPS/` at its top. The folder goes to a hidden one first and each file is read back and checked against the build before it replaces the old app, so a card pulled out halfway still has the old one; the script goes last, so the Apps list shows the game only once it is whole. Eject the card before taking it out.

Renaming the project (Project Settings > Project > Name) gives the app a new name. The old one stays on the card, as it may hold the game's saves, and the next Install says which it is; delete it from the card when it is no longer wanted.

Only the editor's own app is ever replaced. An app of the same name installed another way is left alone, and Install says so; rename the project, or move that app off the card.

Without Install: copy what is in `APPS/` into `Roms/APPS/` on the card yourself.

## Install over Wi-Fi

With the handheld on the same Wi-Fi, **Install** can put the game on it with the card still inside. Give the platform the handheld's address (Build panel, edit the platform, **Install over Wi-Fi > Address**); the target list then offers it beside the cards. From the command line, the target is `ssh:root@<address>`:

```
DekiEditor --tool deploy_build --args '{"target":"ssh:root@192.168.0.133"}' <project>
```

It copies over SSH with the computer's own `ssh` and `scp` (on Windows, the built-in OpenSSH Client), and does what an install onto the card does: the app goes to a hidden folder first, every file is checked against the build before it replaces the old app, the script goes last, and an app the editor did not install is left alone. Then start the game from the Apps list; if it was open, quit it with Menu first.

Once, on the handheld:

1. Turn on its SSH server. On the stock firmware with the Modified System add-on: **Apps > Modified System Settings > SSH Service > Enable** (its user is `root`, password `root`). "Enable Temporarily" turns it off again at the next restart.
2. Find its address, in its Wi-Fi settings or the router's list of devices.

Once, on the computer, add its SSH key to the handheld (the editor logs in with the key only, never a password). In a terminal, with the handheld's address, type `root` when asked:

```
ssh root@192.168.0.133 "mkdir -p ~/.ssh && chmod go-w ~ && chmod 700 ~/.ssh && echo '<your key>' >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys"
```

where `<your key>` is the one line in `~/.ssh/id_ed25519.pub` (or `id_rsa.pub`; `ssh-keygen` makes one if there is none). The `chmod go-w ~` matters: the stock firmware leaves `/root` writable by everyone, and the SSH server then ignores the key.

## The handheld

Boot order, as the boot scene lists it:

1. Display: `AnbernicDisplaySetup` opens `/dev/fb0`, sets it to the panel's size (640x480) in ARGB8888 with two pages, and scales the game's screen by whole pixels to fill it. Frames are presented on their own thread, in step with the panel's refresh. The game draws into three buffers the display lends it, so a frame is handed to that thread without a copy. When the game quits, the framebuffer goes back to the mode the firmware had. The log says what it found and what it set.
2. Input: `AnbernicInputSetup` reads the buttons from the device the firmware names `ANBERNIC-keys` and sends them as keys: the d-pad and the left stick as the arrows, A and Start as Enter, B as Esc, X as Space, Y as Tab, Select as Backspace, L1/R1 as Q/E, L2/R2 as Z/C. **Menu** quits the game, back to the firmware's menu.

Memory: the platform states 512 MB (`externalMemorySize`), what a game can use. Not measured yet.

Not set up: sound, the right stick, battery reading, brightness, rumble, HDMI out.
