#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { VR_SHADOW_WRITE=1, VR_SHADOW_READ_AFTER_WRITE=2,
       VR_SHADOW_READ_BEFORE_WRITE=4, VR_SHADOW_MULTIPHASE=8 };
typedef struct { uint32_t epoch; uint8_t flags; } VrShadowAccess;
static void vr_shadow_access(VrShadowAccess *s,uint32_t epoch,bool write) {
    if(s->epoch!=epoch) { s->epoch=epoch; s->flags=0; }
    if(write) {
        if(s->flags&VR_SHADOW_READ_AFTER_WRITE) s->flags|=VR_SHADOW_MULTIPHASE;
        s->flags|=VR_SHADOW_WRITE;
    } else s->flags|=(s->flags&VR_SHADOW_WRITE)?VR_SHADOW_READ_AFTER_WRITE:VR_SHADOW_READ_BEFORE_WRITE;
}
static bool vr_shadow_complete(const VrShadowAccess *s,uint32_t epoch) {
    return s->epoch==epoch && (s->flags&VR_SHADOW_WRITE) &&
           !(s->flags&(VR_SHADOW_READ_BEFORE_WRITE|VR_SHADOW_MULTIPHASE));
}

// Maps with read-before-write are safe when their two versions are selected at
// the replay's first write. Reject any write/read/write reuse within the frame.
static bool vr_shadow_single_phase(const VrShadowAccess *s,uint32_t epoch) {
    return s->epoch==epoch && s->flags && !(s->flags&VR_SHADOW_MULTIPHASE);
}
