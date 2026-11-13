# Changelog

Notable changes to `deki-anbernic-boards`. Engine and editor changes are in the
[engine changelog](https://github.com/dekiengine/deki-engine/blob/master/CHANGELOG.md).

## Unreleased

### The Anbernic RG40XX H as a build target
- `anbernic_rg40xx_h`: 640x480 ARGB8888, cross-compiled with Arm's
  `aarch64-none-linux-gnu` toolchain and linked static.
- Display: sets `/dev/fb0` to the panel's size in ARGB8888 with two pages,
  flips between them on its own thread, and puts the firmware's mode back
  when the game quits.
- Input: the buttons from the `ANBERNIC-keys` device, the d-pad and the left
  stick as keys; Menu quits.
- Install: `<Name>.sh` and `<Name>/` in `Roms/APPS/` on a stock firmware card,
  with `Imgs/<Name>.png` as its picture. The script goes last, so the Apps list
  shows the game only once it is whole.

### Install over Wi-Fi
- With the handheld's address in the platform (**Install over Wi-Fi**:
  `frameworkOptions` `sshHost`, `sshUser`, `sshCard`), Install offers it
  beside the SD cards, as the target `ssh:root@<address>`. It copies with the
  computer's own `ssh` and `scp`, logs in with its key only, and installs as
  onto a card: a hidden folder first, every file checked against the build
  (SHA-256) before it replaces the old app, the script last, an app the editor
  did not install left alone. A refused login says how to add the key; the
  README has the steps, and the stock firmware's `/root` permissions trap.

### Sticks
- Both sticks are read as sticks (`DekiInput::GetStick`), from -1 to 1 over
  each axis' own range; the left one still sends the arrow keys too.

### Measuring
- A file named `profile` beside the game turns on a report in its log every
  2 s: frames and their spacing, the engine's update, render and present,
  each kind of component and blit path, the costliest objects, and the
  display thread's copy. With `trace` in that file, every frame also goes to
  `trace.txt`: when it was handed over, its time step, the camera in screen
  pixels, and when it went up on the panel.
