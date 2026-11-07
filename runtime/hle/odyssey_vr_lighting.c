// Opt-in suppression of the camera-relative lens-flare pass. Preserve fog,
// bloom and all camera/submission poses, matching the original NoFlare test.
#include <stdatomic.h>
#include <string.h>
#include "../hle.h"

#ifndef __APPLE__
static GuestFn lens_flare_original;
static bool disable_flare;
static atomic_uint flare_calls;
extern bool vr_enabled(void);

static void lens_flare_draw(Cpu *c) {
    unsigned call = atomic_fetch_add(&flare_calls, 1);
    if (!call)
        LOG("[vr-lighting] lens-flare draw reached; %s\n",
            disable_flare ? "disabled (NoFlare)" : "original draw retained");
    if (!disable_flare || !vr_enabled()) lens_flare_original(c);
}
#endif

void odyssey_vr_lighting_init(void) {
#ifndef __APPLE__
    // Install at boot, before guest threads can cache the virtual draw target.
    // Launchers explicitly select no-lens-flare; unset preserves native draws.
    const char *mode = getenv("ODYSSEY_VR_LIGHTING_DIAG");
    // The render backend starts after odyssey_boot; vr_enabled() is still false
    // here. Register from the requested mode, then guard the draw at execution.
    const char *vr = getenv("ODYSSEY_VR");
    if (!vr || !atoi(vr) || !mode || lens_flare_original) return;
    if (strcmp(mode, "trace") && strcmp(mode, "no-lens-flare")) {
        LOG("[vr-lighting] unknown diagnostic mode '%s'; effects unchanged\n", mode);
        return;
    }
    const char *symbol = "_ZNK3agl2fx12OfxLensFlare9drawImpl_EPNS_11DrawContextEiRKNS_12RenderBufferERKN4sead8ViewportERKNS0_7OfxBase7ContextERKNS0_17OcclusionRendererE";
    uint64_t address = loader_find_symbol(symbol);
    if (!address || !(lens_flare_original = dispatch_lookup(address))) {
        LOG("[vr-lighting] lens-flare draw unavailable; effects unchanged\n");
        return;
    }
    disable_flare = !strcmp(mode, "no-lens-flare");
    hle_override(address, lens_flare_draw, "OfxLensFlare::drawImpl (VR NoFlare)");
    LOG("[vr-lighting] %s installed; fog/bloom untouched\n", mode);
#endif
}

RT_ON_RESET(reset_vr_lighting_counters) {
#ifndef __APPLE__
    atomic_store(&flare_calls, 0);
#endif
}
