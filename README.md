# Power Moon VR

OpenXR VR support for [Power Moon Recomp](https://gitlab.com/zzzarathustra-dev/power-moon-recomp), based on v0.2.1 (`cdbe05c9df0e2ff4bfbecece89ad3f0fd9806193`). Windows x64, Vulkan, SteamVR/OpenXR, with gamepad and experimental VR-controller buttons/sticks.

Supply your own Super Mario Odyssey 1.0.0 files. This repository and the release package contain no game dumps, keys, generated game code, compiled game libraries, saves, or caches.

## Download and play

Download the Windows runtime package from [Releases](https://github.com/evertec82/power-moon-vr/releases/latest).

1. Extract the complete runtime ZIP into a new folder. It includes SDL3, the LGPL FFmpeg DLLs, other runtime dependencies, Roboto, and the first-run generator/compiler tools. A separate base app installation is unnecessary.
2. Supply your own Super Mario Odyssey v1.0.0 ExeFS/RomFS using the setup screen. When updating, back up your installation and retain your own `extracted/` and `save/` folders; replace the app files and remove obsolete launchers.
3. Start SteamVR, select it as the active OpenXR runtime, and connect your headset and gamepad.
4. Choose **`Launch-VR-4x.cmd`** (3840×2160), **`Launch-VR-6x.bat`** (5760×3240), **`Launch-VR-8x.cmd`** (7680×4320), **`Launch-VR-9x.bat`** (8640×4860), or **`Launch-VR-10x.bat`** (9600×5400). All use the same controller, rendering and pacing settings.

The package contains the base app and setup tools, but no game content. A game-library build-ID mismatch requires rebuilding the library locally from your own files. Windows 10/11 x64 and a Vulkan-capable GPU driver are required; system/driver DLLs are not redistributed.

## Controls

| Control | Action |
| --- | --- |
| Both stick clicks together (L3 + R3) | Toggle first/third person during gameplay; release before pressing again. |
| F8 | Keyboard fallback for view switching. |
| F10 | Recenter the headset view and stationary title panel. |
| F9 | Capture the eye images locally. |
| F1 | Open desktop settings. |

The game starts in third person. VR reserves the stick-click buttons for view switching; analog movement and camera axes remain available. VR-controller buttons and sticks are supported alongside the gamepad; tracked hands and swing gestures are not included.

## VR controller defaults

| Control | Game input |
| --- | --- |
| Left/right sticks | Movement/camera |
| Right A / right B | Jump or confirm / cap |
| Left X / left Y | Back or jump / secondary |
| Triggers / grips | ZL/ZR / L/R |
| Left menu; both secondary face buttons | Pause; fallback Pause chord |
| Both grips | Map |
| Left grip + right stick | D-pad |
| Both stick clicks | First/third-person toggle |

Touch-style controllers, Index, Microsoft motion controllers and simple-controller fallback have suggested bindings. Index uses A/B on each hand; Microsoft motion controllers use trackpad click for primary and right menu for secondary. SteamVR can rebind actions. Buttons combine with gamepad input; the stronger stick vector wins. VR input clears on focus loss or stale data.

The gameplay HUD is anchored to the character camera, retaining the v0.2.3 HUD and haptics. The launchers enable the isolated NoBloom workaround: ODYSSEY_VR_BLOOM_DIAG=off removes the identified HDR bloom input from both native and stereo replay frames. Fog and lens flares remain enabled; native camera blur remains disabled. The owner confirmed the NoBloom result and reported improved judder with the pose guard; longer testing remains open.

Game vibration drives the gamepad motors and the corresponding left/right VR controllers. **Controls → Vibration** controls both; **Test vibration** sends a short pulse to connected devices. Switch HD-rumble amplitudes are approximated on ordinary motors/OpenXR pulses, with bounded duration and stale-input expiry. Physical controller testing of VR haptics is pending.

## Current behavior

- v0.2.7 addresses stereo alignment on canted-display headsets by preserving each OpenXR eye orientation and asymmetric FOV, using a centered head reference, and matching rendered/submitted views. The HUD uses the same per-eye optics. Synthetic regression and gameplay checks are provided; physical Pimax confirmation is still needed. The game's shared native culling frustum remains bounded for extremely wide peripheral views.
- Stereo rendering with native camera head tracking, separate eye histories, and serial GPU retirement.
- Gameplay compositor handoff before the GPU wait. The stationary main-menu panel waits for its image to complete, preserving button labels.
- A fixed title panel and a character-camera-anchored gameplay HUD at 65% neutral scale.
- Fixed-60 simulation catch-up keeps game speed normal when visual FPS falls. Visual delivery can still judder below the headset refresh rate.
- Optional first-person camera at Mario's head position, levelled against gravity, using existing head tracking. His displayed model is hidden while that view is applied and restored on return to third person.
- Conservative shadow sharing, cached shader fingerprints, folded eligible eye clears, and nonblocking desktop mirroring reduce repeated work.
- Unsupported HTML help/manual pages close immediately in VR instead of blocking on a desktop-only notice.
- Native pose-query timestamps cannot move backward within the current recenter/reference-space epoch. Failed fresh queries retain the last applied pose and timestamp. This reduces a possible stale-pose transition without changing render threading; occasional below-refresh judder may remain.
- Frame-owned replay commands remove a live command-handle lifetime hazard, and optional native camera-time queries reduce pose age while preserving consistent geometry/lighting poses.
- Native camera blur is disabled by default in VR. The game triggers this effect from horizontal camera-angle changes, which produced distracting light smearing during fast head or stick turns. This uses the game's system control and preserves fog, lighting and lens flares; the separate NoBloom workaround suppresses the identified HDR bloom input. Set `ODYSSEY_VR_CAMERA_BLUR=on` to compare; the shipped launchers explicitly select `off`. The owner reported initial improvement; extended testing is pending.

4×, 6×, 8×, 9× and 10× launcher labels use twice the internal linear scale (2, 3, 4, 4.5, 5). These labels are preset names, not literal total-pixel multipliers; exact dimensions are listed above. Display interpolation, repeated-image scheduling and the asynchronous completion-worker experiment are disabled.

## Build

See [Windows toolchain setup](docs/BUILD-windows.md).

1. Clone with LF line endings and run `powershell -ExecutionPolicy Bypass -File tools\setup_windows.ps1`.
2. From an MSYS2 UCRT64 shell run `make -j16` for the runtime and `make dlls` for its dependencies. OpenXR headers are bundled; the loader is loaded dynamically.
3. Generate your game library locally using the bundled first-run setup, or place your own v1.0.0 ExeFS/RomFS in `extracted/exefs/` and `extracted/romfs/`, then run `make gen` and `make -j16`. Never upload `extracted/` or `generated/`.
4. Copy the desired scripts from `launchers/` beside the executable in your installation.

For a portable optimized runtime, use `make B=build-release PERF_FLAGS="-O3 -ffp-contract=off -flto=thin"`. Runtime optimization flags do not change game-library generation flags or ABI. The distributed runtime additionally uses a locally trained execution profile and does not use `-march=native`. The execution profile and generated game library are excluded from downloads. VR on other platforms is unverified.

## Validation and limitations

Standard and optimized Windows builds, camera math, simulation catch-up, frame metrics, shadow access ordering, shader fingerprinting, title-panel math, and the manual applet regression checks pass. Offline stereo captures were checked for ground, character, HUD, first-person occlusion and return to third person, with zero replay draw/uniform misses. The repository owner confirmed the gameplay handoff improvement and visible stationary-menu labels.

This remains an experimental VR port. First-person comfort, captures, cutscenes, unusual gravity and full-game coverage need further headset testing. Rendering artifacts, flicker, frame drops and shader-compilation stutter may remain; reaching headset refresh is not guaranteed. There is no OpenXR depth submission. The controller mapping/merge checks and SteamVR binding attachment pass, but physical controller validation is pending. The renamed 6×/10× presets and the new 9× preset need further headset validation.

One reported Vulkan device-loss crash occurred while the game waited in an unsupported HTML help applet. The blocking VR path has been removed and tested; the exact GPU-loss cause was not proven and longer-session stability is unverified.

## Credits and license

Runtime source: [MIT](LICENSE), retaining the upstream Power Moon Recomp / LAS Recomp notices. VR changes were developed with Codex and tested by the repository owner. See [THIRD_PARTY.md](THIRD_PARTY.md). This project is unaffiliated with Nintendo, Valve, or the upstream project. Super Mario Odyssey belongs to its respective owners.
