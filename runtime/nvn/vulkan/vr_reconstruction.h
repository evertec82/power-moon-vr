#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "vr_math.h"

static void vr_rows_to_matrix(float out[16],const float *rows) {
    for(int r=0;r<4;r++)for(int c=0;c<4;c++)out[c*4+r]=rows[r*4+c];
}
static void vr_matrix_to_rows(float *rows,const float in[16]) {
    for(int r=0;r<4;r++)for(int c=0;c<4;c++)rows[r*4+c]=in[c*4+r];
}
static bool vr_matrix_matches(const float *rows,const float matrix[16]) {
    for(int r=0;r<4;r++)for(int c=0;c<4;c++) {
        float a=rows[r*4+c],b=matrix[c*4+r];
        if(!isfinite(a)||fabsf(a-b)>.00002f*fmaxf(.01f,fabsf(b)))return false;
    }
    return true;
}
// Uniforms use the game's -1..1 clip depth, whereas the eye transform is
// applied after Vulkan's 0..1 conversion. Keep the conversion on both sides.
static bool vr_shadow_view_correction(float inv_native[16],float view_correction[16],
                                      const float projection[16],const float clip[16]) {
    float to_vk[16],from_vk[16],a[16],delta[16],native[16],eye[16],inv_eye[16];
    vr_identity(to_vk);to_vk[10]=.5f;to_vk[14]=.5f;
    vr_identity(from_vk);from_vk[10]=2;from_vk[14]=-1;
    vr_multiply(native,from_vk,projection);
    if(!vr_inverse(inv_native,native))return false;
    vr_multiply(a,clip,to_vk);vr_multiply(delta,from_vk,a);
    vr_multiply(eye,delta,native);
    if(!vr_inverse(inv_eye,eye))return false;
    // A point reconstructed with the native focal lengths and the eye depth
    // becomes a native-view point through this affine mapping (cant + IPD).
    vr_multiply(view_correction,inv_eye,native);
    return true;
}
// The shadow lookup consumes native-view XYZ reconstructed with the original
// camera parameters. Correct that lookup once, leaving its inputs consistent.
// Other screen effects have their own ray/scalar layouts, so matching a matrix
// is insufficient evidence that it can be replaced in isolation.
static unsigned vr_reconstruction_patch(float *out,const float *in,uint64_t bytes,bool shadow,
    const float inv_native[16],const float correction[16]) {
    memcpy(out,in,bytes);unsigned changes=0;
    if(shadow && bytes==608 && vr_matrix_matches(in+24,inv_native) &&
       isfinite(in[0]) && in[0]>0 && isfinite(in[2]) && isfinite(in[3]) &&
       fabsf(in[2]-inv_native[0])<.0001f && fabsf(in[3]-inv_native[5])<.0001f) {
        // The captured shadow resolve reconstructs XYZ from focal scalars,
        // then multiplies by this view-to-shadow matrix; it does not read the
        // stored inverse projection. Correct its input space before lookup.
        float m[16],patched[16];vr_rows_to_matrix(m,in+40);
        vr_multiply(patched,m,correction);vr_matrix_to_rows(out+40,patched);changes++;
    }
    return changes;
}

// The traced ambient-shadow atlas pass reconstructs view XYZ from the linear
// depth buffer, then consumes only these three inverse-view rows (CB4[3..5]).
// Its stored inverse projection is validation data, not a matrix to rewrite.
static unsigned vr_ambient_reconstruction_patch(float *out,const float *in,uint64_t bytes,
    const float inv_native[16],const float correction[16]) {
    memcpy(out,in,bytes);
    if(bytes<384 || bytes>4800 || !vr_matrix_matches(in+52,inv_native) ||
       !isfinite(in[92]) || !isfinite(in[93]) ||
       fabsf(in[92]-inv_native[0])>.0001f || fabsf(in[93]+inv_native[5])>.0001f)return 0;
    float m[16],patched[16];vr_identity(m);
    for(int r=0;r<3;r++)for(int c=0;c<4;c++) {
        float v=in[12+r*4+c];if(!isfinite(v))return 0;m[c*4+r]=v;
    }
    vr_multiply(patched,m,correction);
    for(int r=0;r<3;r++)for(int c=0;c<4;c++)out[12+r*4+c]=patched[c*4+r];
    return 1;
}
