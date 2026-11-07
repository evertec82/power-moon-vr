#include <assert.h>
#include <string.h>
#include "../runtime/hle.h"
static bool active=true,missing;
static int installed,calls;
static HostFn hook;
bool vr_enabled(void){return active;}
static void original(Cpu *c){calls++;c->x[0]=1234;}
uint64_t loader_find_symbol(const char *name){assert(strstr(name,"OfxLensFlare9drawImpl_"));return missing?0:0x7000bd1d64;}
GuestFn dispatch_lookup(uint64_t address){assert(address==0x7000bd1d64);return original;}
void hle_override(uint64_t address,HostFn fn,const char *name){assert(address==0x7000bd1d64&&name);hook=fn;installed++;}
#include "../runtime/hle/odyssey_vr_lighting.c"
int main(int argc,char **argv){
 assert(argc==2);const char *mode=argv[1];
 if(!strcmp(mode,"desktop")){active=false;_putenv_s("ODYSSEY_VR_LIGHTING_DIAG","no-lens-flare");}
 else if(!strcmp(mode,"missing")){missing=true;_putenv_s("ODYSSEY_VR_LIGHTING_DIAG","no-lens-flare");}
 else if(strcmp(mode,"unset"))_putenv_s("ODYSSEY_VR_LIGHTING_DIAG",mode);
 else _putenv_s("ODYSSEY_VR_LIGHTING_DIAG","");
 _putenv_s("ODYSSEY_VR",active?"1":"0");odyssey_vr_lighting_init();
 if(!strcmp(mode,"trace")||!strcmp(mode,"no-lens-flare")){
  assert(installed==1&&hook);Cpu c={0},before;c.x[0]=99;c.x[30]=0x4567;c.sp=0x1230;before=c;hook(&c);
  if(!strcmp(mode,"trace"))assert(calls==1&&c.x[0]==1234&&c.x[30]==before.x[30]&&c.sp==before.sp);
  else assert(!calls&&!memcmp(&c,&before,sizeof c));
  _putenv_s("ODYSSEY_VR",active?"1":"0");odyssey_vr_lighting_init();assert(installed==1);
 }else assert(!installed&&!hook&&!calls);
 return 0;
}