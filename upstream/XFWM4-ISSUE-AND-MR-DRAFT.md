# Upstream proposal (NOT YET SUBMITTED / no code patch yet)

Project: https://gitlab.xfce.org/xfce/xfwm4
Existing related issue: https://gitlab.xfce.org/xfce/xfwm4/-/issues/159

## Suggested issue / draft MR title

X11: Prevent XI2 smooth scrolling from reaching client applications during Alt+scroll desktop zoom

## Description

With the Xfce desktop zoom enabled (`/general/zoom_desktop`) and the easy-click modifier set to `Alt`, two-finger scrolling on a libinput touchpad both zooms the desktop *and* scrolls the application under the pointer. Reproduced with xfwm4 4.20.0 (XInput 2 support: No), GTK3 xfce4-terminal, ChatGPT (Electron) and a KVM/QEMU console. Traditional X11 `xev -event button` sees no Button4/5 while Alt is held, yet scrolls still reach some apps.

## Reproducer

1. X11 / XFCE 4.20, compositor enabled, desktop zoom enabled, easy_click=Alt.
2. Open xfce4-terminal with enough lines to scroll.
3. Hold Alt and use a touchpad's two-finger smooth-scroll gesture.
4. Actual: desktop zooms in discrete steps, terminal content scrolls at the same time.
5. Expected: desktop zooms; no scroll reaches the application. Normal smooth scrolling without Alt must be unchanged.

## Diagnostic evidence

- `GDK_CORE_DEVICE_EVENTS=1 xfce4-terminal --disable-server` eliminates the unwanted terminal scrolling, but also loses smooth/pixel scrolling without Alt.
- `xev -event button` observes ordinary Button4/5 events, but no Button4/5 for Alt+scroll.
- `libinput debug-events` on `/dev/input/by-path/platform-AMDI0010:00-event-mouse` reports `POINTER_SCROLL_FINGER` with high-frequency fractional deltas.
- A proof-of-concept userspace work-around temporarily turns `libinput Scroll Method Enabled` to `0 0 0` while Alt is held, and synthesizes step-wise XTest Button4/5 for xfwm4. This prevents scrolling in applications while retaining smooth scrolling when Alt is released.

## Proposed implementation discussion (requires maintainer review)

Explore whether xfwm4's X11 zoom grab can suppress XI2 smooth-scroll events for clients while retaining normal XI2 input when Alt is not held. Please consider protocol implications: passive grabs for core button events do not guarantee suppression of XI2 valuator events. Merely changing event handler return values may not prevent another client receiving an XI2 event, so an implementation must be validated with real touchpads and modern GTK/Electron applications.

Tests should include core wheel Button4/5, XI2 touchpad smooth-scroll, both Alts, modifier combinations, keyboard layout switches, multiple seats/devices, and preserving smooth scroll without Alt.

No upstream code changes are included yet because an untested patch would not be appropriate for submission. First reproduce against current upstream master, agree on technical approach, then create an actual merge request.

## Submission

XFCE accepts Merge Requests through GitLab, not GitHub `xfce-mirror` PRs. Relevant official repo: https://gitlab.xfce.org/xfce/xfwm4. This markdown is a ready-to-use technical issue/MR *description*, not a created MR. GitLab sign-in/fork and source test/build are still necessary.
