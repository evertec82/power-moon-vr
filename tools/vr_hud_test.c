#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_hud.h"
static void near(float a,float b){assert(fabsf(a-b)<.0001f);}
static float point(float *m,int row,float x,float y){return (m[row]*x+m[4+row]*y+m[12+row])/(m[3]*x+m[7]*y+m[15]);}
int main(void){
    XrFovf f={-.9f,.9f,.8f,-.8f};
    XrPosef h={.orientation={0,0,0,1}};float m[16];
    vr_hud_matrix(m,h,f,.65f);
    near(point(m,0,1,0),.65f);near(point(m,1,0,1),.65f);
    near(point(m,0,0,0),0);near(point(m,2,1,1),.5f);
    h.orientation=(XrQuaternionf){0,sinf(.2f),0,cosf(.2f)};
    vr_hud_matrix(m,h,f,.65f);
    near(point(m,0,0,0),tanf(.4f)/tanf(.9f));
    h.orientation=(XrQuaternionf){sinf(.15f),0,0,cosf(.15f)};
    vr_hud_matrix(m,h,f,.65f);
    near(point(m,1,0,0),-tanf(.3f)/tanf(.8f));
    h=(XrPosef){.orientation={0,0,0,1},.position={.1f,.05f,0}};
    vr_hud_matrix(m,h,f,.65f);
    near(point(m,0,0,0),-.1f/(2*tanf(.9f)));
    near(point(m,1,0,0),-.05f/(2*tanf(.8f)));
    float eyes[2];for(int i=0;i<2;i++){
        h.position=(XrVector3f){i?.032f:-.032f,0,0};
        vr_hud_matrix(m,h,f,.65f);eyes[i]=point(m,0,0,0);
    }
    assert(eyes[0]>0&&eyes[1]<0);near(eyes[0],-eyes[1]);
    puts("Camera-fixed HUD: neutral size, yaw/pitch/translation, stereo disparity and clip depth pass");
}
