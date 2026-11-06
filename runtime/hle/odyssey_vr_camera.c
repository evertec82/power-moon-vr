// Runtime-only first-person integration. No game-library ABI or lifted code changes.
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#include "../hle.h"

#ifndef __APPLE__
extern bool vr_enabled(void),vr_first_person_requested(void);
extern void vr_first_person_frame_begin(void);
static _Atomic uint64_t vr_scene;
static uint64_t hidden_actor,hidden_keeper;
static uint64_t get_player,player_vtable,get_model,get_head,get_gravity,calc_front,is_hidden,hide_model,show_model;
static bool bound;
static bool in_heap(uint64_t p,uint64_t size){return p>=HEAP_BASE && p-HEAP_BASE<HEAP_MAX && size<=HEAP_MAX-(p-HEAP_BASE);}
static void bind_queries(void) {
    if(bound)return;bound=true;
    get_player=loader_find_symbol("_ZN2rs14getPlayerActorEPKN2al5SceneE");
    player_vtable=loader_find_symbol("_ZTV19PlayerActorHakoniwa");
    get_model=loader_find_symbol("_ZN19PlayerActorHakoniwa12getDemoActorEv");
    get_head=loader_find_symbol("_ZN2rs16getPlayerHeadPosEPKN2al9LiveActorE");
    get_gravity=loader_find_symbol("_ZN2al10getGravityEPKNS_9LiveActorE");
    calc_front=loader_find_symbol("_ZN2rs18calcPlayerFrontDirEPN4sead7Vector3IfEEPKN2al9LiveActorE");
    is_hidden=loader_find_symbol("_ZN2al11isHideModelEPKNS_9LiveActorE");
    hide_model=loader_find_symbol("_ZN2al15hideModelIfShowEPNS_9LiveActorE");
    show_model=loader_find_symbol("_ZN2al15showModelIfHideEPNS_9LiveActorE");
    LOG("[vr-view] player eye/model interfaces %s\n",get_player&&get_model&&get_head&&is_hidden&&hide_model&&show_model?"ready":"unavailable (third-person fallback)");
}
static uint64_t call(Cpu *q,uint64_t fn,uint64_t a0,uint64_t a1) {
    q->x[0]=a0;q->x[1]=a1;guest_call(q,fn);return q->x[0];
}
static uint64_t player(Cpu *q,uint64_t scene) {
    if(!get_player || !in_heap(scene,0x100))return 0;
    uint64_t objs=LD64(scene+0x90);
    if(!in_heap(objs,0xa0) || !in_heap(LD64(objs+0x98),16))return 0;
    uint64_t p=call(q,get_player,scene,0);
    return player_vtable && in_heap(p,0x218) && LD64(p)==player_vtable+24 && in_heap(LD64(p+0x100),8)?p:0;
}
static uint64_t model(Cpu *q,uint64_t scene) {
    uint64_t p=player(q,scene);
    if(!p||!get_model)return 0;
    uint64_t m=call(q,get_model,p,0);
    return in_heap(m,0x108)&&in_heap(LD64(m+0x100),8)?m:0;
}
static Cpu scratch(Cpu *c) {Cpu q=*c;q.sp=(q.sp-0x800)&~UINT64_C(15);return q;}
void odyssey_vr_scene_reset(void) {atomic_store(&vr_scene,0);hidden_actor=hidden_keeper=0;vr_first_person_frame_begin();}
void odyssey_vr_scene_begin(Cpu *c,uint64_t scene) {
    if(!vr_enabled())return;
    extern void odyssey_vr_disable_camera_blur(Cpu *,uint64_t);
    odyssey_vr_disable_camera_blur(c,scene);
    bind_queries();atomic_store(&vr_scene,scene);
    if(hidden_actor) {
        Cpu q=scratch(c);uint64_t p=model(&q,scene);
        // Restore only the same live model we hid, never an actor from an old scene.
        if(p==hidden_actor && LD64(p+0x100)==hidden_keeper) {
            if(!vr_first_person_requested()) {call(&q,show_model,p,0);hidden_actor=hidden_keeper=0;}
        } else hidden_actor=hidden_keeper=0;
    }
}
void odyssey_vr_camera_visibility(Cpu *c,bool active) {
    if(!vr_enabled() || !is_hidden || !hide_model)return;
    Cpu q=scratch(c);uint64_t p=model(&q,atomic_load(&vr_scene));
    if(!active) {
        if(p && p==hidden_actor && LD64(p+0x100)==hidden_keeper)call(&q,show_model,p,0);
        hidden_actor=hidden_keeper=0;return;
    }
    if(p && !call(&q,is_hidden,p,0)) {
        hidden_actor=p;hidden_keeper=LD64(p+0x100);call(&q,hide_model,p,0);
    }
}
bool odyssey_vr_player_eye(Cpu *c,float eye[3],float up[3],float front[3]) {
    if(!vr_first_person_requested() || !get_head || !is_hidden || !hide_model || !show_model)return false;
    Cpu q=scratch(c);uint64_t p=player(&q,atomic_load(&vr_scene));
    // A natively hidden model keeps its native camera rather than being revealed.
    uint64_t m=model(&q,atomic_load(&vr_scene));
    if(!p || !m || (call(&q,is_hidden,m,0) && !(m==hidden_actor && LD64(m+0x100)==hidden_keeper)))return false;
    uint64_t pos=call(&q,get_head,p,0);
    if(!in_heap(pos,12))return false;
    memcpy(eye,(const void *)(uintptr_t)pos,12);
    for(int i=0;i<3;i++)if(!isfinite(eye[i]) || fabsf(eye[i])>1e7f)return false;
    up[0]=up[2]=0;up[1]=1;front[0]=front[1]=0;front[2]=-1;
    if(get_gravity) {
        uint64_t g=call(&q,get_gravity,p,0);
        if(in_heap(g,12)) {float v[3];memcpy(v,(const void *)(uintptr_t)g,12);
            float n=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
            if(isfinite(n)&&n>.00001f)for(int i=0;i<3;i++)up[i]=-v[i];}
    }
    if(calc_front) {
        uint64_t out=q.sp+0x100;memset((void *)(uintptr_t)out,0,16);
        call(&q,calc_front,out,p);memcpy(front,(const void *)(uintptr_t)out,12);
    }
    return true;
}
RT_ON_RESET(reset_vr_player_queries) {
    bound=false;hidden_actor=hidden_keeper=0;atomic_store(&vr_scene,0);
    vr_first_person_frame_begin();
}
#else
void odyssey_vr_scene_reset(void){}
void odyssey_vr_scene_begin(Cpu *c,uint64_t scene){(void)c;(void)scene;}
void odyssey_vr_camera_visibility(Cpu *c,bool active){(void)c;(void)active;}
#endif
