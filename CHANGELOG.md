# Changelog

## v0.2.3

- Anchor gameplay HUD layouts and text to the character-camera panel instead of the headset. Preserve neutral size, project clipping bounds with the panel, and use the scene pose for compositor reprojection. Headset validation pending.

- Route game vibration to left/right OpenXR haptic actions alongside gamepad rumble.
- Expire stale side amplitudes, bound pulse durations, protect gamepad disconnects and stop vibration on reset/settings changes.
- Add Controls → Test vibration. Host routing, OpenXR output and controller-input regression checks pass; physical strength/compatibility testing is pending.

## v0.2.2

- **Native camera blur disabled by default in VR:** use the game's own blur-system control to address light smearing during fast horizontal head and stick turns. Bloom, fog, lighting and lens flares are restored; the previous broad fog/glow workaround is disabled in the launchers.
- **Resolution presets:** 2x, 3x, 4x and 5x (9600x5400). The 5x launcher replaces 6x.
- **Complete Windows runtime:** retains the base DLLs, VR-controller/gamepad support and first-run setup/compiler tools from v0.2.1. No separate base download is required.

The owner reported promising initial results with camera blur disabled. Extended headset testing remains pending; this is an incremental experimental update, not a guarantee that all rendering artifacts are fixed. Heap-boundary/register-preservation checks pass, and a Sand Kingdom stereo replay completed with zero draw/uniform misses. Head-tracking and render scheduling are unchanged from v0.2.1.

## v0.2.1

Power Moon VR v0.2.1 includes a complete Windows runtime package and the latest controller/lighting test updates.

- **Missing-DLL packaging fix:** SDL3.dll, avcodec-63.dll, avformat-63.dll, avutil-61.dll, swscale-10.dll and the recursive runtime dependencies are now included. Roboto and the first-run generator/compiler tools are included too. A separate base app download is no longer required.
- **VR controller buttons and sticks alongside gamepad:** right A jump/confirm, right B cap, left X back, left Y secondary; movement/camera sticks, triggers, pause/map chords and the existing two-stick-click view toggle. No tracked hands, gestures or VR haptics in this iteration.
- **Cloud-light diagnostic:** the launchers enable the known fog/glow compatibility filter for moving light artifacts. It can reduce fog/bloom; headset confirmation of this workaround is pending. Set ODYSSEY_VR_EFFECTS=full to compare.
- **Crash hardening:** replay uses frame-owned command copies instead of resolving game command handles that may have been replaced. Ownership and repeated-stereo regression checks pass; long-session stability is not guaranteed.
- **Pose/pacing test updates:** native camera-time pose queries, consistent geometry/lighting/submission poses, full-speed simulation and serial rendering. Repeated-image scheduling is disabled. The user reported improved motion and fixed shadows, with smaller light artifacts remaining.
- **Resolution choices:** 2x/4x .cmd launchers plus 3x/6x .bat launchers. Higher-resolution presets have not been headset-validated.

Extract into a new folder, or back up your current installation and replace its app files. Supply your own Super Mario Odyssey v1.0.0 ExeFS/RomFS through the first-run setup, or retain your existing extracted/ and save/ folders. Start SteamVR as the active OpenXR runtime. Windows 10/11 x64 and a Vulkan-capable graphics driver are required.

**No game dumps, keys, generated game code, compiled game libraries, saves or caches are included.** gamelib-src contains only the open-source runtime headers/build glue needed to build your own game library.

Portable optimized build; no -march=native. Controller bindings were accepted by SteamVR and mapping/merge tests pass; physical controller testing remains pending. Treat this as experimental VR support. Dependency sources and checksum files are provided alongside the runtime download.


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
