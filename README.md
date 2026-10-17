# xfce-alt-zoom-scroll-fix

I use **Alt + two-finger scrolling** to zoom the XFCE desktop. The problem was that the same gesture also scrolled the application underneath.

This small C filter fixes that: holding Alt zooms the desktop, and releasing Alt brings back normal smooth scrolling.

For **XFCE on X11 with a libinput touchpad**. The package is for **Debian/MX Linux, amd64**. Physical mouse wheels and Wayland are not supported.

## Install

1. Download [xfce-alt-scroll_0.1.0-1_amd64.deb](https://github.com/ZorgoSK/xfce-alt-zoom-scroll-fix/releases/download/v0.1.0/xfce-alt-scroll_0.1.0-1_amd64.deb).
2. Open a terminal in the download folder and run:

   ```sh
   sudo apt install ./xfce-alt-scroll_0.1.0-1_amd64.deb
   ```

3. Log out and back in, or reboot.

The filter starts automatically with XFCE. Keep desktop zoom enabled and its modifier set to Alt. If you already run an older filter manually, stop it first.

The installed package and command are named `xfce-alt-scroll`. Default sensitivity is **2 mm per zoom step**, with a **5 ms minimum interval**.

## Uninstall

```sh
sudo apt remove xfce-alt-scroll
```

Then log out and back in, or reboot.

## Build from source

With a C compiler, make, X11 development headers and the XTest library installed:

```sh
make
mkdir -p tmp
TMPDIR="$PWD/tmp" sh scripts/build-deb.sh
```

The `.deb` will be in `dist/`.

## About

This is a standalone workaround, not an official Xfce patch. I'd like to see the underlying issue fixed in [xfwm4](https://gitlab.xfce.org/xfce/xfwm4).

Licensed under [GPL-2.0-or-later](LICENSE).

## Technical details

<details>
<summary>Why Alt + scroll can affect both the desktop and an app, and what the C filter does</summary>

### X11 core scrolling and XI2 smooth scrolling

The relevant names are **X11** and **XI2 (XInput 2)**. XI2 is an extension within X11, not a separate display server or “X12”.

Traditional X11 scrolling represents vertical wheel steps as button **4/5** press/release events. XI2.1 added smooth scrolling through scroll-axis values carried in device events. Those values can describe movement smaller than one wheel step; the server also supports compatibility emulation between scroll-axis changes and legacy button events. This is what “micro-scroll” means here: small scroll deltas, not another protocol. See the [X Input Extension specification, “Smooth scrolling”](https://www.x.org/releases/current/doc/inputproto/XI2proto.txt).

A desktop zoom shortcut and an application can therefore react to different representations of the same physical gesture. In the affected setup, desktop zoom reacts while a smooth-scrolling application also moves its content. Seeing no button 4/5 events in `xev -event button` does not prove that XI2 scroll-axis events stopped reaching another application.

### What points to this explanation

During troubleshooting, forcing the terminal onto core input with `GDK_CORE_DEVICE_EVENTS=1` stopped the unwanted scrolling, but also removed normal smooth scrolling. Temporarily disabling the touchpad's libinput scroll method stopped the application scroll; sending synthetic button 4/5 events while Alt remained held still zoomed the desktop.

These observations are consistent with a mismatch between the zoom shortcut's event handling and XI2 smooth-scroll delivery. **They do not yet identify a verified defective line in xfwm4 or establish how every X server/device grab behaves.** Blocking legacy button events alone is not evidence that all smooth-scroll delivery is blocked. A proper upstream fix needs an event trace and reproduction against current xfwm4; simply returning early from its event handler does not retract events another client has already received.

### What the C program does in the background

The implementation is in [`src/xfce-alt-scroll.c`](src/xfce-alt-scroll.c):

1. **Find a touchpad.** `discover_touchpad()` scans `/dev/input/event*`, checks for multitouch slots and a vertical position axis, and requires “Touchpad” or “touchpad” in the device name. It matches that name to an XInput device and saves its current scroll-method setting. Device numbers are not hard-coded.
2. **Watch Alt through X11.** `alt_down()` uses `XQueryKeymap()` to check the left and right Alt keycodes. It does not read a keyboard event device. The main loop checks this state around its input polling; it is not an instantaneous keyboard hook.
3. **Disable the driver's scrolling while Alt is held.** `update_alt()` runs `xinput set-prop` to set `libinput Scroll Method Enabled` to `0 0 0`. Those three flags represent two-finger, edge and button scrolling. This changes the touchpad's scroll generation for the session, rather than discarding events inside one application. See the [Xorg libinput driver documentation](https://manpages.debian.org/bookworm/xserver-xorg-input-libinput/libinput.4.en.html).
4. **Read finger movement directly.** The program keeps reading Linux evdev multitouch positions even while the Xorg driver's scrolling is disabled. `read_touchpad()` tracks contacts by slot; `process_sync()` uses the average vertical position only when exactly two active contacts have known positions.
5. **Turn movement into zoom steps.** The default threshold is **2 mm**, converted using the device's reported axis resolution. If resolution is missing, the code estimates it from the axis range using a 76 mm fallback. At a threshold crossing, `inject_zoom()` sends a button 4 or 5 press/release pair through XTest. With Alt held and desktop zoom enabled, xfwm4 interprets that as zoom.
6. **Avoid a growing zoom backlog.** Movement accumulation is clamped to one step in either direction. At most one step is emitted per processed frame, with a **5 ms minimum interval** between steps. This is a rate limit, not a guaranteed 200 Hz output rate. XTest is called directly; the high-frequency gesture path does not launch `xdotool` processes.
7. **Restore scrolling.** On Alt release, the program clears its gesture accumulator and attempts to restore the saved scroll-method values. Normal exit and SIGINT, SIGTERM or SIGHUP also trigger a restoration attempt. When a touchpad disappears, it closes that device and periodically looks for another one.

The package provides an XFCE autostart entry and a udev rule intended to grant the active local session access to touchpad event nodes. The graphical filter runs as the session user.

### Limits of this workaround

This changes scrolling for the selected touchpad while **either Alt key** is held, regardless of which application has focus. It does not check whether desktop zoom is currently enabled, so Alt remains reserved for this behavior while the filter runs. It suppresses the touchpad's normal scrolling in both directions but generates only vertical zoom steps.

Version 0.1 handles one touchpad at a time. Matching by name can be ambiguous with identical devices. Because Alt is polled and changing the driver property takes time, the transition is not atomic and an initial scroll event can slip through. Physical mouse wheels are not covered by this touchpad scroll-method switch.

Restoration can fail if the device or X server disappears. SIGKILL cannot run cleanup; if scrolling stays disabled, restore the device's previously saved `libinput Scroll Method Enabled` values with `xinput`. The program does not patch xfwm4 or change XI2 itself: it prevents the unwanted driver-generated scroll and substitutes desktop-zoom steps while Alt is held.

</details>
