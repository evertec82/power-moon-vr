#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_hud.h"
static void near(float a,float b){assert(isfinite(a)&&isfinite(b)&&fabsf(a-b)<.0008f);}
static void inverse_pose(float* m,XrPosef p){vr_pose_inverse(m,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,p.position.x,p.position.y,p.position.z);}
static void project(float out[2],const float m[16],const float p[4]){float c[4]={0};for(int r=0;r<4;r++)for(int k=0;k<4;k++)c[r]+=m[k*4+r]*p[k];out[0]=c[0]/c[3];out[1]=c[1]/c[3];}
int main(void){
 unsigned cases=0;
 for(int cant=0;cant<=25;cant+=5)for(int ipd=58;ipd<=74;ipd+=8)for(int motion=0;motion<4;motion++){
  float angle=cant*.01745329252f,half=angle*.5f;XrView eyes[2]={0};
  XrPosef head={.orientation={sinf(.07f),0,0,cosf(.07f)},.position={.13f,1.6f,-.2f}};
  XrPosef recorded={.orientation={0,sinf(.08f*motion),0,cosf(.08f*motion)},.position={.1f,1.5f,0}};
  XrPosef origin={.orientation={0,sinf(.12f),0,cosf(.12f)},.position={2,0,-1}};
  for(int i=0;i<2;i++){
   XrPosef local={.orientation={0,i?-sinf(half):sinf(half),0,cosf(half)},.position={i?ipd*.0005f:-ipd*.0005f,0,0}};
   eyes[i].pose=pose_compose(head,local);eyes[i].fov=i?(XrFovf){-.67f,1.04f,.82f,-.71f}:(XrFovf){-1.04f,.67f,.79f,-.75f};
  }
  XrPosef center=vr_head_from_views(eyes);near(center.position.x,head.position.x);near(center.position.y,head.position.y);near(center.orientation.x,head.orientation.x);near(center.orientation.y,head.orientation.y);near(center.orientation.w,head.orientation.w);
  XrView neg[2]={eyes[0],eyes[1]};neg[1].pose.orientation.x*=-1;neg[1].pose.orientation.y*=-1;neg[1].pose.orientation.z*=-1;neg[1].pose.orientation.w*=-1;
  XrPosef signed_center=vr_head_from_views(neg);near(signed_center.orientation.w,center.orientation.w);
  XrFovf native;assert(vr_enclosing_fov(&native,eyes));float base_p[16],base_v[16],base_clip[16],inv_p[16];vr_projection(base_p,native.angleLeft,native.angleRight,native.angleUp,native.angleDown,.1f,10000);inverse_pose(base_v,recorded);vr_multiply(base_clip,base_p,base_v);assert(vr_inverse(inv_p,base_p));
  for(int late=0;late<2;late++)for(int eye=0;eye<2;eye++){
   XrPosef local=pose_relative(head,eyes[eye].pose),delta=vr_replay_eye_delta(recorded,head,eyes[eye].pose,true,late);
   XrPosef expected_eye=pose_compose(late?head:recorded,local),submitted=vr_replay_eye_pose(origin,recorded,head,eyes[eye].pose,late);
   float eye_v[16],delta_v[16],q[16],tmp[16],clip_delta[16],actual[16],expected[16];inverse_pose(eye_v,expected_eye);inverse_pose(delta_v,delta);XrFovf f=eyes[eye].fov;vr_eye_projection(q,f.angleLeft,f.angleRight,f.angleUp,f.angleDown,.1f,10000);
   vr_multiply(tmp,delta_v,inv_p);vr_multiply(clip_delta,q,tmp);vr_multiply(actual,clip_delta,base_clip);vr_multiply(expected,q,eye_v);
   float submitted_v[16];inverse_pose(submitted_v,pose_relative(origin,submitted));for(int j=0;j<16;j++)near(submitted_v[j],eye_v[j]);
   for(int depth=0;depth<3;depth++){float point[4]={.15f,-.08f,-(depth==0?.75f:depth==1?3.f:1000.f),1},a[2],b[2];project(a,actual,point);project(b,expected,point);near(a[0],b[0]);near(a[1],b[1]);cases++;}
   float hud[16];vr_hud_eye_matrix(hud,expected_eye,native,f,.65f);for(int j=0;j<16;j++)assert(isfinite(hud[j]));
   float hud_center[4]={0,0,0,1},panel_center[4]={0,0,-2,1},huv[2],puv[2],hud_p[16],hud_expected[16];
   vr_eye_projection(hud_p,f.angleLeft,f.angleRight,f.angleUp,f.angleDown,.01f,100);vr_multiply(hud_expected,hud_p,eye_v);
   project(huv,hud,hud_center);project(puv,hud_expected,panel_center);near(huv[0],puv[0]);near(huv[1],puv[1]);
   // Frustum boundary rays map to the exact runtime eye's image edges.
   float ray[4]={tanf(f.angleLeft),tanf(f.angleDown),-1,0},ndc[2];project(ndc,q,ray);near(ndc[0],-1);near(ndc[1],-1);
   ray[0]=tanf(f.angleRight);ray[1]=tanf(f.angleUp);project(ndc,q,ray);near(ndc[0],1);near(ndc[1],1);
  }
 }
 puts("Canted/asymmetric stereo: centered head, signed quaternions, IPD, near/far projection, render/submission pose and HUD checks pass");printf("%u stereo point checks\n",cases);
}
