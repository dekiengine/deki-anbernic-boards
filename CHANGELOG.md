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
