#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_first_person.h"
#include "../runtime/nvn/vulkan/vr_camera_math.h"
static void near(float a,float b){assert(fabsf(a-b)<.0001f);}
int main(void){
    float base[21]={1,0,0,0,0,1,0,-200,0,0,1,-500,0,200,500,0,200,400,0,1,0};
    float saved[21],out[21];memcpy(saved,base,sizeof base);
    float eye[3]={10,150,20},up[3]={0,1,0},front[3]={0,0,-1};
    assert(vr_first_person_base(out,base,eye,up,front));
    for(int i=0;i<3;i++){near(out[12+i],eye[i]);float p=out[i*4+3];for(int j=0;j<3;j++)p+=out[i*4+j]*eye[j];near(p,0);}
    near(out[16],150);near(out[17],-80);assert(!memcmp(base,saved,sizeof base));
    base[9]=.5f;base[10]=.8660254f;
    assert(vr_first_person_base(out,base,eye,up,front));near(out[9],0);near(out[10],1);
    up[1]=0;up[2]=1;front[0]=1;front[2]=0;
    base[8]=0;base[9]=0;base[10]=1;
    assert(vr_first_person_base(out,base,eye,up,front));near(out[8],-1);near(out[18],0);near(out[20],1);
    float head_view[21];XrPosef head={.orientation={0,0,0,1},.position={.032f,0,0}};
    assert(vr_lookat_head(head_view,out,head,100));
    near(head_view[12],out[12]+out[0]*3.2f);near(head_view[13],out[13]+out[1]*3.2f);
    up[0]=up[1]=up[2]=0;assert(!vr_first_person_base(out,base,eye,up,front));
    up[1]=1;eye[0]=NAN;assert(!vr_first_person_base(out,base,eye,up,front));
    puts("First-person eye placement, gravity horizon, fallback and head-transform tests passed");
}
