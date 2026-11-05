#pragma once
#include <stddef.h>
#include <stdint.h>
// Exact fingerprint used by existing Vulkan compatibility rules and diagnostics.
// Compute when a fragment shader is installed, rather than on every shader switch.
static uint64_t nvn_shader_fingerprint(const void *bytes,size_t size) {
    const uint8_t *p=(const uint8_t *)bytes; uint64_t h=UINT64_C(1469598103934665603);
    for(size_t i=0;i<size;i++) h=(h^p[i])*UINT64_C(1099511628211);
    return h;
}
