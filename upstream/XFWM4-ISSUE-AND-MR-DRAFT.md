# Upstream fix proposal

Draft for [xfwm4](https://gitlab.xfce.org/xfce/xfwm4), not yet submitted.

## Problem

When I hold Alt and scroll with two fingers to zoom the XFCE desktop, the application underneath also scrolls. I'd like the gesture to affect only desktop zoom while Alt is held, with normal smooth scrolling unchanged after release.

## Reproduce

1. Enable desktop zoom in an XFCE/X11 session and use Alt as the modifier.
2. Open a window with scrollable content.
3. Hold Alt and scroll with two fingers on the touchpad.
4. Both the desktop zoom and the application's scroll position change.

## Current workaround

My standalone filter temporarily disables the touchpad's normal scrolling while Alt is held and sends zoom steps instead. It restores normal scrolling when Alt is released.

For an upstream fix, I'd like to investigate how xfwm4 handles XI2 smooth-scroll events during desktop zoom. The fix needs to prevent those events from reaching applications without breaking ordinary scrolling. This still needs reproduction against current upstream and a tested patch before a GitLab merge request.
