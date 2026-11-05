#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { VR_PASS_SCREEN, VR_PASS_WORLD, VR_PASS_HUD };
static int vr_classify_pass(uint32_t cbuf_mask, uint32_t cb3_read, uint64_t cb0_size,
                            bool ui_covers_screen, uint64_t vertices, uint32_t scene_vertex_inputs, uint32_t depth_w, uint32_t depth_h) {
    // Bound state persists across draws. A leftover HUD buffer does not make a shader a HUD shader.
    bool ui = (cbuf_mask & (1u << 3)) && cb3_read >= 512 && cb0_size == 560;
    if (ui) return ui_covers_screen ? VR_PASS_SCREEN : VR_PASS_HUD;
    // The game changes its scene resolution dynamically. Use shape, not a 1280x720 minimum;
    // square/atlas shadow maps and procedural postprocessing triangles stay in their original space.
    // scene_vertex_inputs is zero for draws without depth testing: fullscreen quads stay in screen space.
    // Depth-tested particle billboards and small mesh triangles follow the eye just like larger meshes.
    bool scene = vertices >= 3 && (vertices > 6 || scene_vertex_inputs) && (cbuf_mask & ~15u) && depth_w >= 160 && depth_h >= 90 &&
                 depth_w > depth_h * 1.70f && depth_w < depth_h * 1.82f;
    return scene ? VR_PASS_WORLD : VR_PASS_SCREEN;
}

// Temporary VR compatibility filter: these exact shader fingerprints were traced to
// fog/depth composites and downsampled glow using the original camera's reconstruction.
// Leave unknown shaders alone. The native camera path uses full effects by default;
// ODYSSEY_VR_EFFECTS=compatible explicitly enables this older fallback.
static inline bool vr_incompatible_effect(uint64_t fragment_hash) {
    return fragment_hash==0xe090bac8c2a9209aull ||
           fragment_hash==0xea06ced58b44ad97ull ||
           fragment_hash==0xb99a4565b6fa1f1eull;
}

// A depth-reconstruction triangle reads camera/depth data rather than object transforms.
// Translating it as world geometry moves the entire effect and exposes an eye-edge stripe.
static inline bool vr_depth_screen_pass(uint64_t vertices,uint32_t cbuf_mask,uint32_t vertex_inputs,bool primary_screen) {
    return vertices>=3 && vertices<=12 && primary_screen && vertex_inputs &&
           !(vertex_inputs & ~3u) && (cbuf_mask & 16u) && !(cbuf_mask & ~27u);
}
