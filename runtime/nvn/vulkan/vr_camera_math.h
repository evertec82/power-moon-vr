#pragma once
#include <stdbool.h>
#include <openxr/openxr.h>
#include "vr_math.h"
static XrPosef pose_relative(XrPosef origin,XrPosef p) {
    XrQuaternionf a=origin.orientation,b=p.orientation;
    a.x=-a.x; a.y=-a.y; a.z=-a.z;
    XrPosef o;
    o.orientation=(XrQuaternionf){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
    float r[16];
    vr_pose_inverse(r,origin.orientation.x,origin.orientation.y,origin.orientation.z,origin.orientation.w,0,0,0);
    float x=p.position.x-origin.position.x,y=p.position.y-origin.position.y,z=p.position.z-origin.position.z;
    o.position=(XrVector3f){r[0]*x+r[4]*y+r[8]*z,r[1]*x+r[5]*y+r[9]*z,r[2]*x+r[6]*y+r[10]*z};
    return o;
}

static XrPosef pose_compose(XrPosef a,XrPosef b) {
    XrQuaternionf q=a.orientation,r=b.orientation;
    XrPosef p;
    p.orientation=(XrQuaternionf){q.w*r.x+q.x*r.w+q.y*r.z-q.z*r.y,
        q.w*r.y-q.x*r.z+q.y*r.w+q.z*r.x,
        q.w*r.z+q.x*r.y-q.y*r.x+q.z*r.w,
        q.w*r.w-q.x*r.x-q.y*r.y-q.z*r.z};
    float m[16];
    vr_pose_inverse(m,-q.x,-q.y,-q.z,q.w,0,0,0);
    p.position=(XrVector3f){a.position.x+m[0]*b.position.x+m[4]*b.position.y+m[8]*b.position.z,
        a.position.y+m[1]*b.position.x+m[5]*b.position.y+m[9]*b.position.z,
        a.position.z+m[2]*b.position.x+m[6]*b.position.y+m[10]*b.position.z};
    return p;
}

static bool vr_lookat_head(float out[21],const float base[21],XrPosef head,float scale) {
    for(int i=0;i<21;i++) if(!isfinite(base[i])) return false;
    for(int r=0;r<3;r++) {
        float norm=0;
        for(int c=0;c<3;c++) norm+=base[r*4+c]*base[r*4+c];
        if(fabsf(norm-1)>.005f) return false;
    }
    float original[16],inverse[16]; vr_identity(original);
    for(int r=0;r<3;r++) for(int c=0;c<4;c++) original[c*4+r]=base[r*4+c];
    if(!vr_inverse(inverse,original)) return false;
    for(int r=0;r<3;r++) if(fabsf(inverse[12+r]-base[12+r])>.1f) return false;
    float h_inv[16],view[16],world[16];
    vr_pose_inverse(h_inv,head.orientation.x,head.orientation.y,head.orientation.z,head.orientation.w,
                    head.position.x*scale,head.position.y*scale,head.position.z*scale);
    vr_multiply(view,h_inv,original);
    if(!vr_inverse(world,view)) return false;
    float distance=0;
    for(int r=0;r<3;r++) { float d=base[15+r]-base[12+r]; distance+=d*d; }
    distance=sqrtf(distance);
    if(distance<.01f) return false;
    for(int r=0;r<3;r++) {
        for(int c=0;c<4;c++) out[r*4+c]=view[c*4+r];
        out[12+r]=world[12+r];
        out[15+r]=world[12+r]-world[8+r]*distance;
        out[18+r]=world[4+r];
    }
    return true;
}

// A shared symmetric render frustum lets native screen-space effects use the same
// projection in both eyes. OpenXR receives this actual render FOV and camera pose.
static bool vr_enclosing_fov(XrFovf *out,const XrView views[2]) {
    float h=0,v=0;
    for(int i=0;i<2;i++) {
        XrPosef eye=pose_relative(views[0].pose,views[i].pose);
        XrQuaternionf q=eye.orientation;
        float rotation[16]; vr_pose_inverse(rotation,-q.x,-q.y,-q.z,q.w,0,0,0);
        float xs[2]={tanf(views[i].fov.angleLeft),tanf(views[i].fov.angleRight)};
        float ys[2]={tanf(views[i].fov.angleDown),tanf(views[i].fov.angleUp)};
        for(int x=0;x<2;x++) for(int y=0;y<2;y++) {
            float a=rotation[0]*xs[x]+rotation[4]*ys[y]-rotation[8];
            float b=rotation[1]*xs[x]+rotation[5]*ys[y]-rotation[9];
            float z=rotation[2]*xs[x]+rotation[6]*ys[y]-rotation[10];
            if(z>=-.001f) return false;
            h=fmaxf(h,atan2f(fabsf(a),-z));
            v=fmaxf(v,atan2f(fabsf(b),-z));
        }
    }
    if(h<.1f||v<.1f||h>1.5f||v>1.5f) return false;
    *out=(XrFovf){-h,h,v,-v};
    return true;
}

static XrPosef vr_pose_blend(XrPosef a,XrPosef b,float t) {
    float dot=a.orientation.x*b.orientation.x+a.orientation.y*b.orientation.y+
              a.orientation.z*b.orientation.z+a.orientation.w*b.orientation.w;
    float sign=dot<0?-1:1,u=1-t;
    XrPosef out;
    out.orientation=(XrQuaternionf){u*a.orientation.x+t*sign*b.orientation.x,
        u*a.orientation.y+t*sign*b.orientation.y,u*a.orientation.z+t*sign*b.orientation.z,
        u*a.orientation.w+t*sign*b.orientation.w};
    float norm=sqrtf(out.orientation.x*out.orientation.x+out.orientation.y*out.orientation.y+
                     out.orientation.z*out.orientation.z+out.orientation.w*out.orientation.w);
    if(norm>.00001f) {
        out.orientation.x/=norm; out.orientation.y/=norm; out.orientation.z/=norm; out.orientation.w/=norm;
    } else out.orientation=(XrQuaternionf){0,0,0,1};
    out.position=(XrVector3f){u*a.position.x+t*b.position.x,u*a.position.y+t*b.position.y,u*a.position.z+t*b.position.z};
    return out;
}

// Ignore tracking noise; a visible head turn or translation needs one consistent native camera.
static bool vr_head_changed(XrPosef a,XrPosef b) {
    float dot=fabsf(a.orientation.x*b.orientation.x+a.orientation.y*b.orientation.y+
                    a.orientation.z*b.orientation.z+a.orientation.w*b.orientation.w);
    float dx=a.position.x-b.position.x,dy=a.position.y-b.position.y,dz=a.position.z-b.position.z;
    return dot<.99999848f || dx*dx+dy*dy+dz*dz>0.000004f;
}
