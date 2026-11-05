# Changelog

## v0.2.0

Power Moon VR v0.2.0 promotes the current serial EarlySubmit build to main.

- Exactly two launchers: **Launch-VR-2x.cmd** and **Launch-VR-4x.cmd**. Both use the same pacing and head tracking.
- Full-speed fixed-60 simulation during visual FPS drops, with conservative rendering optimizations.
- Stationary main-menu panel with visible button labels; smaller gameplay HUD.
- First/third-person toggle with **L3 + R3**, plus **F8** fallback. **F10** recenters.
- Unsupported HTML help pages close immediately in VR, removing the desktop-only blocking path seen during a device-loss crash. The exact GPU-loss cause remains unproven.

Install the overlay into an initialized Power Moon Recomp v0.2.1 Windows installation after backing up your executable and saves. Remove old Launch-VR*.cmd scripts, keep the base dependency DLLs, start SteamVR as the active OpenXR runtime, then choose 2× or 4×.

**No game files, keys, generated game code, compiled game libraries, saves or caches are included.** The runtime is built for general Windows x64 CPUs, without machine-specific native instructions.

Experimental VR support: first-person comfort and full-game/long-session stability need further headset testing. Frame drops and rendering artifacts may remain.

## v0.1.0

Initial experimental OpenXR overlay.
