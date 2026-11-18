#pragma once
#include "vr_camera_math.h"
// Map a runtime eye clip coordinate into a completed image rendered with the
// native head-aligned projection. IPD translation is already in that image;
// this transform changes only angular projection and eye cant.
static bool vr_image_projection(float out[16],const float native[16],const float eye[16],XrQuaternionf cant) {
    float inverse[16],rotation[16],tmp[16];
    if(!vr_inverse(inverse,eye))return false;
    vr_pose_inverse(rotation,-cant.x,-cant.y,-cant.z,cant.w,0,0,0);
    vr_multiply(tmp,rotation,inverse);vr_multiply(out,native,tmp);
    return true;
}
