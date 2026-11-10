#pragma once
#include "vr_camera_math.h"

// Native UI clip coordinates become a panel in the untracked character camera.
// Use the rendered/submitted pose so compositor correction moves scene and HUD
// together. The game's camera movement naturally carries the panel with it.
static void vr_hud_eye_matrix(float out[16],XrPosef rendered_eye,XrFovf fov,XrFovf eye_fov,float size) {
    float panel[16],view[16],projection[16],tmp[16];
    const float distance=2;
    vr_identity(panel);
    panel[0]=distance*tanf(fov.angleRight)*size;
    panel[5]=distance*tanf(fov.angleUp)*size;
    panel[10]=0;panel[14]=-distance;
    vr_pose_inverse(view,rendered_eye.orientation.x,rendered_eye.orientation.y,
        rendered_eye.orientation.z,rendered_eye.orientation.w,
        rendered_eye.position.x,rendered_eye.position.y,rendered_eye.position.z);
    vr_eye_projection(projection,eye_fov.angleLeft,eye_fov.angleRight,eye_fov.angleUp,eye_fov.angleDown,.01f,100);
    vr_multiply(tmp,view,panel);vr_multiply(out,projection,tmp);
    for(int c=0;c<4;c++)out[c*4+2]=out[c*4+3]*.5f;
}
static void vr_hud_matrix(float out[16],XrPosef rendered_eye,XrFovf fov,float size) {
    vr_hud_eye_matrix(out,rendered_eye,fov,fov,size);
}
