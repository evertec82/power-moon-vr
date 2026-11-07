#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_bloom.h"
int main(void) {
    assert(vr_bloom_compose(3,false,3,0x3d,1600,900,0x3f,400,225,0x3f,1600,900));
    assert(vr_bloom_compose(3,false,3,0x3d,1440,810,0x3f,360,202,0x3f,1440,810));
    assert(!vr_bloom_compose(6,false,3,0x3d,1600,900,0x3f,400,225,0x3f,1600,900));
    assert(!vr_bloom_compose(3,true,3,0x3d,1600,900,0x3f,400,225,0x3f,1600,900));
    assert(!vr_bloom_compose(3,false,1,0x3d,1600,900,0x3f,400,225,0x3f,1600,900));
    assert(!vr_bloom_compose(3,false,3,0x3f,1600,900,0x3f,400,225,0x3f,1600,900));
    assert(!vr_bloom_compose(3,false,3,0x3d,1600,900,0x3f,800,450,0x3f,1600,900));
    assert(!vr_bloom_compose(3,false,3,0x3d,1600,900,0x25,400,225,0x3f,1600,900));
    assert(!vr_bloom_compose(3,false,3,0x3d,1600,900,0x3f,400,225,0x3f,1280,720));
    puts("HDR bloom layout recognizes native/dynamic sizes and rejects scene, HUD, HDR intermediates and half-resolution effects");
}
