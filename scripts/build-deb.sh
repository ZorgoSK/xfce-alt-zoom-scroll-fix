#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make
architecture="$(dpkg --print-architecture)"
version=0.1.0-1
root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT HUP INT TERM
chmod 0755 "$root"
install -d "$root/DEBIAN" "$root/usr/bin" "$root/usr/share/doc/xfce-alt-scroll" \
    "$root/etc/xdg/autostart" "$root/etc/udev/rules.d"
install -m755 xfce-alt-scroll "$root/usr/bin/xfce-alt-scroll"
install -m644 README.md "$root/usr/share/doc/xfce-alt-scroll/README.md"
install -m644 packaging/xfce-alt-scroll.desktop "$root/etc/xdg/autostart/xfce-alt-scroll.desktop"
install -m644 packaging/70-xfce-alt-scroll-touchpad.rules "$root/etc/udev/rules.d/70-xfce-alt-scroll-touchpad.rules"
sed "s/^Architecture: amd64$/Architecture: $architecture/" packaging/control.template > "$root/DEBIAN/control"
chmod 0755 "$root/DEBIAN"
mkdir -p dist
dpkg-deb --build --root-owner-group "$root" "dist/xfce-alt-scroll_${version}_${architecture}.deb"
