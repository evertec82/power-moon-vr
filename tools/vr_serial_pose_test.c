#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_camera_math.h"
static void near(float a,float b){assert(fabsf(a-b)<.00002f);}
static void view(float *m,XrPosef p){vr_pose_inverse(m,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,p.position.x,p.position.y,p.position.z);}
int main(void){
 XrPosef origin={.orientation={0,sinf(.2f),0,cosf(.2f)},.position={2,1,-3}};
 XrPosef recorded={.orientation={sinf(.1f),0,0,cosf(.1f)},.position={.1f,.2f,.3f}};
 for(int frame=0;frame<100;frame++){
  float yaw=sinf(frame*.13f)*.5f;XrPosef head={.orientation={0,sinf(yaw),0,cosf(yaw)},.position={frame*.002f,.15f,-.1f}};
  for(int i=0;i<2;i++){
   XrPosef local={.orientation={0,sinf(.02f),0,cosf(.02f)},.position={i?.032f:-.032f,0,0}};
   XrPosef eye=pose_compose(head,local);local.orientation=(XrQuaternionf){0,0,0,1};
   XrPosef want=pose_compose(head,local);
   XrPosef delta=vr_replay_eye_delta(recorded,head,eye,true,true);
   float a[16],b[16],actual[16],expected[16];view(a,delta);view(b,recorded);vr_multiply(actual,a,b);view(expected,want);
   for(int j=0;j<16;j++)near(actual[j],expected[j]);
   XrPosef submitted=vr_replay_eye_pose(origin,recorded,head,eye,true);
   XrPosef relative=pose_relative(origin,submitted);view(actual,relative);for(int j=0;j<16;j++)near(actual[j],expected[j]);
   XrPosef source_eye=pose_compose(recorded,local);
   XrPosef fixed_delta=vr_replay_eye_delta(recorded,head,eye,true,false);
   view(a,fixed_delta);view(b,recorded);vr_multiply(actual,a,b);view(expected,source_eye);
   for(int j=0;j<16;j++)near(actual[j],expected[j]);
   XrPosef fixed_pose=vr_replay_eye_pose(origin,recorded,head,eye,false);
   view(actual,pose_relative(origin,fixed_pose));for(int j=0;j<16;j++)near(actual[j],expected[j]);
   near(fixed_delta.orientation.w,1);near(fixed_delta.orientation.y,0);
   XrPosef hud=vr_replay_eye_delta(recorded,head,eye,false,true);near(hud.orientation.w,1);near(hud.position.x,local.position.x);
  }
 }
 puts("Recorded camera and submission stay consistent as prediction changes; geometry/shadow camera is not late-rotated; diagnostic late-pose math also passes");
}
