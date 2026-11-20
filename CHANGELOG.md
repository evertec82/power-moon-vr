# Changelog

## v0.2.12

- Share decoded graphics pipeline state and uploads of frame-owned uniforms between the two eye renders. Each eye still resolves its own attachments and camera-dependent uniforms.
- Reduce CPU work on replay and shader lookup, prepare descriptor binding order once per shader program, and use restricted mip views to avoid redundant framebuffer copies in eligible passes.
- Enable shared preparation and restricted mip views in all five VR presets. GPU overlap remains opt-in through `ODYSSEY_VR_OVERLAP_GPU=1`.
- In the controlled 4× forest A/B, shared preparation reduced average stereo replay and handoff from 9.50 ms to 9.37 ms and the 95th percentile from 14.63 ms to 14.13 ms. The forest still misses 90 FPS in heavy views. A physical 90 Hz head-turn check for this release is pending.
- Existing v0.2.11 game libraries can be used directly.

## v0.2.11

- Enable native headset-refresh rendering in the 4×, 6×, 8×, 9× and 10× launchers. Supported gameplay scenes render extra stereo pairs between 60 Hz gameplay ticks. Each pair uses the head pose captured for that scene draw.
- Keep input and particle updates on gameplay ticks. Use the original cadence during loading and on the title screen, and fall back to a 60 Hz draw target under sustained load.
- The 4× preset reached 90 FPS during headset testing, with smooth head turns and scene-dependent delivery of 60–90 FPS. The same refresh settings apply at every resolution.
- Update the bundled generator and import library. Upgrading requires a one-time local game-library rebuild through setup.

## v0.2.10

- Fix misplaced/ghost-like shading by rendering lighting and postprocessing in the native enclosing camera frustum, then projecting the completed image into each OpenXR eye's asymmetric FOV and cant. Keep eye separation and the recorded head-pose/submission timing. The owner confirmed the misplaced shadows are gone.
- Correct traced shadow and ambient-atlas lookups and forward screen-lighting coordinates without replacing unrelated effect matrices.
- Synchronize native camera FOV/aspect/clip metadata with the VR projection. The owner reports improved orange-platform visibility, but some platforms still disappear with head direction; further culling investigation is deferred.
- Make the confirmed settings default for the five 4x/6x/8x/9x/10x presets and direct VR-enabled launches. Preserve corrected colors, isolated NoBloom, stationary menu, camera-fixed HUD, haptics and VR-controller/gamepad support.
- Keep the original 2 ms render-debt policy. The 8 ms pacing experiment worsened judder in headset testing and is not included in the defaults. Occasional head-turn judder remains.
- Extend canted/asymmetric stereo, image-projection, angular-coverage and HUD regression checks. Offline gameplay replays report no draw/uniform misses; GPU color transfer remains exact for sRGB output and within one 8-bit level for linear output.

## v0.2.9

- Fix washed-out headset colors when the game presents sRGB-encoded pixels in an RGBA8/BGRA8 UNORM image. Sample that scanout through an sRGB view and render into the OpenXR swapchain with the correct conversion. In-game GPU captures match the desktop reference exactly for an sRGB destination and within one 8-bit level for a linear destination.
- Make the isolated NoBloom workaround the VR default, including direct executable launches. Recognize padded quarter-resolution bloom targets and use a clamp sampler for the black replacement texture. Headset confirmation of the returning moving-light artifact is pending.
- Add an opt-in GPU color-transfer capture and comparison tool. Retain the canted-display, per-eye shadow, HUD, haptics and controller changes from previous releases.

## v0.2.8

- Correct the screen-space shadow resolve's view-to-shadow lookup for each rendered eye instead of reconstructing both eyes with the center camera. Preserve the canted-display alignment introduced in v0.2.7.
- Update matching native projection/inverse-projection uniforms in depth-reading screen effects to the rendered eye's clip space, including the game's -1..1 versus Vulkan 0..1 depth conversion. Leave world, HUD and light-camera draws unchanged.
- Add per-eye shadow/depth reconstruction regression checks for cant, asymmetric FOV, eye separation and reversed depth. Existing resolution presets, NoBloom, haptics and pose timing remain unchanged. Physical headset confirmation of the reported one-eye effects remains pending.

## v0.2.7

- Address stereo alignment on Pimax and other canted-display headsets: preserve the runtime's eye orientations and asymmetric FOVs, use a binocular midpoint for native tracking/recentering, and submit the same per-eye pose/FOV used to render.
- Project the camera-anchored HUD through each eye's optical frustum while retaining its shared physical panel size. Keep the existing pose timing, NoBloom, haptics and five resolution presets.
- Add canted/asymmetric stereo regression coverage and an opt-in synthetic-cant gameplay check. Physical Pimax confirmation remains pending; this release does not claim a general judder or peripheral-culling fix.

## v0.2.6

- Correct the repository's 4x launcher to internal scale 2 (3840x2160), matching the packaged 4x preset. Runtime and five resolution choices remain unchanged. QPC/guard-band experiments remain excluded.

## v0.2.5

- Relabel resolution presets 2x/3x/4x/5x as 4x/6x/8x/10x without changing their render sizes. Add 9x at 8640x4860 (ODYSSEY_SCALE=4.5). These are preset labels, not literal total-pixel multipliers.
- Retain the v0.2.4 executable with NoBloom, native camera-blur control, pose timing guard, camera-anchored HUD and controller haptics. The QPC/guard-band experiments were not promoted and are excluded.

## v0.2.4

- Guard native pose-query targets against backward time steps when cadence/prediction lead changes. Retain the last applied pose and timestamp on a failed fresh query; add diagnostic counters. Rendering threads, serial retirement and pacing settings remain unchanged.

- Restore the isolated NoBloom HDR-composition input filter on native and eye replays in all resolution launchers. Keep camera-anchored HUD, haptics and native camera-blur fix. Fog and lens flares remain enabled. The owner confirmed that NoBloom looks good and the pose-guard test improves judder.

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
