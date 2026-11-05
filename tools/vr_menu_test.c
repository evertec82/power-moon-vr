#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_menu.h"
static void near(float a,float b){assert(fabsf(a-b)<.00001f);}
int main(void) {
    XrPosef a={.orientation={0,0,0,1},.position={1,2,3}};
    XrPosef p=vr_menu_panel_pose(a,2);
    near(p.position.x,1);near(p.position.y,2);near(p.position.z,1);
    a.orientation=(XrQuaternionf){0,.707106781f,0,.707106781f};
    p=vr_menu_panel_pose(a,2);
    near(p.position.x,-1);near(p.position.y,2);near(p.position.z,3);
    near(p.orientation.y,a.orientation.y);near(p.orientation.w,a.orientation.w);
    a.orientation=(XrQuaternionf){.707106781f,0,0,.707106781f};
    p=vr_menu_panel_pose(a,2);near(p.position.y,4);near(p.position.z,3);
    puts("Stationary menu anchor/recenter tests passed");
}
