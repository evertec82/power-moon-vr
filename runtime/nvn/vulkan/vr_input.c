// Optional OpenXR buttons/sticks. All OpenXR action calls run on render owner;
// game input consumes one mutex-protected snapshot, preserving gamepad input.
#include <SDL3/SDL_timer.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../rt.h"
#include "../../settings.h"
#include "../../vibration.h"
#include "vr_input.h"
#define INPUT_FUNCS(X) X(StringToPath) X(PathToString) X(CreateActionSet) X(DestroyActionSet) X(CreateAction) X(SuggestInteractionProfileBindings) X(AttachSessionActionSets) X(SyncActions) X(GetActionStateBoolean) X(GetActionStateFloat) X(GetActionStateVector2f) X(GetCurrentInteractionProfile) X(ApplyHapticFeedback) X(StopHapticFeedback)
#define DECL(n) static PFN_xr##n p##n;
INPUT_FUNCS(DECL)
#undef DECL
static XrInstance instance;static XrActionSet set;static XrPath hands[2];static bool ready,attached;
enum {PRIMARY,SECONDARY,MENU,CLICK,TRIGGER,GRIP,STICK,HAPTIC,N_ACTIONS};
static XrAction actions[N_ACTIONS];static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;static VrControllerState state;
static VibrationMixer vibration;
static bool haptic_focus;
static struct {float amplitude;uint64_t at;bool active,attempted,failed,reported;} output[2];

void vr_input_vibration(int side,float low,float high) {
 pthread_mutex_lock(&lock);
 if(haptic_focus)vibration_publish(&vibration,side,low,high,SDL_GetTicksNS());
 pthread_mutex_unlock(&lock);
}
void vr_input_stop_vibration(void) {
 pthread_mutex_lock(&lock);memset(&vibration,0,sizeof vibration);pthread_mutex_unlock(&lock);
}
// Called only on the render owner, including the focus-loss path.
static void haptic_poll(XrSession session,bool focused) {
 uint64_t now=SDL_GetTicksNS();VibrationOutput next;
 pthread_mutex_lock(&lock);
 haptic_focus=focused;
 if(!focused||!g_settings.rumble)memset(&vibration,0,sizeof vibration);
 next=vibration_output(&vibration,now,focused&&g_settings.rumble);
 pthread_mutex_unlock(&lock);
 for(int h=0;h<2;h++) {
  XrHapticActionInfo info={.type=XR_TYPE_HAPTIC_ACTION_INFO,.action=actions[HAPTIC],.subactionPath=hands[h]};
  float amp=next.hand[h];
  if(!amp) {
   if(output[h].active)pStopHapticFeedback(session,&info);
   output[h].active=false;output[h].attempted=false;output[h].amplitude=0;continue;
  }
  if(output[h].attempted&&amp==output[h].amplitude&&now>=output[h].at&&now-output[h].at<100000000ull)continue;
  XrHapticVibration pulse={.type=XR_TYPE_HAPTIC_VIBRATION,.duration=(XrDuration)next.remaining[h],.frequency=XR_FREQUENCY_UNSPECIFIED,.amplitude=amp};
  XrResult r=pApplyHapticFeedback(session,&info,(const XrHapticBaseHeader *)&pulse);
  output[h].at=now;output[h].amplitude=amp;output[h].active=r==XR_SUCCESS;output[h].attempted=true;
  if(r==XR_SUCCESS&&!output[h].reported){output[h].reported=true;LOG("[vr-haptics] %s controller feedback active\n",h?"right":"left");}
  if(XR_FAILED(r)&&!output[h].failed){output[h].failed=true;LOG("[vr-haptics] %s output unavailable (%d); controls unaffected\n",h?"right":"left",r);}
 }
}
static bool ok(XrResult r,const char *what){if(XR_SUCCEEDED(r))return true;LOG("[vr-input] %s failed (%d); gamepad remains available\n",what,r);return false;}
static XrPath path(const char *s){XrPath p=XR_NULL_PATH;if(!ok(pStringToPath(instance,s,&p),s))return XR_NULL_PATH;return p;}
static void bind(XrActionSuggestedBinding *b,uint32_t *n,int action,int hand,const char *component){char p[192];snprintf(p,sizeof p,"/user/hand/%s/%s/%s",hand?"right":"left",action==HAPTIC?"output":"input",component);XrPath v=path(p);if(v)b[(*n)++]=(XrActionSuggestedBinding){actions[action],v};}
static void suggest(const char *profile,int kind){XrActionSuggestedBinding b[32];uint32_t n=0;
 for(int h=0;h<2;h++){
  if(kind<2){bind(b,&n,PRIMARY,h,h?"a/click":kind==1?"a/click":"x/click");bind(b,&n,SECONDARY,h,h?"b/click":kind==1?"b/click":"y/click");}
  if(kind<3){bind(b,&n,STICK,h,"thumbstick");bind(b,&n,CLICK,h,"thumbstick/click");bind(b,&n,TRIGGER,h,"trigger/value");bind(b,&n,GRIP,h,kind==2?"squeeze/click":"squeeze/value");}
  if(kind==2)bind(b,&n,PRIMARY,h,"trackpad/click");
  if(kind==3)bind(b,&n,PRIMARY,h,"select/click");
  bind(b,&n,HAPTIC,h,"haptic");
 }
 if(kind==2)bind(b,&n,SECONDARY,1,"menu/click");
 if(kind!=1)bind(b,&n,MENU,0,"menu/click");
 XrInteractionProfileSuggestedBinding info={.type=XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,.interactionProfile=path(profile),.countSuggestedBindings=n,.suggestedBindings=b};
 if(info.interactionProfile)ok(pSuggestInteractionProfileBindings(instance,&info),profile);
}
void vr_input_init(XrInstance inst,PFN_xrGetInstanceProcAddr get){
 const char *e=getenv("ODYSSEY_VR_CONTROLLERS");if(e&&!atoi(e))return;instance=inst;
 #define LOAD(n) if(!ok(get(instance,"xr" #n,(PFN_xrVoidFunction *)&p##n),"load " #n))return;
 INPUT_FUNCS(LOAD)
 #undef LOAD
 hands[0]=path("/user/hand/left");hands[1]=path("/user/hand/right");if(!hands[0]||!hands[1])return;
 XrActionSetCreateInfo si={.type=XR_TYPE_ACTION_SET_CREATE_INFO,.priority=0};strcpy(si.actionSetName,"vr_gamepad");strcpy(si.localizedActionSetName,"VR game controls");if(!ok(pCreateActionSet(instance,&si,&set),"create action set"))return;
 const char *names[N_ACTIONS]={"primary","secondary","pause","stick_click","trigger","grip","thumbstick","vibration"};
 for(int i=0;i<N_ACTIONS;i++){XrActionCreateInfo ai={.type=XR_TYPE_ACTION_CREATE_INFO,.actionType=i==HAPTIC?XR_ACTION_TYPE_VIBRATION_OUTPUT:i==STICK?XR_ACTION_TYPE_VECTOR2F_INPUT:i>=TRIGGER?XR_ACTION_TYPE_FLOAT_INPUT:XR_ACTION_TYPE_BOOLEAN_INPUT,.countSubactionPaths=2,.subactionPaths=hands};strcpy(ai.actionName,names[i]);strcpy(ai.localizedActionName,names[i]);if(!ok(pCreateAction(set,&ai,&actions[i]),"create action")){pDestroyActionSet(set);set=XR_NULL_HANDLE;return;}}
 suggest("/interaction_profiles/oculus/touch_controller",0);suggest("/interaction_profiles/valve/index_controller",1);suggest("/interaction_profiles/microsoft/motion_controller",2);suggest("/interaction_profiles/khr/simple_controller",3);ready=true;
}
void vr_input_attach(XrSession session){if(!ready)return;XrSessionActionSetsAttachInfo ai={.type=XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,.countActionSets=1,.actionSets=&set};attached=ok(pAttachSessionActionSets(session,&ai),"attach action set");if(attached)LOG("[vr-input] action set attached; VR buttons/sticks and left/right haptics alongside gamepad\n");}
void vr_input_clear(void){pthread_mutex_lock(&lock);memset(&state,0,sizeof state);memset(&vibration,0,sizeof vibration);haptic_focus=false;pthread_mutex_unlock(&lock);}
void vr_input_read(VrControllerState *out){pthread_mutex_lock(&lock);*out=state;pthread_mutex_unlock(&lock);if(!vr_controller_fresh(out->stamp,SDL_GetTicksNS()))memset(out,0,sizeof *out);}
static bool boolean(XrSession s,int action,int h){XrActionStateGetInfo gi={.type=XR_TYPE_ACTION_STATE_GET_INFO,.action=actions[action],.subactionPath=hands[h]};XrActionStateBoolean v={.type=XR_TYPE_ACTION_STATE_BOOLEAN};return XR_SUCCEEDED(pGetActionStateBoolean(s,&gi,&v))&&v.isActive&&v.currentState;}
static float scalar(XrSession s,int action,int h){XrActionStateGetInfo gi={.type=XR_TYPE_ACTION_STATE_GET_INFO,.action=actions[action],.subactionPath=hands[h]};XrActionStateFloat v={.type=XR_TYPE_ACTION_STATE_FLOAT};return XR_SUCCEEDED(pGetActionStateFloat(s,&gi,&v))&&v.isActive?v.currentState:0;}
void vr_input_poll(XrSession session,bool focused){
 if(!attached||!focused){if(attached)haptic_poll(session,false);vr_input_clear();return;}XrActiveActionSet active={.actionSet=set};XrActionsSyncInfo si={.type=XR_TYPE_ACTIONS_SYNC_INFO,.countActiveActionSets=1,.activeActionSets=&active};XrResult r=pSyncActions(session,&si);if(r!=XR_SUCCESS){haptic_poll(session,false);vr_input_clear();return;}
 haptic_poll(session,true);
 VrControllerRaw raw={0};for(int h=0;h<2;h++){raw.primary[h]=boolean(session,PRIMARY,h);raw.secondary[h]=boolean(session,SECONDARY,h);raw.menu[h]=boolean(session,MENU,h);raw.click[h]=boolean(session,CLICK,h);raw.trigger[h]=scalar(session,TRIGGER,h);raw.grip[h]=scalar(session,GRIP,h);
 XrActionStateGetInfo gi={.type=XR_TYPE_ACTION_STATE_GET_INFO,.action=actions[STICK],.subactionPath=hands[h]};XrActionStateVector2f v={.type=XR_TYPE_ACTION_STATE_VECTOR2F};if(XR_SUCCEEDED(pGetActionStateVector2f(session,&gi,&v))&&v.isActive){raw.stick[h][0]=v.currentState.x;raw.stick[h][1]=v.currentState.y;}}
 VrControllerState next=vr_controller_convert(&raw);next.stamp=SDL_GetTicksNS();
 static uint64_t last_buttons;if(last_buttons!=next.buttons){LOG("[vr-input] buttons %#llx\n",(unsigned long long)next.buttons);last_buttons=next.buttons;}pthread_mutex_lock(&lock);state=next;pthread_mutex_unlock(&lock);
}
void vr_input_profiles(XrSession session){if(!attached)return;for(int h=0;h<2;h++){XrInteractionProfileState p={.type=XR_TYPE_INTERACTION_PROFILE_STATE};if(XR_SUCCEEDED(pGetCurrentInteractionProfile(session,hands[h],&p))&&p.interactionProfile){char name[256];uint32_t n;if(XR_SUCCEEDED(pPathToString(instance,p.interactionProfile,sizeof name,&n,name)))LOG("[vr-input] %s profile %s\n",h?"right":"left",name);}}}
