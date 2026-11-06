#include <assert.h>
#include <SDL3/SDL.h>
#include "../runtime/settings.h"
static uint64_t tick=1000000000;
static uint64_t test_ticks(void){return tick;}
#define SDL_GetTicksNS test_ticks
#include "../runtime/nvn/vulkan/vr_input.c"
Settings g_settings={.rumble=true};
static int applies,stops,side_bits;static XrDuration duration;static XrResult result=XR_SUCCESS;
static XrResult XRAPI_CALL apply(XrSession s,const XrHapticActionInfo *a,const XrHapticBaseHeader *p){assert(s&&a->action==actions[HAPTIC]);const XrHapticVibration *v=(const XrHapticVibration *)p;assert(v->type==XR_TYPE_HAPTIC_VIBRATION&&v->amplitude>0&&v->amplitude<=1&&v->duration>0&&v->duration<=250000000);assert(v->frequency==XR_FREQUENCY_UNSPECIFIED);side_bits|=a->subactionPath==hands[0]?1:2;duration=v->duration;applies++;return result;}
static XrResult XRAPI_CALL stop(XrSession s,const XrHapticActionInfo *a){assert(s&&a->action==actions[HAPTIC]);stops++;return XR_SUCCESS;}
static int output_paths,profiles;
static XrResult XRAPI_CALL strpath(XrInstance i,const char *s,XrPath *p){(void)i;assert(!strstr(s,"/input/haptic"));if(strstr(s,"/output/haptic"))output_paths++;static XrPath id=100;*p=++id;return XR_SUCCESS;}
static XrResult XRAPI_CALL bindings(XrInstance i,const XrInteractionProfileSuggestedBinding *b){(void)i;int outputs=0;assert(b->countSuggestedBindings<=32);for(unsigned k=0;k<b->countSuggestedBindings;k++)outputs+=b->suggestedBindings[k].action==actions[HAPTIC];assert(outputs==2);profiles++;return XR_SUCCESS;}
int main(void){
 pApplyHapticFeedback=apply;pStopHapticFeedback=stop;hands[0]=10;hands[1]=20;for(int i=0;i<N_ACTIONS;i++)actions[i]=(XrAction)(uintptr_t)(i+1);XrSession s=(XrSession)(uintptr_t)1;
 vr_input_vibration(0,1,0);haptic_poll(s,true);assert(!applies);
 vr_input_vibration(0,.2f,0);vr_input_vibration(1,0,.4f);assert(!applies);haptic_poll(s,true);assert(applies==2&&side_bits==3);
 tick+=10000000;haptic_poll(s,true);assert(applies==2);
 tick+=100000000;haptic_poll(s,true);assert(applies==4&&duration==140000000);
 tick+=140000000;haptic_poll(s,true);assert(stops==2);
 vr_input_vibration(0,.5f,0);haptic_poll(s,true);assert(applies==5);haptic_poll(s,false);assert(stops==3);
 vr_input_vibration(1,1,1);haptic_poll(s,true);assert(applies==5);
 vr_input_vibration(1,NAN,INFINITY);haptic_poll(s,true);assert(applies==5);
 vr_input_vibration(1,0,.5f);haptic_poll(s,true);assert(applies==6);g_settings.rumble=false;haptic_poll(s,true);assert(stops==4);
 g_settings.rumble=true;haptic_poll(s,true);assert(applies==6);
 vr_input_vibration(0,1,0);haptic_poll(s,true);vr_input_stop_vibration();haptic_poll(s,true);assert(stops==5);
 result=XR_ERROR_RUNTIME_FAILURE;vr_input_vibration(0,1,0);haptic_poll(s,true);int n=applies;tick+=1000000;haptic_poll(s,true);assert(applies==n);tick+=100000000;haptic_poll(s,true);assert(applies==n+1);
 pStringToPath=strpath;pSuggestInteractionProfileBindings=bindings;instance=(XrInstance)(uintptr_t)1;for(int k=0;k<4;k++)suggest("/interaction_profiles/test/controller",k);assert(profiles==4&&output_paths==8);
 puts("OpenXR haptics: queued-only producers, left/right mapping, bounded refresh, stops, focus/settings/stale handling, error throttle and all profile bindings pass");}