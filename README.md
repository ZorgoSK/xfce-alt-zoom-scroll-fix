# xfce-alt-scroll

Small native X11 tool for XFCE `Alt + two-finger touchpad scroll` desktop zoom, without passing smooth scrolling to GTK, Electron or virtual-machine windows.

**Version:** 0.1.0 (Debian package 0.1.0-1). The original package was installed and confirmed working after reboot by its user on an ASUS laptop running MX Linux, XFCE 4.20 and X11. This is a single-machine success report, not a general compatibility guarantee. C port of a working Python experiment. This version supports auto-discovered **touchpads only**, not physical mouse wheels. Neither Wayland nor other window managers are supported. The `.deb` is an experimental build for Debian Bookworm/MX Linux or newer on amd64, not an official Xfce package.

## How it works

- Enumerates `/dev/input/event*` (event node numbers are not hard-coded); selects a multitouch device whose evdev name contains `Touchpad`.
- Maps its name to the matching XInput device, saves `libinput Scroll Method Enabled`.
- While Alt is held, sets that property to `0 0 0`, reads `ABS_MT_POSITION_Y` for two fingers from evdev and converts every 2 mm of motion into a single XTest button 4/5 event, at most every 5 milliseconds.
- When Alt is released or the tool exits normally, restores the original scroll method. No queued zoom events.
- Polls for changes/reconnection. Uses one touchpad at a time (typical laptop).

The stateful evdev/XTest approach prevents the stutter caused by spawning `xdotool` for every event.

## Installing

1. Stop any earlier Python filter or manually started copy before installing. Do not run multiple filters at once. Re-enable original scrolling if necessary.
2. From the repository root, verify the preserved package with `sha256sum -c artifacts/SHA256SUMS`, then install it with `sudo apt install ./artifacts/xfce-alt-scroll_0.1.0-1_amd64.deb`. This is the original user-tested binary, not a new build.
3. **Sign out and in again** to allow the udev `uaccess` rule to grant active seat users permission to read touchpad evdev events, and to start the XFCE session autostart. A reboot also works.
4. Test `Alt + two-finger scroll` in `xfce4-terminal`, ChatGPT, and the VM.

The package installs a udev rule matching **touchpads only** and an XFCE-only XDG autostart entry. It does not require running the graphical program as root. Reading raw touchpad events requires ACL access to the device.

Manual foreground test (only when the autostart copy is not already running):

```
xfce-alt-scroll --verbose
```

Default tuning matches the tested Python prototype: `--step-mm 2 --interval-ms 5`. For reverse zoom use `--invert`.

## Emergency / rollback

Stop with Ctrl+C in a foreground session. The original scroll method is restored on SIGTERM and SIGINT as well. SIGKILL or power loss does *not* permit cleanup. In that case, restore with `xinput` using the current device ID:

```
xinput list --short
xinput set-prop <TOUCHPAD_ID> 'libinput Scroll Method Enabled' 1 0 0
```

The example `1 0 0` enables two-finger scrolling. Restore the values recorded for your own device; they may differ.

Disable auto-start without uninstalling:

```
mkdir -p ~/.config/autostart
printf '[Desktop Entry]\nType=Application\nName=xfce-alt-scroll\nHidden=true\n' > ~/.config/autostart/xfce-alt-scroll.desktop
```

Uninstall: `sudo apt remove xfce-alt-scroll`, then log out and in. The package never patches `xfwm4`.

## Known limitations

- Mouse-wheel-only desktops are **not fixed** by this v0.1 C port. Upstream `xfwm4` fix should be considered separately.
- Uses `xinput` executable to set the property on each Alt state transition. That operation is intentionally outside the high-frequency gesture path, which uses direct XTest.
- Requires readable `/dev/input/event*` touchpad node, and an XInput device with the `libinput Scroll Method Enabled` property. Matching by device name is not yet robust for two touchpads with identical names.
- Some early/delayed touch frames may be lost at gesture start. Threshold is 2mm by default.
- The original package has one reported successful hardware test after reboot. A fresh local rebuild was attempted during source recovery but failed because `X11/Xlib.h` was missing. No newly built binary or broad regression suite has been validated.

## Build

The build requires a C compiler, make, Linux input headers, X11 development headers/linker files and the XTest runtime library. Packaging also requires Debian packaging tools. `xinput` is required at runtime. Supply the build dependencies for your distribution first; the current recovery environment is missing `X11/Xlib.h`.

```sh
make
```

Build a Debian package without installing it:

```sh
mkdir -p tmp
TMPDIR="$PWD/tmp" sh scripts/build-deb.sh
```

The resulting package is written to `dist/`. The script includes the binary, README, touchpad-only udev rule and XFCE autostart entry. `make install` installs only the executable; it does not configure autostart or input permissions.

The original C source, Makefile and packaging files are preserved without functional changes. The preserved binary and a future local build are not claimed to be byte-for-byte reproducible. The repository README has been updated since the original package was built.

## Upstream work

This is an independent userspace workaround; it does not patch xfwm4. A proposed upstream investigation is recorded in [the draft](upstream/XFWM4-ISSUE-AND-MR-DRAFT.md). It is not a tested patch or a submitted merge request. Any xfwm4 contribution belongs on the [official Xfce GitLab](https://gitlab.xfce.org/xfce/xfwm4), after reproducing the issue and testing a fix there.

## License

GPL-2.0-or-later, as declared in the C source. See [LICENSE](LICENSE) for the GNU GPL version 2 text; you may use version 2 or any later version.
