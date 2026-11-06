#include <assert.h>
#include <SDL3/SDL.h>
#include "../runtime/settings.h"
static uint64_t tick=1000000000;
static int sends,vr_sends,vr_stops;static uint16_t low,high;static uint32_t ms;static SDL_JoystickID device=1;static int vr_side;
static uint64_t test_ticks(void){return tick;}
static SDL_JoystickID test_id(SDL_Gamepad *p){assert(p);return device;}
static bool test_rumble(SDL_Gamepad *p,Uint16 l,Uint16 h,Uint32 duration){assert(p&&duration<=250);sends++;low=l;high=h;ms=duration;return true;}
static const char *test_error(void){return "test error";}
#define SDL_GetTicksNS test_ticks
#define SDL_GetGamepadID test_id
#define SDL_RumbleGamepad test_rumble
#define SDL_GetError test_error
#include "../runtime/hle.h"
#undef HLE_FUNC
#define HLE_FUNC(sym,fn) static void fn(Cpu *c)
#undef RT_ON_RESET
#define RT_ON_RESET(fn) static void fn(void)
#include "../runtime/hle/input.c"
Settings g_settings={.rumble=true};
void vr_input_vibration(int side,float l,float h){(void)l;(void)h;vr_side=side;vr_sends++;}
void vr_input_stop_vibration(void){vr_stops++;}
int main(void){
 Cpu c={0};uint32_t info[2]={0},handle=0;c.x[0]=(uintptr_t)info;c.x[1]=(uintptr_t)&handle;hid_vibration_info(&c);assert(info[0]==1&&info[1]==1);handle=1;hid_vibration_info(&c);assert(info[0]==1&&info[1]==2);
 pad=(SDL_Gamepad *)(uintptr_t)1;
 input_vibration(0,.2f,0);assert(sends==1&&low>0&&high==0&&ms==250&&vr_sends==1&&vr_side==0);
 input_vibration(1,.2f,0);assert(sends==1&&vr_sends==2&&vr_side==1);
 tick+=260000000;input_vibration(0,0,0);assert(sends==2&&low==0&&high==0&&ms==0);
 input_stop_vibration();int n=sends;
 input_vibration(0,1,0);assert(sends==n+1&&low==65535);
 tick+=200000000;input_vibration(1,0,0);assert(ms==50);
 tick+=60000000;input_vibration(1,0,0);assert(low==0&&high==0);
 input_stop_vibration();input_vibration(0,.2f,0);n=sends;pad=NULL;input_vibration(0,.2f,0);assert(sends==n);pad=(SDL_Gamepad *)(uintptr_t)2;device=2;input_vibration(0,.2f,0);assert(sends==n+1);
 g_settings.rumble=false;input_stop_vibration();assert(!low&&!high);n=sends;int v=vr_sends;input_test_vibration();assert(sends==n&&vr_sends==v&&vr_stops>=3);
 g_settings.rumble=true;input_test_vibration();assert(vr_sends==v+2&&low>0&&high>0);
 puts("Gamepad routing: both VR sides survive aggregate throttle, silence expires, refresh deadline bounded, reconnect forces output, setting/test button work");}// Unused input/UI dependencies deliberately fail if this routing test enters them.
int16_t g_bind_key[BIND_KEY_COUNT],g_bind_pad[BIND_PAD_COUNT];
bool vr_enabled(void){assert(0);return false;}
bool menu_input_captured(void){assert(0);return false;}
void vr_toggle_first_person(void){assert(0);}
void watch_arm(void){assert(0);}
int bind_pad_from_name(const char *s){(void)s;assert(0);return 0;}
int bind_slot_from_name(const char *s){(void)s;assert(0);return 0;}
bool bind_capture_event(const SDL_Event *e){(void)e;assert(0);return false;}
void bind_capture_begin(int dev,int slot){(void)dev;(void)slot;assert(0);}
void bind_reset(int dev){(void)dev;assert(0);}
void vr_poll_view_buttons(uint64_t b){(void)b;assert(0);}
uint64_t vr_filter_view_buttons(uint64_t b){assert(0);return b;}
void vr_input_read(VrControllerState *s){(void)s;assert(0);}
