#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_camera_math.h"
static void close_value(float a,float b) { assert(fabsf(a-b)<.0001f); }
int main(void) {
    XrPosef pa={.orientation={0,0,0,1},.position={0,0,0}},pb={.orientation={0,0,0,-1},.position={2,4,6}};
    XrPosef mid=vr_pose_blend(pa,pb,.5f);
    close_value(mid.orientation.w,1); close_value(mid.position.x,1); close_value(mid.position.y,2); close_value(mid.position.z,3);
    close_value(vr_pose_blend(pa,pb,0).position.x,0); close_value(vr_pose_blend(pa,pb,1).position.x,2);
    XrPosef still={.orientation={0,0,0,1}},moved=still;
    assert(!vr_head_changed(still,still));
    moved.orientation.w=-1; assert(!vr_head_changed(still,moved));
    moved=still; moved.position.x=.003f; assert(vr_head_changed(still,moved));
    moved=still; moved.position.x=.0001f; assert(!vr_head_changed(still,moved));
    moved=still; moved.orientation=(XrQuaternionf){0,sinf(.01f),0,cosf(.01f)};
    assert(vr_head_changed(still,moved));
    XrPosef origin={.orientation={0,sinf(.2f),0,cosf(.2f)},.position={1,2,3}};
    XrPosef local={.orientation={0,0,0,1},.position={.032f,.01f,0}};
    XrPosef combined=pose_compose(origin,local);
    XrPosef recovered=pose_relative(origin,combined);
    close_value(recovered.position.x,local.position.x);
    close_value(recovered.position.y,local.position.y);
    close_value(recovered.position.z,local.position.z);
    close_value(recovered.orientation.w,1);
    XrPosef head={.orientation={0,sinf(.1f),0,cosf(.1f)},.position={.032f,.01f,0}};
    float base[21]={1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0, 0,0,-100, 0,1,0};
    float fields[21],again[21],unchanged[21]; memcpy(unchanged,base,sizeof base);
    assert(vr_lookat_head(fields,base,head,100));
    assert(!memcmp(base,unchanged,sizeof base));
    close_value(fields[12],3.2f); close_value(fields[13],1); close_value(fields[14],0);
    for(int r=0;r<3;r++) {
        float position=fields[r*4+3],target=fields[r*4+3];
        for(int c=0;c<3;c++) {
            position+=fields[r*4+c]*fields[12+c];
            target+=fields[r*4+c]*fields[15+c];
        }
        close_value(position,0); close_value(target,r==2?-100:0);
    }
    assert(vr_lookat_head(again,base,head,100));
    assert(!memcmp(fields,again,sizeof fields));
    head=(XrPosef){.orientation={0,0,0,1}};
    assert(vr_lookat_head(fields,base,head,100));
    for(int i=0;i<21;i++) close_value(fields[i],base[i]);
    float invalid[21]={0}; assert(!vr_lookat_head(fields,invalid,head,100));
    XrView eyes[2]={0};
    for(int i=0;i<2;i++) {
        eyes[i].pose.orientation.w=1;
        eyes[i].fov=(XrFovf){-.8f,.8f,.7f,-.7f};
    }
    XrFovf enclosing; assert(vr_enclosing_fov(&enclosing,eyes));
    close_value(enclosing.angleRight,.8f); close_value(enclosing.angleUp,.7f);
    eyes[1].pose.orientation=(XrQuaternionf){0,sinf(.1f),0,cosf(.1f)};
    assert(vr_enclosing_fov(&enclosing,eyes));
    assert(enclosing.angleRight>.999f && enclosing.angleRight<1.001f);
    puts("Native camera and OpenXR pose/frustum tests passed");
}
