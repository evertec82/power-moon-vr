#pragma once
#include <stdbool.h>
#include <stdint.h>

// Identify the captured HDR-composition resource layout rather than a single
// fragment-program permutation. Dimensions are guest dimensions, before scaling.
static inline bool vr_bloom_compose(uint64_t vertices, bool depth_test,
    uint32_t vertex_inputs, uint32_t target_format, uint32_t target_w, uint32_t target_h,
    uint32_t bloom_format, uint32_t bloom_w, uint32_t bloom_h,
    uint32_t scene_format, uint32_t scene_w, uint32_t scene_h) {
    return vertices==3 && !depth_test && vertex_inputs==3 &&
        target_format==0x3d && target_w>=160 && target_h>=90 &&
        bloom_format==0x3f && scene_format==0x3f &&
        scene_w==target_w && scene_h==target_h &&
        bloom_w==scene_w/4 && bloom_h==scene_h/4;
}
