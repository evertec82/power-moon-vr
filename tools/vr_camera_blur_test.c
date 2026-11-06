#ifdef _WIN32
#include <assert.h>
#include <windows.h>
#include "../runtime/hle.h"
static bool vr=true;static int calls;
static uint64_t scene,param;
bool vr_enabled(void){return vr;}
uint64_t loader_find_symbol(const char *n){assert(strstr(n,"invalidateCameraBlurSystem"));return 0x7000875c00;}
void guest_call(Cpu *c,uint64_t address){assert(address==0x7000875c00&&c->x[0]==scene);calls++;*(uint8_t *)(uintptr_t)(param+0x6c)=0;c->x[0]=123;c->sp-=16;}
#include "../runtime/hle/odyssey_vr_camera_blur.c"
int main(int argc,char **argv){
 bool enabled=argc>1&&!strcmp(argv[1],"on"); _putenv_s("ODYSSEY_VR_CAMERA_BLUR",enabled?"on":argc>1&&!strcmp(argv[1],"default")?"":"off");
 assert(VirtualAlloc((void *)HEAP_BASE,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)HEAP_BASE);
 scene=HEAP_BASE+0x1000;uint64_t holder=HEAP_BASE+0x2000,graphics=HEAP_BASE+0x3000,controller=HEAP_BASE+0x4000;param=HEAP_BASE+0x5000;
 ST64(scene+0x90,holder);ST64(holder+0x38,graphics);ST64(graphics+0x260,controller);ST64(controller+0x48,param);
 Cpu c={0},saved;c.x[0]=0x123;c.x[30]=0x456;c.sp=HEAP_BASE+0xf000;saved=c;
 *(uint8_t *)(uintptr_t)(param+0x6c)=1;odyssey_vr_disable_camera_blur(&c,scene);if(enabled){assert(calls==0&&!memcmp(&c,&saved,sizeof c)&&*(uint8_t *)(uintptr_t)(param+0x6c)==1);VirtualFree((void *)HEAP_BASE,0,MEM_RELEASE);puts("Explicit camera-blur opt-in preserves native behavior");return 0;} assert(calls==1&&!memcmp(&c,&saved,sizeof c)&&!*(uint8_t *)(uintptr_t)(param+0x6c));
 vr=false;odyssey_vr_disable_camera_blur(&c,scene);assert(calls==1);vr=true;
 odyssey_vr_disable_camera_blur(&c,HEAP_BASE+HEAP_MAX-0x90);assert(calls==1);
 ST64(controller+0x48,HEAP_BASE+HEAP_MAX-0x6c);odyssey_vr_disable_camera_blur(&c,scene);assert(calls==1);
 ST64(controller+0x48,param);ST64(graphics+0x260,HEAP_BASE+HEAP_MAX-0x4c);odyssey_vr_disable_camera_blur(&c,scene);assert(calls==1);
 ST64(graphics+0x260,controller);ST64(holder+0x38,0);odyssey_vr_disable_camera_blur(&c,scene);assert(calls==1);
 ST64(holder+0x38,graphics);ST64(scene+0x90,0);odyssey_vr_disable_camera_blur(&c,scene);assert(calls==1);
 assert(!memcmp(&c,&saved,sizeof c));VirtualFree((void *)HEAP_BASE,0,MEM_RELEASE);
 puts("Native blur control: valid call, desktop guard, invalid chains/bounds, and original CPU registers preserved");
}
#else
int main(void){return 0;}
#endif
