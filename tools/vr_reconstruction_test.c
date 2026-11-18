#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_camera_math.h"
#include "../runtime/nvn/vulkan/vr_reconstruction.h"
static void transform(float *o,const float *m,const float *p){for(int r=0;r<4;r++){o[r]=0;for(int c=0;c<4;c++)o[r]+=m[c*4+r]*p[c];}}
static void near(float a,float b){assert(isfinite(a)&&isfinite(b)&&fabsf(a-b)<.003f*fmaxf(1,fabsf(b)));}
int main(void){
    unsigned checks=0;
    for(int cant=0;cant<=25;cant+=5)for(int eye=0;eye<2;eye++)for(int reverse=0;reverse<2;reverse++) {
        float p[16],q[16],v[16],ip[16],clip[16],tmp[16];
        vr_projection(p,-.9f,.9f,.82f,-.82f,25,500000);
        if(reverse){p[10]=-1-p[10];p[14]=-p[14];}
        vr_eye_projection(q,eye?-.67f:-1.04f,eye?1.04f:.67f,.79f,-.75f,25,500000);
        q[10]=p[10];q[14]=p[14];
        float a=cant*.00872664626f;
        vr_pose_inverse(v,0,eye?-sinf(a):sinf(a),0,cosf(a),eye?3.2f:-3.2f,0,0);
        assert(vr_inverse(ip,p));vr_multiply(tmp,v,ip);vr_multiply(clip,q,tmp);
        float native[16],inv_native[16],actual[16],correction[16],from_vk[16];
        vr_identity(from_vk);from_vk[10]=2;from_vk[14]=-1;
        vr_multiply(native,from_vk,p);vr_multiply(tmp,q,v);vr_multiply(actual,from_vk,tmp);
        assert(vr_shadow_view_correction(inv_native,correction,p,clip));
        float block[152]={0},patched[152],before[152],light[16];vr_identity(light);
        light[0]=.00017f;light[5]=.00023f;light[10]=.0001f;light[12]=.4f;light[13]=.6f;
        block[0]=25;block[2]=inv_native[0];block[3]=inv_native[5];
        vr_matrix_to_rows(block+24,inv_native);vr_matrix_to_rows(block+40,light);
        memcpy(before,block,sizeof block);
        assert(vr_reconstruction_patch(patched,block,sizeof block,true,inv_native,correction)==1);
        assert(!memcmp(block,before,sizeof block));
        assert(!memcmp(block,patched,40*sizeof(float)));
        assert(!memcmp(block+56,patched+56,sizeof block-56*sizeof(float)));
        float ambient[1200]={0},ambient_out[1200],world[16];vr_identity(world);
        world[12]=123;world[13]=-70;world[14]=1234;
        for(int r=0;r<3;r++)for(int c=0;c<4;c++)ambient[12+r*4+c]=world[c*4+r];
        vr_matrix_to_rows(ambient+52,inv_native);ambient[92]=inv_native[0];ambient[93]=-inv_native[5];
        assert(vr_ambient_reconstruction_patch(ambient_out,ambient,sizeof ambient,inv_native,correction)==1);
        assert(!memcmp(ambient,ambient_out,12*sizeof(float)));
        assert(!memcmp(ambient+24,ambient_out+24,sizeof ambient-24*sizeof(float)));
        float ambient_world[16];vr_identity(ambient_world);
        for(int r=0;r<3;r++)for(int c=0;c<4;c++)ambient_world[c*4+r]=ambient_out[12+r*4+c];
        float shadow[16];vr_rows_to_matrix(shadow,patched+40);
        for(int depth=0;depth<3;depth++) {
            float point[4]={eye?20.f:-20.f,10,-(depth==0?50.f:depth==1?200.f:1000.f),1};
            float eye_clip[4],wrong[4],expected[4],result[4];
            transform(eye_clip,actual,point);
            // Reproduce the scalar-focal reconstruction used by the shadow
            // shader, which mistakes an eye clip coordinate for native clip.
            transform(wrong,inv_native,eye_clip);for(int i=0;i<4;i++)wrong[i]/=wrong[3];
            transform(result,shadow,wrong);transform(expected,light,point);
            for(int i=0;i<4;i++)near(result[i],expected[i]);
            transform(result,ambient_world,wrong);transform(expected,world,point);
            for(int i=0;i<4;i++)near(result[i],expected[i]);
            checks++;
        }
        float data[80]={0},out[80];vr_matrix_to_rows(data+4,native);vr_matrix_to_rows(data+24,inv_native);data[75]=19;
        assert(!vr_reconstruction_patch(out,data,sizeof data,false,inv_native,correction));
        assert(!memcmp(data,out,sizeof data));
        assert(out[75]==19);
        assert(!vr_reconstruction_patch(patched,block,sizeof block,false,inv_native,correction));
        assert(!memcmp(block,patched,sizeof block));
        memset(data,0,sizeof data);data[4]=NAN;
        assert(!vr_reconstruction_patch(out,data,sizeof data,false,inv_native,correction));
        assert(isnan(out[4]));
    }
    printf("%u per-eye shadow and ambient-atlas lookup checks pass; ray/projection inputs and other screen effects remain consistent\n",checks);
}
