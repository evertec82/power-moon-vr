#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_shadow.h"
int main(void) {
    VrShadowAccess s={0};
    assert(!vr_shadow_complete(&s,1));
    vr_shadow_access(&s,1,true);vr_shadow_access(&s,1,true);vr_shadow_access(&s,1,false);
    assert(vr_shadow_complete(&s,1)); // all writes precede all reads
    vr_shadow_access(&s,1,false);assert(vr_shadow_complete(&s,1));
    vr_shadow_access(&s,1,true);assert(!vr_shadow_complete(&s,1)); // reused map / write-read-write
    vr_shadow_access(&s,2,false);vr_shadow_access(&s,2,true);
    assert(!vr_shadow_complete(&s,2)); // temporal / read-before-write
    vr_shadow_access(&s,3,true);assert(vr_shadow_complete(&s,3));
    assert(!vr_shadow_complete(&s,2)); // stale frame
    vr_shadow_access(&s,4,false);assert(vr_shadow_single_phase(&s,4)); // read-only map
    vr_shadow_access(&s,4,true);vr_shadow_access(&s,4,true);vr_shadow_access(&s,4,false);
    assert(vr_shadow_single_phase(&s,4)); // old -> update -> new, two canonical versions
    vr_shadow_access(&s,4,true);assert(!vr_shadow_single_phase(&s,4)); // another update after sampling
    assert(!vr_shadow_single_phase(&s,3));
    puts("Shadow-sharing access-order tests passed");
}
