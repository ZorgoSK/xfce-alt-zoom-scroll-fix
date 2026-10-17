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
