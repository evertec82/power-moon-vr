// OpenXR stereo replay with native head camera, isolated eye history and headset-rate interpolation.
// Game simulation stays at sixty steps per second.
#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <SDL3/SDL.h>
#include "vr.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "vr_pacing.h"
#include "vr_prediction.h"
#include "vr_input.h"
#include "vr_simulation.h"
#include "vr_frame_metrics.h"
#include "vr_math.h"
#include "vr_camera_math.h"
#include "vr_camera_state.h"
#include "vr_menu.h"
#include "vr_hud.h"
#include "vr_reconstruction.h"
#include "vr_first_person.h"
#include "../../cpu.h"
#include "../renderer.h"
extern uint64_t renderer_fence_seq(void);
extern void renderer_submit(void);
extern void renderer_wait_seq(uint64_t seq);
extern void renderer_interp_replay(void);

#define XR_FUNCS(X) \
 X(CreateInstance) X(DestroyInstance) X(GetSystem) X(GetInstanceProperties) \
 X(GetVulkanGraphicsRequirementsKHR) X(GetVulkanInstanceExtensionsKHR) X(GetVulkanDeviceExtensionsKHR) \
 X(GetVulkanGraphicsDeviceKHR) X(CreateSession) X(DestroySession) X(CreateReferenceSpace) X(DestroySpace) \
 X(EnumerateViewConfigurationViews) X(EnumerateSwapchainFormats) X(CreateSwapchain) X(DestroySwapchain) \
 X(EnumerateSwapchainImages) X(PollEvent) X(BeginSession) X(EndSession) X(WaitFrame) X(BeginFrame) X(EndFrame) \
 X(LocateViews) X(AcquireSwapchainImage) X(WaitSwapchainImage) X(ReleaseSwapchainImage)
#define DECL(n) static PFN_xr##n p##n;
XR_FUNCS(DECL)
#undef DECL
static PFN_xrGetInstanceProcAddr getproc;
static XrInstance instance;
static XrSystemId system_id;
static XrSession session;
static XrSpace space;
static bool enabled, booted, running, failed,focused;
static bool offscreen_check;
static int eye = -1;
static XrView views[2];
static XrPosef center;
static bool centered, recenter_down;
static XrViewConfigurationView config[2];
static struct {
    XrSwapchain swapchain;
    uint32_t count, width, height;
    XrSwapchainImageVulkanKHR *images;
} targets[2];
static uint64_t frame_count;
static VrRateClock refresh_clock;
static _Atomic int64_t display_period_ns;
static _Atomic float measured_vr_fps,target_vr_hz;
static bool multiple_samples,previous_camera_valid;
static _Atomic bool pose_queries_active;
static _Atomic int64_t camera_lead_ns;
static _Atomic uint64_t native_query_ns,native_query_n;
static _Atomic uint64_t native_time_clamps,native_query_failures;
static int64_t last_native_query_time;
static int64_t pose_prediction_time,applied_pose_time,frame_pose_time;
static uint64_t pose_prediction_host;
static _Atomic bool title_menu_active;
static bool frame_title_menu;
void vr_set_title_menu(bool active) {
    if(active)vr_first_person_frame_begin();
    bool was=atomic_exchange_explicit(&title_menu_active,active,memory_order_relaxed);
    if(was!=active && vr_enabled()) LOG("[vr-menu] %s\n",active?"stationary title panel":"gameplay projection");
}
static VrFrameMetrics frame_metrics;
static _Atomic bool first_person_requested,first_person_applied;
bool vr_first_person_requested(void) {return atomic_load(&first_person_requested);}
bool vr_first_person_applied(void) {return atomic_load(&first_person_applied);}
void vr_first_person_frame_begin(void) {atomic_store(&first_person_applied,false);}
void vr_toggle_first_person(void) {
    if(!vr_enabled() || atomic_load(&title_menu_active))return;
    bool old=atomic_load(&first_person_requested);
    while(!atomic_compare_exchange_weak(&first_person_requested,&old,!old)){}
    LOG("[vr-view] %s requested\n",old?"third-person":"first-person");
}
static uint64_t vr_view_button_mask(void) {
    static _Atomic uint64_t cached=UINT64_MAX;
    uint64_t mask=atomic_load(&cached);
    if(mask==UINT64_MAX) {
        const char *e=getenv("ODYSSEY_VR_VIEW_BUTTON");
        mask=e && !strcmp(e,"r3")?UINT64_C(1)<<5:e && !strcmp(e,"off")?0:((UINT64_C(1)<<4)|(UINT64_C(1)<<5));
        atomic_store(&cached,mask);
    }
    return mask;
}
void vr_poll_view_buttons(uint64_t buttons) {
    static bool down;
    uint64_t mask=vr_view_button_mask();bool held=mask && (buttons&mask)==mask;
    if(held&&!down)vr_toggle_first_person();
    down=held;
}
uint64_t vr_filter_view_buttons(uint64_t buttons) {
    return vr_enabled()?buttons&~vr_view_button_mask():buttons;
}
static XrPosef previous_head;
static float previous_projection[16];
static bool rate_requested(void) {
    static _Atomic int cached=-1;
    int requested=atomic_load_explicit(&cached,memory_order_relaxed);
    if(requested<0) {
        const char *e=getenv("ODYSSEY_VR_RATE");
        requested=e && (!strcmp(e,"display")||!strcmp(e,"auto")||!strcmp(e,"on"));
        atomic_store_explicit(&cached,requested,memory_order_relaxed);
    }
    return requested;
}
static bool serial_high_rate(void) {
    static int on=-1;
    if(on<0){const char *e=getenv("ODYSSEY_VR_SERIAL_RATE");on=e&&atoi(e)!=0;}
    return on;
}
// Geometry-only late rotation leaves screen-space reconstruction in the old
// camera. Default to the recorded image pose; the compositor warps all layers
// of that completed image together. Retain the old variant only for diagnostics.
static bool native_pose_query_enabled(void) {
    static int on=-1;
    if(on<0){const char *e=getenv("ODYSSEY_VR_NATIVE_POSE_QUERY");on=e&&atoi(e)!=0;}
    return on;
}
static bool late_geometry_test(void) {
    static int on=-1;
    if(on<0){const char *e=getenv("ODYSSEY_VR_LATE_GEOMETRY_TEST");on=e&&atoi(e)!=0;}
    return serial_high_rate()&&on;
}
bool vr_simulation_enabled(void) {
    static int on=-1;
    if(on<0) { const char *e=getenv("ODYSSEY_VR_SIMULATION"); on=e && atoi(e)!=0; }
    return vr_enabled() && on;
}
uint64_t vr_simulation_deadline(uint64_t now) {
    static VrSimulationClock clock;
    return vr_simulation_step(&clock,now);
}
bool vr_multiple_samples(void) { return multiple_samples; }
bool vr_shadow_share_allowed(void) { return serial_high_rate() || (!multiple_samples && !rate_requested()); }
bool vr_high_rate_pacing(void) {
    int64_t period=atomic_load(&display_period_ns);
    return vr_enabled() && !vr_simulation_enabled() && rate_requested() && running && period>0;
}
bool vr_fps(float *submitted,float *target) {
    if(!vr_enabled() || !session) return false;
    *submitted=atomic_load(&measured_vr_fps); *target=atomic_load(&target_vr_hz);
    return true;
}
enum {C_MATCH,C_PREP,C_XRWAIT,C_REPLAY,C_GPUWAIT,C_END,C_ACQUIRE,C_COPY,C_COUNT};
static uint64_t cost_ns[C_COUNT],cost_n[C_COUNT];
static void cost_add(int kind,uint64_t start) { cost_ns[kind]+=SDL_GetTicksNS()-start; cost_n[kind]++; }
static double cost_ms(int kind) { return cost_n[kind]?(double)cost_ns[kind]/cost_n[kind]/1e6:0; }
static bool previous_head_motion;
static uint64_t motion_frames;
static int64_t last_predicted;
static uint64_t prediction_sum,prediction_n,extra_images;
static void rate_stats(bool game,bool rendered,bool blended) {
    static uint64_t since,games,images,interpolated;
    uint64_t now=SDL_GetTicksNS();
    if(!since) since=now;
    games+=game; images+=rendered; interpolated+=blended&&rendered;
    if(now-since>=2000000000ull) {
        double seconds=(now-since)/1e9;
        atomic_store(&measured_vr_fps,(float)(images/seconds));
        LOG("[vr-rate] VR %.1f fps / %.1f Hz target; game %.1f fps; %llu interpolated images; %llu head-motion frames\n",
            images/seconds,(double)atomic_load(&target_vr_hz),games/seconds,(unsigned long long)interpolated,(unsigned long long)motion_frames);
        LOG("[vr-cost] match %.2f ms/game, prepare %.2f ms/image, xrWait %.2f ms/image, replay %.2f ms/eye, gpuWait %.2f ms/image, xrEnd %.2f ms/image\n",cost_ms(C_MATCH),cost_ms(C_PREP),cost_ms(C_XRWAIT),cost_ms(C_REPLAY),cost_ms(C_GPUWAIT),cost_ms(C_END));
        if(serial_high_rate()&&prediction_n) {
            LOG("[vr-rate-test] predicted cadence %.3f ms; %llu additional images from existing simulation frames\n",prediction_sum/(double)prediction_n/1e6,(unsigned long long)extra_images);
            prediction_sum=prediction_n=extra_images=0;
        }
        uint64_t query_n=atomic_exchange(&native_query_n,0),query_ns=atomic_exchange(&native_query_ns,0);
        if(query_n)LOG("[vr-pose] native camera query %.3f ms/call; adaptive lead %.2f ms; %llu queries\n",query_ns/(double)query_n/1e6,atomic_load(&camera_lead_ns)/1e6,(unsigned long long)query_n);
        uint64_t clamps=atomic_exchange(&native_time_clamps,0),fallbacks=atomic_exchange(&native_query_failures,0);
        if(clamps||fallbacks)LOG("[vr-pose] held monotonic query time %llu; retained recorded pose after query failure %llu\n",(unsigned long long)clamps,(unsigned long long)fallbacks);
        if(cost_n[C_ACQUIRE]) LOG("[vr-transfer] acquire %.2f ms/eye, copy submit %.2f ms/eye\n",cost_ms(C_ACQUIRE),cost_ms(C_COPY));
        if(frame_metrics.count) {
            LOG("[vr-pacing] delivery p50 %.2f p95 %.2f p99 %.2f ms; longest %.2f ms; %u intervals\n",
                vr_metrics_percentile(&frame_metrics,50),vr_metrics_percentile(&frame_metrics,95),
                vr_metrics_percentile(&frame_metrics,99),frame_metrics.longest/1e6,frame_metrics.count);
            vr_metrics_clear(&frame_metrics);
        }
        memset(cost_ns,0,sizeof cost_ns); memset(cost_n,0,sizeof cost_n);
        since=now; games=images=interpolated=motion_frames=0;
    }
}
// The default keeps the confirmed wait-before-release order. The optional
// OpenXR rendering.adoc: release once commands referencing the image have
// been submitted; Vulkan OpenXR calls share the externally synchronized queue.
// Early handoff still retires GPU work before vr_frame returns: no worker and
// no additional game frame in flight, only xrEndFrame overlaps the GPU tail.
static bool vr_early_submit(void) {
    static int enabled=-1;
    if(enabled<0) { const char *e=getenv("ODYSSEY_VR_EARLY_SUBMIT"); enabled=e && atoi(e)!=0;
        LOG("[vr] gameplay compositor handoff %s; menu panels wait for GPU completion; serial GPU retirement\n",enabled?"before GPU wait":"after GPU wait");
        if(serial_high_rate())LOG("[vr-rate-test] serial display cadence, recorded image pose, compositor head correction; no animation interpolation\n"); }
    return enabled;
}
static void vr_wait_pair(uint64_t seq) {
    if(!seq)return;
    uint64_t start=SDL_GetTicksNS();
    renderer_wait_seq(seq);
    cost_add(C_GPUWAIT,start);
}
static bool capture_requested,eye_history_active;
void vr_request_capture(void) { capture_requested=true; }
static float scale = 100;
static pthread_mutex_t camera_lock=PTHREAD_MUTEX_INITIALIZER;
static struct { float fovy,aspect,nearz,farz; bool valid; } game_camera;
static float frame_projection[16];
static XrFovf frame_fov;
static float wanted_fovy=1.8f,wanted_aspect=1.0f;
static XrPosef native_head={.orientation={0,0,0,1}};
static XrPosef applied_head={.orientation={0,0,0,1}};
static XrPosef frame_head={.orientation={0,0,0,1}};
static XrPosef tracking_head={.orientation={0,0,0,1}};
typedef struct { float *key,base[21],last[21]; bool used; } CameraBase;
static bool query_native_head(XrPosef *head,int64_t *pose_time) {
    if(!native_pose_query_enabled()||offscreen_check||!atomic_load(&pose_queries_active))return false;
    uint64_t now=SDL_GetTicksNS();int64_t target;XrPosef anchor;
    pthread_mutex_lock(&camera_lock);
    bool valid=centered&&vr_prediction_target(pose_prediction_time,pose_prediction_host,now,atomic_load(&camera_lead_ns),&target);
    if(valid) {
        int64_t forward=vr_prediction_forward(target,last_native_query_time);
        if(forward!=target)atomic_fetch_add(&native_time_clamps,1);
        target=forward;
    }
    anchor=center;
    pthread_mutex_unlock(&camera_lock);
    if(!valid)return false;
    XrView v[2]={{.type=XR_TYPE_VIEW},{.type=XR_TYPE_VIEW}};uint32_t n=0;
    XrViewState state={.type=XR_TYPE_VIEW_STATE};
    XrViewLocateInfo li={.type=XR_TYPE_VIEW_LOCATE_INFO,.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,.displayTime=target,.space=space};
    XrResult r=pLocateViews(session,&li,&state,2,&n,v);
    atomic_fetch_add(&native_query_ns,SDL_GetTicksNS()-now);atomic_fetch_add(&native_query_n,1);
    if(XR_FAILED(r)||n!=2||!(state.viewStateFlags&XR_VIEW_STATE_ORIENTATION_VALID_BIT)||!(state.viewStateFlags&XR_VIEW_STATE_POSITION_VALID_BIT)) {
        atomic_fetch_add(&native_query_failures,1);return false;
    }
    *head=pose_relative(anchor,vr_head_from_views(v));
    pthread_mutex_lock(&camera_lock);
    last_native_query_time=vr_prediction_forward(target,last_native_query_time);
    pthread_mutex_unlock(&camera_lock);
    *pose_time=target;return true;
}
static CameraBase camera_base[16];
static unsigned camera_next;
void vr_apply_game_camera(float *fields) {
    if(!vr_enabled() || atomic_load_explicit(&title_menu_active,memory_order_relaxed)) return;
    // LookAtCamera contains a row-major view matrix and position/target/up vectors.
    float base[21],out[21]; memcpy(base,fields,sizeof base);
    pthread_mutex_lock(&camera_lock);
    CameraBase *entry=NULL;
    for(int i=0;i<16;i++) if(camera_base[i].key==fields) entry=&camera_base[i];
    if(!entry) for(int i=0;i<16;i++) if(!camera_base[i].key) { entry=&camera_base[i]; entry->key=fields; break; }
    if(!entry) { entry=&camera_base[camera_next++%16]; *entry=(CameraBase){.key=fields}; }
    // Pause/stop cameras can retain last frame's look-at vectors. Avoid accumulating head transforms.
    if(entry && entry->used && !memcmp(base+12,entry->last+12,9*sizeof(float))) memcpy(base,entry->base,sizeof base);
    XrPosef h=native_head;
    pthread_mutex_unlock(&camera_lock);
    int64_t queried_time=0;
    if(!query_native_head(&h,&queried_time) && native_pose_query_enabled() && !offscreen_check) {
        // The image and submitted pose must describe the same camera. A failed
        // fresh query must not switch back to the older render-thread sample.
        pthread_mutex_lock(&camera_lock);
        if(applied_pose_time>0){h=applied_head;queried_time=applied_pose_time;}
        pthread_mutex_unlock(&camera_lock);
    }
    float original_base[21];memcpy(original_base,base,sizeof base);
    bool fp=false;
    if(vr_first_person_requested()) {
        extern Cpu *kernel_current_cpu(void);
        extern bool odyssey_vr_player_eye(Cpu *,float *,float *,float *);
        Cpu *cpu=kernel_current_cpu();float eye_pos[3],up[3],front[3],fp_base[21];
        if(cpu && odyssey_vr_player_eye(cpu,eye_pos,up,front) &&
           vr_first_person_base(fp_base,base,eye_pos,up,front)) {
            memcpy(base,fp_base,sizeof base);fp=true;
        }
    }
    if(!vr_lookat_head(out,base,h,scale)) return;
    extern void odyssey_vr_camera_visibility(Cpu *,bool);
    Cpu *view_cpu=kernel_current_cpu();
    if(view_cpu)odyssey_vr_camera_visibility(view_cpu,fp);
    bool was_fp=atomic_exchange(&first_person_applied,fp);
    if(was_fp!=fp)LOG("[vr-view] %s camera applied\n",fp?"first-person":"third-person");
    memcpy(fields,out,sizeof out);
    pthread_mutex_lock(&camera_lock);
    applied_head=h;applied_pose_time=queried_time;
    if(entry) { memcpy(entry->base,original_base,sizeof original_base); memcpy(entry->last,fields,sizeof base); entry->used=true; }
    pthread_mutex_unlock(&camera_lock);
    static bool logged;
    if(!logged) { logged=true; LOG("[vr] native LookAtCamera head tracking active\n"); }
}
bool vr_camera_target(float *fovy,float *aspect) {
    if(!vr_enabled() || atomic_load_explicit(&title_menu_active,memory_order_relaxed)) return false;
    pthread_mutex_lock(&camera_lock);
    *fovy=wanted_fovy; *aspect=wanted_aspect;
    pthread_mutex_unlock(&camera_lock);
    return true;
}
static void update_camera_target(void) {
    XrFovf f;
    if(!vr_enclosing_fov(&f,views)) return;
    pthread_mutex_lock(&camera_lock);
    wanted_fovy=2*f.angleUp;
    wanted_aspect=tanf(f.angleRight)/tanf(f.angleUp);
    pthread_mutex_unlock(&camera_lock);
}
void vr_game_projection(float fovy,float aspect,float nearz,float farz) {
    if(!isfinite(fovy)||!isfinite(aspect)||!isfinite(nearz)||!isfinite(farz) ||
       fovy<.05f||fovy>3.0f||aspect<.1f||aspect>16||nearz<=0||farz<=nearz) return;
    pthread_mutex_lock(&camera_lock);
    game_camera.fovy=fovy; game_camera.aspect=aspect;
    game_camera.nearz=nearz; game_camera.farz=farz; game_camera.valid=true;
    pthread_mutex_unlock(&camera_lock);
}
static VrCameraState queued_camera;
static bool have_queued_camera;
void vr_camera_snapshot(VrCameraState *out) {
    memset(out,0,sizeof *out);
    pthread_mutex_lock(&camera_lock);
    out->valid=game_camera.valid;
    out->title_menu=atomic_load_explicit(&title_menu_active,memory_order_relaxed);
    out->fovy=game_camera.fovy; out->aspect=game_camera.aspect;
    out->nearz=game_camera.nearz; out->farz=game_camera.farz;
    out->pose_time=applied_pose_time;
    memcpy(out->orientation,&applied_head.orientation,sizeof out->orientation);
    memcpy(out->position,&applied_head.position,sizeof out->position);
    pthread_mutex_unlock(&camera_lock);
}
void vr_camera_queued(const VrCameraState *state) {
    queued_camera=*state; have_queued_camera=true;
}
static void freeze_projection(void) {
    extern bool rq_on_thread(void);
    VrCameraState state;
    if(rq_on_thread() && have_queued_camera) state=queued_camera;
    else vr_camera_snapshot(&state);
    frame_title_menu=state.title_menu;
    frame_pose_time=state.pose_time;
    pthread_mutex_lock(&camera_lock);
    bool valid=state.valid;
    float fy=valid?state.fovy:wanted_fovy;
    float aspect=valid?state.aspect:wanted_aspect;
    float nz=valid?state.nearz:25;
    float fz=valid?state.farz:500000;
    memcpy(&frame_head.orientation,state.orientation,sizeof state.orientation);
    memcpy(&frame_head.position,state.position,sizeof state.position);
    pthread_mutex_unlock(&camera_lock);
    float x=atanf(tanf(fy*.5f)*aspect);
    frame_fov=(XrFovf){-x,x,fy*.5f,-fy*.5f};
    vr_projection(frame_projection,-x,x,fy*.5f,-fy*.5f,nz,fz);
    static bool logged_native;
    if(valid&&!logged_native) { logged_native=true; LOG("[vr] native camera: FOV %.3f deg, aspect %.5f, near %.3f, far %.0f\n",fy*57.2957795f,aspect,nz,fz); }
}

static bool xr_ok(XrResult r, const char *what) {
    if (XR_SUCCEEDED(r)) return true;
    LOG("[vr] %s failed (%d); VR disabled\n", what, r);
    failed = true;
    return false;
}
#define XR_TRY(x) do { if (!xr_ok((x), #x)) return; } while(0)
int vr_eye_index(void) { return eye; }
bool vr_filter_effects(void) {
    if(eye<0) return false;
    // Process options are fixed at launch; avoid CRT environment searches
    // in every draw of both eyes.
    static int compatible=-1,neutral_left;
    if(compatible<0) {
        const char *e=getenv("ODYSSEY_VR_EFFECTS");
        compatible=e && !strcmp(e,"compatible");
        neutral_left=getenv("ODYSSEY_VR_CHECK_NEUTRAL_LEFT")!=NULL;
    }
    if(offscreen_check && eye==0 && neutral_left) return false;
    return compatible;
}
bool vr_enabled(void) { return enabled && !failed; }

void vr_bootstrap(void) {
    if (booted) return;
    booted = true;
    const char *e = getenv("ODYSSEY_VR");
    if (!e || !atoi(e) || g_vk.headless) return;
    const char *check = getenv("ODYSSEY_VR_OFFSCREEN_CHECK");
    if (check && strtoul(check,NULL,10)>0) {
        offscreen_check=true; enabled=true;
        const char *turn=getenv("ODYSSEY_VR_CHECK_TURN");
        if(turn && *turn) {
            float yaw=strtof(turn,NULL)*.5f,pitch=yaw*.5f;
            native_head.orientation=(XrQuaternionf){sinf(pitch)*cosf(yaw),cosf(pitch)*sinf(yaw),-sinf(pitch)*sinf(yaw),cosf(pitch)*cosf(yaw)};
        }
        tracking_head=native_head;
        return;
    }
    SDL_SharedObject *loader = SDL_LoadObject("openxr_loader.dll");
    if (!loader) { LOG("[vr] OpenXR loader missing: %s\n", SDL_GetError()); return; }
    getproc = (PFN_xrGetInstanceProcAddr)SDL_LoadFunction(loader,"xrGetInstanceProcAddr");
    if (!getproc) { LOG("[vr] OpenXR loader has no xrGetInstanceProcAddr\n"); return; }
    XR_TRY(getproc(XR_NULL_HANDLE,"xrCreateInstance",(PFN_xrVoidFunction *)&pCreateInstance));
    const char *ext[] = {XR_KHR_VULKAN_ENABLE_EXTENSION_NAME};
    XrInstanceCreateInfo ci = {.type=XR_TYPE_INSTANCE_CREATE_INFO, .enabledExtensionCount=1, .enabledExtensionNames=ext};
    strcpy(ci.applicationInfo.applicationName,"Power Moon VR experimental");
    strcpy(ci.applicationInfo.engineName,"PowerMoonRecomp");
    ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
    XR_TRY(pCreateInstance(&ci,&instance));
#define LOAD(n) XR_TRY(getproc(instance,"xr" #n,(PFN_xrVoidFunction *)&p##n));
    XR_FUNCS(LOAD)
#undef LOAD
    XrSystemGetInfo si={.type=XR_TYPE_SYSTEM_GET_INFO,.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
    XR_TRY(pGetSystem(instance,&si,&system_id));
    XrGraphicsRequirementsVulkanKHR req={.type=XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    XR_TRY(pGetVulkanGraphicsRequirementsKHR(instance,system_id,&req));
    if (req.minApiVersionSupported > XR_MAKE_VERSION(1,3,0)) { LOG("[vr] runtime requires Vulkan newer than 1.3\n"); failed=true; return; }
    XrInstanceProperties props={.type=XR_TYPE_INSTANCE_PROPERTIES};
    XR_TRY(pGetInstanceProperties(instance,&props));
    if ((e=getenv("ODYSSEY_VR_SCALE"))) { float v=strtof(e,NULL); if(isfinite(v)&&v>=1&&v<=1000) scale=v; }
    LOG("[vr] runtime %s, world scale %.2f units/m; F10 recenters\n",props.runtimeName,scale);
    LOG("[vr] separate eye history enabled; fog/glow compatibility filter available\n");
    enabled=true;
    vr_input_init(instance,getproc);
}

static void append_extensions(bool device,const char **ext,uint32_t *n,uint32_t capacity) {
    if(!vr_enabled() || offscreen_check) return;
    uint32_t size=0;
    XrResult r=device?pGetVulkanDeviceExtensionsKHR(instance,system_id,0,&size,NULL):pGetVulkanInstanceExtensionsKHR(instance,system_id,0,&size,NULL);
    if(!xr_ok(r,"query Vulkan extensions")) return;
    char *text=calloc(size+1,1);
    r=device?pGetVulkanDeviceExtensionsKHR(instance,system_id,size,&size,text):pGetVulkanInstanceExtensionsKHR(instance,system_id,size,&size,text);
    if(!xr_ok(r,"get Vulkan extensions")) { free(text); return; }
    char *save=NULL;
    for(char *s=strtok_r(text," ",&save);s;s=strtok_r(NULL," ",&save)) {
        bool have=false; for(uint32_t i=0;i<*n;i++) if(!strcmp(s,ext[i])) have=true;
        if(have) continue;
        if(*n>=capacity) { LOG("[vr] too many Vulkan extensions\n"); failed=true; break; }
        ext[(*n)++]=strdup(s); // retained for instance/device construction
    }
    free(text);
}
void vr_instance_extensions(const char **e,uint32_t *n,uint32_t c) { append_extensions(false,e,n,c); }
void vr_device_extensions(const char **e,uint32_t *n,uint32_t c) { append_extensions(true,e,n,c); }
VkPhysicalDevice vr_physical_device(VkInstance vk) {
    VkPhysicalDevice device=VK_NULL_HANDLE;
    if(vr_enabled() && !offscreen_check) xr_ok(pGetVulkanGraphicsDeviceKHR(instance,system_id,vk,&device),"get runtime GPU");
    return device;
}

void vr_session_init(void) {
    if(offscreen_check) return;
    if(!vr_enabled()) return;
    XrGraphicsBindingVulkanKHR binding={.type=XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,.instance=g_vk.inst,
        .physicalDevice=g_vk.phys,.device=g_vk.dev,.queueFamilyIndex=g_vk.qfam,.queueIndex=0};
    XrSessionCreateInfo ci={.type=XR_TYPE_SESSION_CREATE_INFO,.next=&binding,.systemId=system_id};
    XR_TRY(pCreateSession(instance,&ci,&session));
    vr_input_attach(session);
    XrReferenceSpaceCreateInfo ri={.type=XR_TYPE_REFERENCE_SPACE_CREATE_INFO,.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL,
        .poseInReferenceSpace.orientation.w=1};
    XR_TRY(pCreateReferenceSpace(session,&ri,&space));
    uint32_t n=0;
    for(int i=0;i<2;i++) config[i].type=XR_TYPE_VIEW_CONFIGURATION_VIEW;
    XR_TRY(pEnumerateViewConfigurationViews(instance,system_id,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&n,config));
    if(n!=2) { LOG("[vr] exactly two stereo views required\n"); failed=true; return; }
    uint32_t nf=0;
    XR_TRY(pEnumerateSwapchainFormats(session,0,&nf,NULL));
    int64_t *formats=calloc(nf,sizeof *formats);
    XrResult result=pEnumerateSwapchainFormats(session,nf,&nf,formats);
    if(!xr_ok(result,"enumerate swapchain formats")) { free(formats); return; }
    VkFormat format=VK_FORMAT_UNDEFINED;
    const VkFormat candidates[]={VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM};
    for(unsigned i=0;i<sizeof candidates/sizeof *candidates && !format;i++)
        for(uint32_t j=0;j<nf;j++) if(formats[j]==candidates[i]) { format=candidates[i]; break; }
    free(formats);
    if(!format) { LOG("[vr] no compatible OpenXR color format\n"); failed=true; return; }
    for(int i=0;i<2;i++) {
        targets[i].width=config[i].recommendedImageRectWidth;
        targets[i].height=config[i].recommendedImageRectHeight;
        XrSwapchainCreateInfo sc={.type=XR_TYPE_SWAPCHAIN_CREATE_INFO,.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
            .format=format,.sampleCount=1,.width=targets[i].width,.height=targets[i].height,.faceCount=1,.arraySize=1,.mipCount=1};
        XR_TRY(pCreateSwapchain(session,&sc,&targets[i].swapchain));
        XR_TRY(pEnumerateSwapchainImages(targets[i].swapchain,0,&targets[i].count,NULL));
        targets[i].images=calloc(targets[i].count,sizeof *targets[i].images);
        for(uint32_t j=0;j<targets[i].count;j++) targets[i].images[j].type=XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
        XR_TRY(pEnumerateSwapchainImages(targets[i].swapchain,targets[i].count,&targets[i].count,(XrSwapchainImageBaseHeader *)targets[i].images));
    }
    LOG("[vr] session created, eyes %ux%u / %ux%u\n",targets[0].width,targets[0].height,targets[1].width,targets[1].height);
}

static void poll_events(void) {
    XrEventDataBuffer event={.type=XR_TYPE_EVENT_DATA_BUFFER};
    XrResult r;
    while((r=pPollEvent(instance,&event))==XR_SUCCESS) {
        if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrSessionState s=((XrEventDataSessionStateChanged *)&event)->state;
            LOG("[vr] session state %d\n",s);
            focused=s==XR_SESSION_STATE_FOCUSED;
            if(!focused)vr_input_clear();else vr_input_profiles(session);
            if(s==XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi={.type=XR_TYPE_SESSION_BEGIN_INFO,.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
                if(xr_ok(pBeginSession(session,&bi),"begin session")){running=true;atomic_store(&pose_queries_active,true);}
            } else if(s==XR_SESSION_STATE_STOPPING) { atomic_store(&pose_queries_active,false); running=false; xr_ok(pEndSession(session),"end session"); }
            else if(s==XR_SESSION_STATE_EXITING||s==XR_SESSION_STATE_LOSS_PENDING) { atomic_store(&pose_queries_active,false); running=false; failed=true; }
        } else if(event.type==XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {vr_input_profiles(session);
        } else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) { running=false; failed=true; }
        else if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {pthread_mutex_lock(&camera_lock);centered=false;last_native_query_time=0;applied_pose_time=0;pthread_mutex_unlock(&camera_lock);}
        event=(XrEventDataBuffer){.type=XR_TYPE_EVENT_DATA_BUFFER};
    }
    if(r!=XR_EVENT_UNAVAILABLE) xr_ok(r,"poll events");
}

static XrPosef relative_pose(XrPosef p) { return pose_relative(center,p); }
static XrPosef rendered_eye_pose(int i) {
    return vr_replay_eye_pose(center,frame_head,tracking_head,relative_pose(views[i].pose),late_geometry_test());
}

// Keep the projection shared by the depth and color passes. Per-draw uniform scans are unsafe.
static void vr_clip_matrix_uncached(float out[16],bool world,bool flip_y,bool depth_minus_one,const void *uniforms,uint64_t bytes) {
    vr_identity(out);
    if(eye<0 || (offscreen_check && (!getenv("ODYSSEY_VR_CHECK_POSE") || (eye==0 && getenv("ODYSSEY_VR_CHECK_NEUTRAL_LEFT"))))) return;
    float p[16],inv[16],v[16],q[16],tmp[16];
    memcpy(p,frame_projection,sizeof p);
    // Use one projection for every pass. Scanning arbitrary bound data per draw picked
    // different matrices for the depth prepass and depth-equal color pass.
    // CameraPoseUpdater supplies the actual projection; freeze it once for both eyes and all passes.
    (void)uniforms; (void)bytes; (void)depth_minus_one;
    if(flip_y) for(int c=0;c<4;c++) p[c*4+1]=-p[c*4+1];
    // Preserve the native camera for geometry and camera-dependent lighting.
    // Submit the recorded pose so compositor head correction moves the whole
    // completed image together. Geometry-only late warp is diagnostic only.
    XrPosef pose=vr_replay_eye_delta(frame_head,tracking_head,relative_pose(views[eye].pose),world,late_geometry_test());
    XrFovf f=views[eye].fov;
    vr_eye_projection(q,f.angleLeft,f.angleRight,f.angleUp,f.angleDown,1,100000);
    // Match game's depth convention (including reverse Z) so its depth tests remain valid.
    q[10]=p[10]; q[14]=p[14];
    if(!world) {
        XrPosef delta=vr_replay_eye_delta(frame_head,tracking_head,relative_pose(views[eye].pose),true,late_geometry_test());
        vr_hud_eye_matrix(out,pose_compose(frame_head,delta),frame_fov,f,vr_ui_scale());
        return;
    }
    if(!vr_inverse(inv,p)) return;
    vr_pose_inverse(v,pose.orientation.x,pose.orientation.y,pose.orientation.z,pose.orientation.w,
                    pose.position.x*scale,pose.position.y*scale,pose.position.z*scale);
    vr_multiply(tmp,v,inv); vr_multiply(out,q,tmp);

}

static float eye_clip_cache[2][2][16];
static bool eye_clip_valid[2][2];
static bool reconstruction_valid,reconstruction_ready;
static float reconstruction_native[16],reconstruction_inv_native[16],reconstruction_eye[16],reconstruction_inv_eye[16],reconstruction_view[16];
void vr_eye_replay_begin(void) { memset(eye_clip_valid,0,sizeof eye_clip_valid);reconstruction_valid=false; }
void vr_clip_matrix(float out[16],bool world,bool flip_y,bool depth_minus_one,const void *uniforms,uint64_t bytes) {
    if(eye<0 || frame_title_menu) { vr_identity(out); return; }
    if(!eye_clip_valid[world][flip_y]) {
        vr_clip_matrix_uncached(eye_clip_cache[world][flip_y],world,flip_y,depth_minus_one,uniforms,bytes);
        eye_clip_valid[world][flip_y]=true;
    }
    memcpy(out,eye_clip_cache[world][flip_y],16*sizeof(float));
}

unsigned vr_reconstruction_uniforms(float *out,const float *in,uint64_t bytes,bool shadow) {
    if(eye<0||frame_title_menu||bytes<64||bytes>65536)return 0;
    if(!reconstruction_valid){
        float clip[16];vr_clip_matrix(clip,true,false,false,NULL,0);
        reconstruction_ready=vr_reconstruction_matrices(reconstruction_native,reconstruction_inv_native,
            reconstruction_eye,reconstruction_inv_eye,reconstruction_view,frame_projection,clip);
        reconstruction_valid=true;
    }
    if(!reconstruction_ready)return 0;
    unsigned n=vr_reconstruction_patch(out,in,bytes,shadow,reconstruction_native,reconstruction_inv_native,
        reconstruction_eye,reconstruction_inv_eye,reconstruction_view);
    if(n){static bool reported[2][2];if(!reported[eye][shadow]){reported[eye][shadow]=true;LOG("[vr-effects] eye %d %s camera correction active (%u matrices)\n",eye,shadow?"shadow resolve":"depth reconstruction",n);}}
    return n;
}

static bool copy_eye(TexBackend *src,int i,uint32_t idx) {
    if(!src) return false;
    vk_end_pass();
    VkCommandBuffer cb=vk_cmd();
    VkImage image=targets[i].images[idx].image;
    VkImageMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.srcAccessMask=0,.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=image,
        .subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&b);
    vk_full_barrier(cb);
    VkImageBlit blit={.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}};
    blit.srcOffsets[1]=(VkOffset3D){(int32_t)src->width,(int32_t)src->height,1};
    blit.dstOffsets[1]=(VkOffset3D){(int32_t)targets[i].width,(int32_t)targets[i].height,1};
    vkCmdBlitImage(cb,src->image,VK_IMAGE_LAYOUT_GENERAL,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
    b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,NULL,0,NULL,1,&b);
    // Both eye copies complete together before either swapchain image is released.
    return true;
}

bool vr_frame(InterpFrame *frame,NvnTexture *texture,uint32_t epoch,uint64_t deadline) {
    if(!frame) return false;
    if(!offscreen_check&&vr_enabled()&&session){poll_events();vr_input_poll(session,focused);}
    static unsigned consecutive_skips;
    static uint64_t skipped;
    bool simulation=vr_simulation_enabled();
    rate_stats(true,false,false);
    if(simulation) {
        static uint64_t debt_budget;
        if(!debt_budget) {
            const char *e=getenv("ODYSSEY_VR_RENDER_DEBT_MS");
            double ms=e?strtod(e,NULL):2.0;
            if(!isfinite(ms) || ms<2 || ms>33.34)ms=2.0;
            debt_budget=(uint64_t)(ms*1e6);
            LOG("[vr-sim] bounded render debt %.3f ms\n",debt_budget/1e6);
        }
        bool late=vr_simulation_late_budget(plat_now_ns(),deadline,debt_budget);
        // Keep servicing OpenXR during sustained overload. A skipped visual frame
        // stays with the compositor under its original submitted projection pose.
        if(late && consecutive_skips<6 && !capture_requested) {
            consecutive_skips++; skipped++;
            interp_frame_drop(frame);
            previous_camera_valid=false; previous_head_motion=false;
            if(eye_history_active) { extern void vk_vr_history_reset(void); vk_vr_history_reset(); }
            eye_history_active=false;
            if(skipped%60==1) LOG("[vr-sim] skipped %llu stereo frames to catch up simulation\n",(unsigned long long)skipped);
            return true;
        }
        consecutive_skips=0;
    }
    // Offscreen-only deterministic overload for an A/B test with the real game.
    if(offscreen_check) {
        const char *cost=getenv("ODYSSEY_VR_TEST_COST_MS");
        extern uint64_t renderer_present_count(void);
        if(cost && renderer_present_count()>=1800) {
            int ms=atoi(cost); if(ms>0 && ms<=100) SDL_Delay((Uint32)ms);
        }
    }
    bool completed=false;
    freeze_projection();
    static bool previous_title_menu;
    if(previous_title_menu!=frame_title_menu) {
        extern void vk_vr_history_reset(void);
        vk_vr_history_reset();
        eye_history_active=false;previous_camera_valid=false;
        previous_title_menu=frame_title_menu;
    }
    XrPosef simulation_head=frame_head;
    unsigned check_samples=1;
    const char *sample_env=getenv("ODYSSEY_VR_CHECK_SAMPLES");
    if(offscreen_check && sample_env && atoi(sample_env)>1) check_samples=(unsigned)atoi(sample_env);
    if(check_samples>4) check_samples=4;
    bool high_rate=rate_requested() && !offscreen_check && (!simulation || serial_high_rate()) && !frame_title_menu;
    multiple_samples=high_rate || check_samples>1;
    bool blend=false;
    bool head_motion=previous_camera_valid && vr_head_changed(previous_head,simulation_head);
    const char *motion_interp=getenv("ODYSSEY_VR_MOTION_INTERP");
    bool stable_camera=!(motion_interp && atoi(motion_interp)) && (head_motion || previous_head_motion);
    previous_head_motion=head_motion;
    if(stable_camera) motion_frames++;
    if(multiple_samples && !stable_camera && !serial_high_rate()) {
        uint64_t match_at=SDL_GetTicksNS();
        blend=interp_frame_begin(frame);
        cost_add(C_MATCH,match_at);
        if(!interp_replay_ready()) goto done;
    } else if(!interp_frame_current(frame)) goto done;
    if(!previous_camera_valid) blend=false;
    for(int i=0;i<16;i++)
        if(fabsf(frame_projection[i]-previous_projection[i])>.0001f) blend=false;
    if(offscreen_check) {
        extern uint64_t renderer_present_count(void);
        unsigned long at=strtoul(getenv("ODYSSEY_VR_OFFSCREEN_CHECK"),NULL,10);
        unsigned long count=1;
        const char *count_env=getenv("ODYSSEY_VR_CHECK_FRAMES");
        if(count_env && strtoul(count_env,NULL,10)>0) count=strtoul(count_env,NULL,10);
        bool capture = renderer_present_count()>=at;
        bool save_capture=capture && !getenv("ODYSSEY_VR_BENCH_NO_CAPTURE");
        const char *bench_env=getenv("ODYSSEY_VR_BENCH_START");
        uint64_t bench_start=bench_env?strtoull(bench_env,NULL,10):UINT64_MAX;
        if(renderer_present_count()+30>=at || renderer_present_count()>=bench_start) {
            static int clear_ab=-1;
            if(clear_ab<0)clear_ab=getenv("ODYSSEY_VR_BENCH_CLEAR_AB")!=NULL;
            int bench_variant=-1;
            if(clear_ab && renderer_present_count()>=bench_start) {
                uint64_t f=renderer_present_count();
                // ABBA balances both even/odd game passes in a single run.
                bench_variant=(int)((f^(f>>1))&1);
                vk_vr_load_clear_override(bench_variant);
            }
            uint64_t bench_at=SDL_GetTicksNS(),bench_cpu=0;
            extern void renderer_save_texture_tex(TexBackend *,const char *);
            extern TexBackend *vk_tex_replayed(NvnTexture *);
            extern uint32_t g_interp_replay_epoch;
            if(save_capture) renderer_save_texture_tex(vk_tex(texture,true),"build/vr_reference.png");
            bool compare_clear=getenv("ODYSSEY_VR_COMPARE_CLEAR")!=NULL;
            if(save_capture && (compare_clear || getenv("ODYSSEY_VR_COMPARE_SHADOW"))) {
                bool saved_multiple=multiple_samples;
                multiple_samples=false; interp_use_current();
                for(int mode=0;mode<2;mode++) {
                    extern void vk_vr_history_reset(void);
                    vk_vr_history_reset();
                    if(compare_clear) vk_vr_load_clear_override(mode);
                    else vk_vr_shadow_override(mode?1:0);
                    for(int i=0;i<2;i++) if(interp_replay_lock()) {
                        eye=i; g_interp_replay_epoch=epoch;
                        renderer_interp_replay(); interp_replay_report();
                        char path[128];snprintf(path,sizeof path,"build/vr_%s_%d_%c.png",compare_clear?"clear":"shadow",mode,i?'R':'L');
                        renderer_save_texture_tex(vk_tex_replayed(texture),path);
                        interp_replay_unlock();
                    }
                }
                vk_vr_shadow_override(-1); vk_vr_load_clear_override(-1); multiple_samples=saved_multiple;
                LOG("[vr-check] same-frame %s A/B capture complete\n",compare_clear?"clear":"shadow");
            }
            for(unsigned sample=0;sample<check_samples;sample++) {
                float alpha=(sample+.5f)/check_samples;
                bool prepared=check_samples>1 && blend && interp_prepare(alpha);
                if(!prepared) interp_use_current();
                frame_head=prepared?vr_pose_blend(previous_head,simulation_head,alpha):simulation_head;
                if(capture && check_samples>1) LOG("[vr-check] sample %u/%u alpha %.3f blended %d\n",sample+1,check_samples,alpha,prepared);
            int check_eyes=frame_title_menu && getenv("ODYSSEY_VR_CHECK_TITLE_LIVE")?1:2;
            for(int order=0;order<check_eyes;order++) if(interp_replay_lock()) {
                int i=getenv("ODYSSEY_VR_CHECK_REVERSE")?1-order:order;
                eye=i; g_interp_replay_epoch=epoch;
                if(getenv("ODYSSEY_VR_CHECK_POSE")) {
                    center=(XrPosef){.orientation={0,0,0,1}};
                    views[i].pose=(XrPosef){.orientation={0,0,0,1},.position={i?.032f:-.032f,0,0}};
                    const char *turn=getenv("ODYSSEY_VR_CHECK_TURN");
                    if(turn && *turn) {
                        float yaw=strtof(turn,NULL)*.5f;
                        float pitch=yaw*.5f;
                        views[i].pose.orientation=(XrQuaternionf){sinf(pitch)*cosf(yaw),cosf(pitch)*sinf(yaw),-sinf(pitch)*sinf(yaw),cosf(pitch)*cosf(yaw)};
                    }
                    XrPosef head={.orientation=views[i].pose.orientation};
                    if(getenv("ODYSSEY_VR_CHECK_MOTION")) {
                        float time=(float)renderer_present_count();
                        const char *motion_end=getenv("ODYSSEY_VR_CHECK_MOTION_END");
                        if(motion_end && strtoul(motion_end,NULL,10)>0) time=fminf(time,(float)strtoul(motion_end,NULL,10));
                        float yaw=.075f+sinf(time*.08f)*.1f,pitch=.0375f+sinf(time*.07f)*.05f;
                        head.orientation=(XrQuaternionf){sinf(pitch)*cosf(yaw),cosf(pitch)*sinf(yaw),-sinf(pitch)*sinf(yaw),cosf(pitch)*cosf(yaw)};
                    }
                    pthread_mutex_lock(&camera_lock);
                    native_head=head; tracking_head=head;
                    pthread_mutex_unlock(&camera_lock);
                    views[i].pose=pose_compose(head,(XrPosef){.orientation={0,0,0,1},.position={i?.032f:-.032f,0,0}});
                    views[i].fov=(XrFovf){-.9f,.9f,.9f,-.9f};
                    const char *cant=getenv("ODYSSEY_VR_CHECK_CANT");
                    if(cant){
                        float half=strtof(cant,NULL)*.00872664626f;
                        views[i].pose=pose_compose(head,(XrPosef){.orientation={0,i?-sinf(half):sinf(half),0,cosf(half)},.position={i?.032f:-.032f,0,0}});
                        views[i].fov=i?(XrFovf){-.67f,1.04f,.82f,-.71f}:(XrFovf){-1.04f,.67f,.79f,-.75f};
                    }
                }
                extern bool g_debug_frame;
                g_debug_frame=capture && i==1 && getenv("ODYSSEY_VR_CHECK_PASSES");
                uint64_t replay_at=SDL_GetTicksNS();
                renderer_interp_replay();
                bench_cpu+=SDL_GetTicksNS()-replay_at;
                g_debug_frame=false;
                if(capture) interp_replay_report();
                if(save_capture) {
                    char path[160];
                    if(check_samples>1) snprintf(path,sizeof path,"build/vr_rate_%llu_s%u_%c.png",(unsigned long long)renderer_present_count(),sample,i?'R':'L');
                    else if(count>1) snprintf(path,sizeof path,"build/vr_sequence_%llu_%c.png",(unsigned long long)renderer_present_count(),i?'R':'L');
                    else snprintf(path,sizeof path,"build/vr_neutral_%s.png",i?"right":"left");
                    renderer_save_texture_tex(vk_tex_replayed(texture),path);
                }
                interp_replay_unlock();
            }
            }
            if(bench_env) {
                vk_wait_seq(vk_pending_seq()-1);
                static uint64_t n,cpu,total;
                if(renderer_present_count()>=bench_start) {
                    uint64_t elapsed=SDL_GetTicksNS()-bench_at;
                    n++;cpu+=bench_cpu;total+=elapsed;
                    if(bench_variant>=0) {
                        static struct { uint64_t n,cpu,total;VrFrameMetrics times; } variants[2];
                        __typeof__(variants[0]) *s=&variants[bench_variant];
                        s->n++;s->cpu+=bench_cpu;s->total+=elapsed;vr_metrics_observe(&s->times,elapsed);
                        if(n%120==0 || capture)for(int mode=0;mode<2;mode++)if(variants[mode].n) {
                            __typeof__(variants[0]) *v=&variants[mode];
                            LOG("[vr-clear-ab] mode %d, %llu pairs: CPU %.3f ms; complete %.3f ms; p95 %.3f ms; max %.3f ms\n",
                                mode,(unsigned long long)v->n,v->cpu/(double)v->n/1e6,v->total/(double)v->n/1e6,
                                vr_metrics_percentile(&v->times,95),v->times.longest/1e6);
                        }
                    }
                    rate_stats(false,true,false);
                    if(n%120==0 || capture) LOG("[vr-bench] %llu pairs: translate %.3f ms/pair; complete %.3f ms/pair\n",(unsigned long long)n,cpu/(double)n/1e6,total/(double)n/1e6);
                }
            }
            if(bench_variant>=0)vk_vr_load_clear_override(-1);
            completed=true;
            if(capture) {
                LOG("[vr-check] captured sequence frame %llu; title panel %d; first person requested %d applied %d\n",(unsigned long long)renderer_present_count(),frame_title_menu,vr_first_person_requested(),vr_first_person_applied());
                extern uint64_t vk_vr_folded_clear_count(void);
                LOG("[vr-clear] folded %llu image initializations\n",(unsigned long long)vk_vr_folded_clear_count());
                if(renderer_present_count()>=at+count-1) {
                    LOG("[vr-check] neutral replay capture complete\n"); fflush(stderr);
#ifdef ODYSSEY_PROFILE_TRAIN
                    // The offscreen harness exits on the render thread: flush
                    // LLVM counters explicitly before process teardown.
                    extern int __llvm_profile_write_file(void);
                    LOG("[vr-bench] profile write status %d\n",__llvm_profile_write_file());
#endif
                    exit(0);
                }
            }
        }
        goto done;
    }
    if(!vr_enabled()||!session) goto done;
    poll_events();
    if(!running||failed) goto done;
    unsigned headset_samples=0;
    bool more_samples;
    do {
    XrFrameWaitInfo wi={.type=XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs={.type=XR_TYPE_FRAME_STATE};
    uint64_t wait_at=SDL_GetTicksNS();
    if(!xr_ok(pWaitFrame(session,&wi,&fs),"wait frame")) goto done;
    cost_add(C_XRWAIT,wait_at);
    pthread_mutex_lock(&camera_lock);
    pose_prediction_time=fs.predictedDisplayTime;pose_prediction_host=SDL_GetTicksNS();
    pthread_mutex_unlock(&camera_lock);
    if(!headset_samples&&!frame_title_menu&&frame_pose_time>0)atomic_store(&camera_lead_ns,vr_prediction_lead(atomic_load(&camera_lead_ns),fs.predictedDisplayTime-frame_pose_time));
    if(last_predicted&&fs.predictedDisplayTime>last_predicted){prediction_sum+=(uint64_t)(fs.predictedDisplayTime-last_predicted);prediction_n++;}
    last_predicted=fs.predictedDisplayTime;
    if(headset_samples)extra_images++;
    int64_t period=fs.predictedDisplayPeriod;
    if(period>0) {
        atomic_store(&display_period_ns,period);
        atomic_store(&target_vr_hz,(float)(1e9/period));
    }
    if(high_rate && !headset_samples) {
        if(serial_high_rate())vr_rate_begin_source(&refresh_clock,fs.predictedDisplayTime,(int64_t)deadline);
        else vr_rate_begin(&refresh_clock,fs.predictedDisplayTime);
        multiple_samples=vr_rate_next_within(&refresh_clock,fs.predictedDisplayTime,period);
    }
    float alpha=high_rate?vr_rate_alpha(&refresh_clock,fs.predictedDisplayTime):1;
    uint64_t prep_at=SDL_GetTicksNS();
    bool prepared=high_rate && blend && interp_prepare(alpha);
    cost_add(C_PREP,prep_at);
    if(!prepared) interp_use_current();
    frame_head=prepared?vr_pose_blend(previous_head,simulation_head,alpha):simulation_head;
    XrFrameBeginInfo bi={.type=XR_TYPE_FRAME_BEGIN_INFO};
    vk_queue_lock();
    XrResult begin_result=pBeginFrame(session,&bi);
    vk_queue_unlock();
    if(!xr_ok(begin_result,"begin frame")) goto done;
    XrCompositionLayerProjectionView projection[2];
    memset(projection,0,sizeof projection);
    bool rendered=fs.shouldRender;
    if(rendered) {
        XrViewLocateInfo li={.type=XR_TYPE_VIEW_LOCATE_INFO,.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            .displayTime=fs.predictedDisplayTime,.space=space};
        XrViewState vs={.type=XR_TYPE_VIEW_STATE}; uint32_t n=0;
        for(int i=0;i<2;i++) views[i]=(XrView){.type=XR_TYPE_VIEW};
        rendered=xr_ok(pLocateViews(session,&li,&vs,2,&n,views),"locate views")&&n==2&&
            (vs.viewStateFlags&XR_VIEW_STATE_ORIENTATION_VALID_BIT)&&(vs.viewStateFlags&XR_VIEW_STATE_POSITION_VALID_BIT);
        bool key=SDL_GetKeyboardState(NULL)[SDL_SCANCODE_F10];
        if(rendered&&(!centered||(key&&!recenter_down))) {
            pthread_mutex_lock(&camera_lock);
            center=vr_head_from_views(views);
            centered=true;
            last_native_query_time=0;applied_pose_time=0;
            pthread_mutex_unlock(&camera_lock);
            LOG("[vr] view recentered\n");
        }
        if(rendered) {
            update_camera_target();
            static bool stereo_logged;
            if(!stereo_logged){
                stereo_logged=true;XrPosef left=pose_relative(vr_head_from_views(views),views[0].pose),right=pose_relative(vr_head_from_views(views),views[1].pose);
                float dx=left.position.x-right.position.x,dy=left.position.y-right.position.y,dz=left.position.z-right.position.z;
                LOG("[vr-stereo] runtime eye poses and asymmetric FOVs preserved; IPD %.2f mm; eye yaw %.2f / %.2f deg; horizontal FOV %.2f..%.2f / %.2f..%.2f deg\n",
                    sqrtf(dx*dx+dy*dy+dz*dz)*1000,2*atan2f(left.orientation.y,left.orientation.w)*57.2957795f,2*atan2f(right.orientation.y,right.orientation.w)*57.2957795f,
                    views[0].fov.angleLeft*57.2957795f,views[0].fov.angleRight*57.2957795f,views[1].fov.angleLeft*57.2957795f,views[1].fov.angleRight*57.2957795f);
            }
            XrPosef head=relative_pose(vr_head_from_views(views));
            pthread_mutex_lock(&camera_lock);
            tracking_head=head;
            native_head=head;
            pthread_mutex_unlock(&camera_lock);
        }
        recenter_down=key;
    }
    bool acquired[2]={false,false};
    uint64_t pair_seq=0;
    if(rendered) for(int i=0;i<(frame_title_menu?1:2);i++) {
        uint64_t acquire_at=SDL_GetTicksNS();
        uint32_t index;
        XrSwapchainImageAcquireInfo ai={.type=XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        vk_queue_lock();
        XrResult acquire_result=pAcquireSwapchainImage(targets[i].swapchain,&ai,&index);
        vk_queue_unlock();
        if(!xr_ok(acquire_result,"acquire eye")) { rendered=false; break; }
        XrSwapchainImageWaitInfo swi={.type=XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,.timeout=XR_INFINITE_DURATION};
        if(!xr_ok(pWaitSwapchainImage(targets[i].swapchain,&swi),"wait eye")) { rendered=false; break; }
        acquired[i]=true;
        cost_add(C_ACQUIRE,acquire_at);
        eye=i;
        extern uint32_t g_interp_replay_epoch;
        g_interp_replay_epoch=epoch;
        bool locked=interp_replay_lock();
        if(locked) {
            uint64_t replay_at=SDL_GetTicksNS();
            renderer_interp_replay();
            cost_add(C_REPLAY,replay_at);
            extern TexBackend *vk_tex_replayed(NvnTexture *t);
            TexBackend *eye_image=vk_tex_replayed(texture);
            extern bool g_debug_frame;
            extern void renderer_save_texture_tex(TexBackend *b,const char *path);
            if(capture_requested) renderer_save_texture_tex(eye_image,i?"build/vr_right.png":"build/vr_left.png");
            static bool reported_size;
            if(!reported_size && eye_image) {
                LOG("[vr] rendered eye source %ux%u; OpenXR swapchain %ux%u; pacing %s\n",eye_image->width,eye_image->height,targets[i].width,targets[i].height,high_rate?"display":"game");
                reported_size=true;
            }
            uint64_t copy_at=SDL_GetTicksNS();
            rendered=copy_eye(eye_image,i,index);
            cost_add(C_COPY,copy_at);
            if(rendered) pair_seq=renderer_fence_seq();
            interp_replay_unlock();
        } else rendered=false;
        eye=-1;

        projection[i]=(XrCompositionLayerProjectionView){.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
            .pose=rendered_eye_pose(i),.fov=views[i].fov,.subImage={.swapchain=targets[i].swapchain,
                .imageRect={{0,0},{(int32_t)targets[i].width,(int32_t)targets[i].height}},.imageArrayIndex=0}};
        if(!rendered) break;
    }
    // Quad panels must be complete before handoff. Keep EarlySubmit for the
    // gameplay projection while font/label updates in the menu retire first.
    bool early_submit=vr_early_submit() && !frame_title_menu;
    if(pair_seq) {
        renderer_submit();
        if(!early_submit) vr_wait_pair(pair_seq);
    }
    for(int i=0;i<2;i++) if(acquired[i]) {
        XrSwapchainImageReleaseInfo ri={.type=XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        vk_queue_lock();
        XrResult release_result=pReleaseSwapchainImage(targets[i].swapchain,&ri);
        vk_queue_unlock();
        if(!xr_ok(release_result,"release eye")) rendered=false;
    }
    XrCompositionLayerProjection layer={.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION,.space=space,.viewCount=2,.views=projection};
    XrCompositionLayerQuad menu_layer={.type=XR_TYPE_COMPOSITION_LAYER_QUAD,.space=space,
        .eyeVisibility=XR_EYE_VISIBILITY_BOTH,
        .subImage={.swapchain=targets[0].swapchain,.imageRect={{0,0},{(int32_t)targets[0].width,(int32_t)targets[0].height}}},
        .pose=vr_menu_panel_pose(center,2.0f),.size={2.8f,2.8f*9.0f/16.0f}};
    const XrCompositionLayerBaseHeader *layers[]={frame_title_menu?
        (XrCompositionLayerBaseHeader *)&menu_layer:(XrCompositionLayerBaseHeader *)&layer};
    XrFrameEndInfo ei={.type=XR_TYPE_FRAME_END_INFO,.displayTime=fs.predictedDisplayTime,
        .environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE,.layerCount=rendered?1:0,.layers=layers};
    uint64_t end_at=SDL_GetTicksNS();
    vk_queue_lock();
    XrResult end_result=pEndFrame(session,&ei);
    vk_queue_unlock();
    if(!xr_ok(end_result,"end frame")) rendered=false;
    cost_add(C_END,end_at);
    if(early_submit) vr_wait_pair(pair_seq);
    completed=rendered;
    if(rendered) vr_metrics_record(&frame_metrics,SDL_GetTicksNS());
    if(rendered && capture_requested) { capture_requested=false; LOG("[vr] paired eye capture complete\n"); }
    rate_stats(false,rendered,prepared);
    if(++frame_count%300==0) LOG("[vr] %llu stereo frames submitted\n",(unsigned long long)frame_count);
    headset_samples++;
    more_samples=high_rate && rendered && !failed &&
                 vr_rate_more(&refresh_clock,fs.predictedDisplayTime,period);
    } while(more_samples);
done:
    if(interp_replay_ready()) {
        previous_head=simulation_head;
        memcpy(previous_projection,frame_projection,sizeof previous_projection);
        previous_camera_valid=true;
    } else previous_camera_valid=false;
    if(!completed && eye_history_active) {
        extern void vk_vr_history_reset(void);
        vk_vr_history_reset();
    }
    eye_history_active=completed;
    eye=-1;
    interp_frame_rotate();
    return false;
}

bool vr_hud_camera_fixed(void) {return eye>=0 && !frame_title_menu;}
// Angular size at the neutral pose; the HUD panel transform applies this once.
float vr_ui_scale(void) {
    static float value=-1;
    if(value<0) {
        const char *e=getenv("ODYSSEY_VR_UI_SCALE");
        value=e?strtof(e,NULL):.65f;
        if(!isfinite(value) || value<.35f || value>1) value=.65f;
        LOG("[vr] UI layout scale %.0f%%\n",value*100);
    }
    return eye>=0 && !frame_title_menu?value:1;
}
