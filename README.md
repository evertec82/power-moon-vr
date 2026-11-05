# Power Moon VR

Experimental third-person OpenXR support for [Power Moon Recomp](https://gitlab.com/zzzarathustra-dev/power-moon-recomp), based on v0.2.1 (`cdbe05c9df0e2ff4bfbecece89ad3f0fd9806193`). Windows x64, SteamVR/OpenXR, and a gamepad.

This repository contains the runtime and development tools. Supply your own Super Mario Odyssey 1.0.0 game files. Game dumps, keys, generated game code, compiled game libraries, saves, and caches are excluded.

## Features

- Third-person stereo rendering with native camera head tracking and F10 recentering.
- Separate eye texture histories and a serial rendering/submission path.
- HUD and menu layout at 65% size, centered inward for easier viewing in a headset.
- 2× rendering by default, optional 4×, and an experimental display-rate interpolation launcher.
- F9 captures the two eye images. The FPS overlay reports VR submission and game rates.

## Windows download

The [Releases](https://github.com/evertec82/power-moon-vr/releases) page provides a **runtime overlay**, not a complete game installation.

1. Set up Power Moon Recomp v0.2.1 using your own game files and confirm desktop gameplay works.
2. Back up your original runtime executable, then extract the overlay into that installation beside `PowerMoonRecomp.exe`.
3. Start SteamVR and select it as the active OpenXR runtime. Connect a gamepad.
4. Run `Launch-VR.cmd` for the game-rate serial path, or `Launch-VR-serial-refresh.cmd` for experimental display-rate scheduling. Press **F10** to recenter.

The overlay supplies the VR runtime and launchers; it relies on the base installation for its shared libraries and locally generated game library. If a game-library build-ID mismatch is reported, rebuild it locally from your own files using this source. Keep existing saves backed up.

## Build from source

See [Windows toolchain setup](docs/BUILD-windows.md). The bundled OpenXR headers are used at compile time; the runtime loads `openxr_loader.dll` dynamically.

1. Clone this repository with LF line endings (`.gitattributes` enforces them).
2. Run `powershell -ExecutionPolicy Bypass -File tools\setup_windows.ps1` to install the upstream Windows toolchain.
3. From an MSYS2 UCRT64 shell in the checkout, run `make -j16` to build the runtime. Run `make dlls` to collect runtime dependencies.
4. To generate the game library locally, place your own v1.0.0 ExeFS and RomFS in `extracted/exefs/` and `extracted/romfs/`, then run `make gen` and `make -j16`. Never upload these folders or `generated/`. Alternatively use the upstream first-run setup and its required generator/compiler tools.
5. Copy the desired scripts from `launchers/` beside the executable in your working installation. Ensure an OpenXR loader DLL is available; SteamVR includes one, or use your installed OpenXR SDK loader.

The VR changes have been tested on Windows/Vulkan. Other platforms retain upstream code but VR support on them is unverified.

## Settings

Edit the selected launcher to adjust:

| Variable | Default | Effect |
| --- | --- | --- |
| `ODYSSEY_VR_UI_SCALE` | `0.65` | HUD/menu size; supported range `0.35`–`1`. `1` restores the original layout. |
| `ODYSSEY_SCALE` | `2` | Internal resolution multiplier; 2× is 3840×2160 and 4× is 7680×4320. |
| `ODYSSEY_VR_RATE` | `game` | `game` uses the game presentation rate; `display` enables experimental headset-rate scheduling. |

F1 opens desktop settings. Game logic remains at 60 Hz. Headset refresh is controlled in SteamVR/headset settings. Higher resolution and refresh increase frame time and memory requirements.

## Status and limitations

This is an experimental port, not a finished VR conversion. Gameplay HUD and pause-menu stereo captures were checked after the UI resize with zero replay draw/uniform misses. The local tester confirmed the smaller interface works. Full-game VR rendering and sustained headset-refresh performance are unverified.

Some flicker, screen-space artifacts, frame drops, and head-motion judder may remain. Display-rate interpolation is experimental; the default launcher uses game-rate presentation. There is no motion-controller support or OpenXR depth submission. First-time shader compilation can stutter.

The additional asynchronous completion/threading experiment was rolled back after head-motion regressions and is not included.

## Credits and license

Runtime source: [MIT](LICENSE), retaining the upstream Power Moon Recomp / LAS Recomp notices. VR changes were developed with Codex and tested by the repository owner. Third-party notices remain under `third_party/`; see [THIRD_PARTY.md](THIRD_PARTY.md). This project is unaffiliated with Nintendo, Valve, or the upstream project. Super Mario Odyssey belongs to its respective owners.
