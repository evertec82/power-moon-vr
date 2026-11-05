# Power Moon VR

OpenXR VR support for [Power Moon Recomp](https://gitlab.com/zzzarathustra-dev/power-moon-recomp), based on v0.2.1 (`cdbe05c9df0e2ff4bfbecece89ad3f0fd9806193`). Windows x64, Vulkan, SteamVR/OpenXR, and a gamepad.

Supply your own Super Mario Odyssey 1.0.0 files. This repository and the release overlay contain no game dumps, keys, generated game code, compiled game libraries, saves, or caches.

## Download and play

Download the runtime overlay from [Releases](https://github.com/evertec82/power-moon-vr/releases/latest).

1. Set up Power Moon Recomp v0.2.1 with your own game files and confirm desktop gameplay works. Back up the executable and saves.
2. Remove old `Launch-VR*.cmd` scripts from that installation, then extract the overlay beside `PowerMoonRecomp.exe`, replacing the runtime. Keep your existing dependency DLLs, locally generated game library, and game data.
3. Start SteamVR, select it as the active OpenXR runtime, and connect your headset and gamepad.
4. Choose **`Launch-VR-2x.cmd`** (3840×2160) or **`Launch-VR-4x.cmd`** (7680×4320). Both use the same rendering and pacing settings.

The download is an overlay for an initialized installation, not a standalone game. A game-library build-ID mismatch requires rebuilding the library locally from your own files using this source.

## Controls

| Control | Action |
| --- | --- |
| Both stick clicks together (L3 + R3) | Toggle first/third person during gameplay; release before pressing again. |
| F8 | Keyboard fallback for view switching. |
| F10 | Recenter the headset view and stationary title panel. |
| F9 | Capture the eye images locally. |
| F1 | Open desktop settings. |

The game starts in third person. VR reserves the stick-click buttons for view switching; analog movement and camera axes remain available. No motion-controller support is included.

## Current behavior

- Stereo rendering with native camera head tracking, separate eye histories, and serial GPU retirement.
- Gameplay compositor handoff before the GPU wait. The stationary main-menu panel waits for its image to complete, preserving button labels.
- A fixed title panel that stays in place as you turn your head; gameplay HUD at 65% scale.
- Fixed-60 simulation catch-up keeps game speed normal when visual FPS falls. Visual delivery can still judder below the headset refresh rate.
- Optional first-person camera at Mario's head position, levelled against gravity, using existing head tracking. His displayed model is hidden while that view is applied and restored on return to third person.
- Conservative shadow sharing, cached shader fingerprints, folded eligible eye clears, and nonblocking desktop mirroring reduce repeated work.
- Unsupported HTML help/manual pages close immediately in VR instead of blocking on a desktop-only notice.

Only the 2× and 4× launchers are shipped. Display interpolation and the asynchronous completion-worker experiment are disabled in both.

## Build

See [Windows toolchain setup](docs/BUILD-windows.md).

1. Clone with LF line endings and run `powershell -ExecutionPolicy Bypass -File tools\setup_windows.ps1`.
2. From an MSYS2 UCRT64 shell run `make -j16` for the runtime and `make dlls` for its dependencies. OpenXR headers are bundled; the loader is loaded dynamically.
3. Generate your game library locally using the upstream first-run setup, or place your own v1.0.0 ExeFS/RomFS in `extracted/exefs/` and `extracted/romfs/`, then run `make gen` and `make -j16`. Never upload `extracted/` or `generated/`.
4. Copy the two scripts from `launchers/` beside the executable in your installation.

For a portable optimized runtime, use `make B=build-release PERF_FLAGS="-O3 -ffp-contract=off -flto=thin"`. Runtime optimization flags do not change game-library generation flags or ABI. The distributed runtime additionally uses a locally trained execution profile and does not use `-march=native`. The profile and game library are excluded from downloads. VR on other platforms is unverified.

## Validation and limitations

Standard and optimized Windows builds, camera math, simulation catch-up, frame metrics, shadow access ordering, shader fingerprinting, title-panel math, and the manual applet regression checks pass. Offline stereo captures were checked for ground, character, HUD, first-person occlusion and return to third person, with zero replay draw/uniform misses. The repository owner confirmed the gameplay handoff improvement and visible stationary-menu labels.

This remains an experimental VR port. First-person comfort, captures, cutscenes, unusual gravity and full-game coverage need further headset testing. Rendering artifacts, flicker, frame drops and shader-compilation stutter may remain; reaching headset refresh is not guaranteed. There is no OpenXR depth submission.

One reported Vulkan device-loss crash occurred while the game waited in an unsupported HTML help applet. The blocking VR path has been removed and tested; the exact GPU-loss cause was not proven and longer-session stability is unverified.

## Credits and license

Runtime source: [MIT](LICENSE), retaining the upstream Power Moon Recomp / LAS Recomp notices. VR changes were developed with Codex and tested by the repository owner. See [THIRD_PARTY.md](THIRD_PARTY.md). This project is unaffiliated with Nintendo, Valve, or the upstream project. Super Mario Odyssey belongs to its respective owners.
