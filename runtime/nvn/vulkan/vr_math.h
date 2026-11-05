#pragma once
#include <math.h>
#include <string.h>
static void vr_identity(float *m) { memset(m, 0, 64); m[0]=m[5]=m[10]=m[15]=1; }
static void vr_multiply(float *o, const float *a, const float *b) {
    float t[16];
    for(int c=0;c<4;c++) for(int r=0;r<4;r++) {
        t[c*4+r]=0; for(int k=0;k<4;k++) t[c*4+r]+=a[k*4+r]*b[c*4+k];
    }
    memcpy(o,t,64);
}
static bool vr_inverse(float *o, const float *m) {
    double a[4][8];
    for(int r=0;r<4;r++) for(int c=0;c<8;c++) a[r][c]=c<4?m[c*4+r]:(c-4==r);
    for(int c=0;c<4;c++) {
        int p=c; for(int r=c+1;r<4;r++) if(fabs(a[r][c])>fabs(a[p][c])) p=r;
        if(fabs(a[p][c])<1e-12) return false;
        for(int j=0;j<8;j++) { double t=a[p][j]; a[p][j]=a[c][j]; a[c][j]=t; }
        double v=a[c][c]; for(int j=0;j<8;j++) a[c][j]/=v;
        for(int r=0;r<4;r++) if(r!=c) { v=a[r][c]; for(int j=0;j<8;j++) a[r][j]-=v*a[c][j]; }
    }
    for(int r=0;r<4;r++) for(int c=0;c<4;c++) o[c*4+r]=(float)a[r][c+4];
    return true;
}
// Right handed, -Z forward, Vulkan 0..1 depth, matching OpenXR's angular FOV.
static void vr_projection(float *m,float l,float r,float u,float d,float nearz,float farz) {
    l=tanf(l); r=tanf(r); u=tanf(u); d=tanf(d);
    memset(m,0,64); m[0]=2/(r-l); m[5]=2/(u-d);
    m[8]=(r+l)/(r-l); m[9]=(u+d)/(u-d);
    m[10]=-farz/(farz-nearz); m[11]=-1; m[14]=-farz*nearz/(farz-nearz);
}
// exec.c's set_viewport uses height=-h, so this eye projection must keep Y pointing up.
// Applying the usual positive-height Vulkan projection flip here would invert pitch and the image.
static void vr_eye_projection(float *m,float l,float r,float u,float d,float nearz,float farz) {
    vr_projection(m,l,r,u,d,nearz,farz);
}
static void vr_pose_inverse(float *m,float x,float y,float z,float w,float px,float py,float pz) {
    vr_identity(m);
    // Transpose of quaternion rotation: world to eye.
    m[0]=1-2*(y*y+z*z); m[4]=2*(x*y+z*w); m[8]=2*(x*z-y*w);
    m[1]=2*(x*y-z*w); m[5]=1-2*(x*x+z*z); m[9]=2*(y*z+x*w);
    m[2]=2*(x*z+y*w); m[6]=2*(y*z-x*w); m[10]=1-2*(x*x+y*y);
    for(int r=0;r<3;r++) m[12+r]=-(m[r]*px+m[4+r]*py+m[8+r]*pz);
}
