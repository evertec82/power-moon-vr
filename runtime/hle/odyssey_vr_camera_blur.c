// Use the game's own blur-system switch rather than removing lighting shaders.
#include <string.h>
#include "../hle.h"

#ifndef __APPLE__
static uint64_t disable_blur;
static bool looked_up,reported;
static bool blur_heap(uint64_t p,uint64_t bytes) {
    return p>=HEAP_BASE && p-HEAP_BASE<HEAP_MAX && bytes<=HEAP_MAX-(p-HEAP_BASE);
}
#endif

void odyssey_vr_disable_camera_blur(Cpu *c,uint64_t scene) {
#ifndef __APPLE__
    extern bool vr_enabled(void);
    const char *mode=ENV("ODYSSEY_VR_CAMERA_BLUR");
    if(!vr_enabled() || (mode && strcmp(mode,"off")))return;
    if(!looked_up) {
        looked_up=true;
        disable_blur=loader_find_symbol("_ZN18alGraphicsFunction26invalidateCameraBlurSystemEPN2al5SceneE");
        if(!disable_blur)LOG("[vr-camera-blur] native system control unavailable; unchanged\n");
    }
    if(!disable_blur || !blur_heap(scene,0x98))return;
    // Validate the exact chain dereferenced by the exported game function.
    uint64_t holder=LD64(scene+0x90);
    if(!blur_heap(holder,0x40))return;
    uint64_t graphics=LD64(holder+0x38);
    if(!blur_heap(graphics,0x268))return;
    uint64_t controller=LD64(graphics+0x260);
    if(!blur_heap(controller,0x50))return;
    uint64_t param=LD64(controller+0x48);
    if(!blur_heap(param,0x6d))return;
    unsigned before=*(const uint8_t *)(uintptr_t)(param+0x6c);
    Cpu q=*c;q.sp=(q.sp-0x400)&~UINT64_C(15);q.x[0]=scene;
    guest_call(&q,disable_blur);
    if(!reported) {
        reported=true;
        LOG("[vr-camera-blur] horizontal camera-angle blur disabled by native system control (%u -> %u)\n",
            before,*(const uint8_t *)(uintptr_t)(param+0x6c));
    }
#else
    (void)c;(void)scene;
#endif
}

RT_ON_RESET(reset_vr_camera_blur) {
#ifndef __APPLE__
    disable_blur=0;looked_up=reported=false;
#endif
}
