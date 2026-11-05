#pragma once
#include <math.h>
#include <stdbool.h>
#include <string.h>
static float vr_fp_dot(const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static bool vr_fp_normalize(float *v){float n=sqrtf(vr_fp_dot(v,v));if(!isfinite(n)||n<.00001f)return false;for(int i=0;i<3;i++)v[i]/=n;return true;}
static void vr_fp_cross(float *o,const float *a,const float *b){o[0]=a[1]*b[2]-a[2]*b[1];o[1]=a[2]*b[0]-a[0]*b[2];o[2]=a[0]*b[1]-a[1]*b[0];}
// Keep controller camera yaw, level the view against gravity, then let the
// existing head transform supply physical pitch/roll/translation.
static bool vr_first_person_base(float out[21],const float base[21],const float eye[3],const float up[3],const float front[3]) {
    for(int i=0;i<21;i++)if(!isfinite(base[i]))return false;
    for(int i=0;i<3;i++)if(!isfinite(eye[i])||fabsf(eye[i])>1e7f)return false;
    float u[3],f[3]={-base[8],-base[9],-base[10]},r[3];memcpy(u,up,sizeof u);
    if(!vr_fp_normalize(u))return false;
    float d=vr_fp_dot(f,u);for(int i=0;i<3;i++)f[i]-=u[i]*d;
    if(!vr_fp_normalize(f)) {
        memcpy(f,front,sizeof f);d=vr_fp_dot(f,u);for(int i=0;i<3;i++)f[i]-=u[i]*d;
        if(!vr_fp_normalize(f))return false;
    }
    vr_fp_cross(r,f,u);if(!vr_fp_normalize(r))return false;
    vr_fp_cross(u,r,f);
    for(int i=0;i<3;i++){out[i]=r[i];out[4+i]=u[i];out[8+i]=-f[i];out[12+i]=eye[i];out[15+i]=eye[i]+f[i]*100;out[18+i]=u[i];}
    out[3]=-vr_fp_dot(r,eye);out[7]=-vr_fp_dot(u,eye);out[11]=vr_fp_dot(f,eye);
    return true;
}
