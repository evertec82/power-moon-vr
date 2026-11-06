#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
// Game vibration threads publish here; only the render owner calls OpenXR.
void vr_input_vibration(int side,float low,float high);
void vr_input_stop_vibration(void);
typedef struct {uint64_t buttons;int32_t lx,ly,rx,ry;uint64_t stamp;} VrControllerState;
typedef struct {bool primary[2],secondary[2],menu[2],click[2];float trigger[2],grip[2],stick[2][2];} VrControllerRaw;
static void vr_controller_stick(float x,float y,int32_t *ox,int32_t *oy){
 *ox=*oy=0;if(!isfinite(x)||!isfinite(y))return;float r=sqrtf(x*x+y*y);if(r<=.18f)return;
 float k=fminf((r-.18f)/.82f,1)/r;*ox=(int32_t)lroundf(x*k*32767);*oy=(int32_t)lroundf(y*k*32767);
}
static VrControllerState vr_controller_convert(const VrControllerRaw *r){
 VrControllerState s={0};
 if(r->primary[0])s.buttons|=1ull<<1;if(r->primary[1])s.buttons|=1ull<<0;
 if(r->secondary[0])s.buttons|=1ull<<2;if(r->secondary[1])s.buttons|=1ull<<3;
 for(int h=0;h<2;h++){if(r->click[h])s.buttons|=1ull<<(4+h);if(r->trigger[h]>.55f)s.buttons|=1ull<<(8+h);if(r->grip[h]>.65f)s.buttons|=1ull<<(6+h);if(r->menu[h])s.buttons|=1ull<<10;}
 if(r->secondary[0]&&r->secondary[1]){s.buttons&=~((1ull<<2)|(1ull<<3));s.buttons|=1ull<<10;}
 vr_controller_stick(r->stick[0][0],r->stick[0][1],&s.lx,&s.ly);vr_controller_stick(r->stick[1][0],r->stick[1][1],&s.rx,&s.ry);
 if(r->grip[0]>.65f&&r->grip[1]>.65f){s.buttons&=~((1ull<<6)|(1ull<<7));s.buttons|=1ull<<11;}
 else if(r->grip[0]>.65f&&(abs(s.rx)>20000||abs(s.ry)>20000)){
  if(s.rx< -20000)s.buttons|=1ull<<12;if(s.ry>20000)s.buttons|=1ull<<13;if(s.rx>20000)s.buttons|=1ull<<14;if(s.ry< -20000)s.buttons|=1ull<<15;
  s.buttons&=~(1ull<<6);s.rx=s.ry=0;}
 return s;
}
static void vr_merge_stick(int32_t *x,int32_t *y,int32_t vx,int32_t vy){
 int64_t ax=*x,ay=*y;int64_t a=ax*ax+ay*ay,b=(int64_t)vx*vx+(int64_t)vy*vy;if(b>a){*x=vx;*y=vy;}
}
static bool vr_controller_fresh(uint64_t stamp,uint64_t now){return stamp&&now>=stamp&&now-stamp<=UINT64_C(250000000);}
void vr_input_read(VrControllerState *out);
