#include <assert.h>
#include "../runtime/hle.h"
#undef HLE_FUNC
#define HLE_FUNC(sym,fn) static void fn(Cpu *c)
static bool vr;static int notices;
bool vr_enabled(void){return vr;}
void menu_notice_run(const char *title,const char *text){assert(title&&text);notices++;}
#include "../runtime/hle/applets.c"
int main(void){Cpu c={0};uint64_t result=0;uint8_t arg[0x2000]={0};c.x[0]=(uintptr_t)&result;c.x[1]=(uintptr_t)arg;vr=true;web_show_offline(&c);assert(result==1&&c.x[0]==0&&notices==0);result=0;c.x[0]=(uintptr_t)&result;vr=false;web_show_offline(&c);assert(result==1&&c.x[0]==0&&notices==1);puts("VR manual closes without blocking; desktop notice and applet return preserved");}
