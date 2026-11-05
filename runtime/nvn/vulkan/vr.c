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
#include "vr_math.h"
#include "vr_camera_math.h"
#include "vr_camera_state.h"
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
static bool enabled, booted, running, failed;
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
static XrPosef previous_head;
static float previous_projection[16];
static bool rate_requested(void) {
    const char *e=getenv("ODYSSEY_VR_RATE");
    return e && (!strcmp(e,"display")||!strcmp(e,"auto")||!strcmp(e,"on"));
}
bool vr_multiple_samples(void) { return multiple_samples; }
bool vr_high_rate_pacing(void) {
    int64_t period=atomic_load(&display_period_ns);
    return vr_enabled() && rate_requested() && running && period>0;
}
bool vr_fps(float *submitted,float *target) {
    if(!vr_enabled() || !session) return false;
    *submitted=atomic_load(&measured_vr_fps); *target=atomic_load(&target_vr_hz);
    return true;
}
enum {C_MATCH,C_PREP,C_XRWAIT,C_REPLAY,C_GPUWAIT,C_END,C_COUNT};
static uint64_t cost_ns[C_COUNT],cost_n[C_COUNT];
static void cost_add(int kind,uint64_t start) { cost_ns[kind]+=SDL_GetTicksNS()-start; cost_n[kind]++; }
static double cost_ms(int kind) { return cost_n[kind]?(double)cost_ns[kind]/cost_n[kind]/1e6:0; }
static bool previous_head_motion;
static uint64_t motion_frames;
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
        memset(cost_ns,0,sizeof cost_ns); memset(cost_n,0,sizeof cost_n);
        since=now; games=images=interpolated=motion_frames=0;
    }
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
static CameraBase camera_base[16];
static unsigned camera_next;
void vr_apply_game_camera(float *fields) {
    if(!vr_enabled()) return;
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
    if(!vr_lookat_head(out,base,h,scale)) return;
    memcpy(fields,out,sizeof out);
    pthread_mutex_lock(&camera_lock);
    applied_head=h;
    if(entry) { memcpy(entry->base,base,sizeof base); memcpy(entry->last,fields,sizeof base); entry->used=true; }
    pthread_mutex_unlock(&camera_lock);
    static bool logged;
    if(!logged) { logged=true; LOG("[vr] native LookAtCamera head tracking active\n"); }
}
bool vr_camera_target(float *fovy,float *aspect) {
    if(!vr_enabled()) return false;
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
    out->fovy=game_camera.fovy; out->aspect=game_camera.aspect;
    out->nearz=game_camera.nearz; out->farz=game_camera.farz;
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
    if(offscreen_check && eye==0 && getenv("ODYSSEY_VR_CHECK_NEUTRAL_LEFT")) return false;
    const char *e=getenv("ODYSSEY_VR_EFFECTS");
    return e && !strcmp(e,"compatible");
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
            if(s==XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi={.type=XR_TYPE_SESSION_BEGIN_INFO,.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
                if(xr_ok(pBeginSession(session,&bi),"begin session")) running=true;
            } else if(s==XR_SESSION_STATE_STOPPING) { running=false; xr_ok(pEndSession(session),"end session"); }
            else if(s==XR_SESSION_STATE_EXITING||s==XR_SESSION_STATE_LOSS_PENDING) { running=false; failed=true; }
        } else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) { running=false; failed=true; }
        else if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) centered=false;
        event=(XrEventDataBuffer){.type=XR_TYPE_EVENT_DATA_BUFFER};
    }
    if(r!=XR_EVENT_UNAVAILABLE) xr_ok(r,"poll events");
}

static XrPosef relative_pose(XrPosef p) { return pose_relative(center,p); }
static XrPosef rendered_eye_pose(int i) {
    XrPosef local=pose_relative(tracking_head,relative_pose(views[i].pose));
    local.orientation=(XrQuaternionf){0,0,0,1};
    return pose_compose(center,pose_compose(frame_head,local));
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
    // Head motion already affected the native camera and every effect. Only apply
    // the eye's offset/cant here; submit the actual rendered pose for compositor timewarp.
    XrPosef pose=pose_relative(tracking_head,relative_pose(views[eye].pose));
    pose.orientation=(XrQuaternionf){0,0,0,1}; // parallel native-camera eye views
    XrFovf f=frame_fov;
    vr_eye_projection(q,f.angleLeft,f.angleRight,f.angleUp,f.angleDown,1,100000);
    // Match game's depth convention (including reverse Z) so its depth tests remain valid.
    q[10]=p[10]; q[14]=p[14];
    if(!world) {
        // HUD/menu plane 2 metres in front of the original view, maintaining its 16:9 aspect.
        float z=2*scale;
        vr_identity(p); p[0]=z*.70f; p[5]=z*.39375f; p[10]=0; p[14]=-z;
        vr_pose_inverse(v,pose.orientation.x,pose.orientation.y,pose.orientation.z,pose.orientation.w,
                        pose.position.x*scale,pose.position.y*scale,pose.position.z*scale);
        vr_multiply(tmp,v,p); vr_multiply(out,q,tmp);
        return;
    }
    if(!vr_inverse(inv,p)) return;
    vr_pose_inverse(v,pose.orientation.x,pose.orientation.y,pose.orientation.z,pose.orientation.w,
                    pose.position.x*scale,pose.position.y*scale,pose.position.z*scale);
    vr_multiply(tmp,v,inv); vr_multiply(out,q,tmp);

}

static float eye_clip_cache[2][2][16];
static bool eye_clip_valid[2][2];
void vr_eye_replay_begin(void) { memset(eye_clip_valid,0,sizeof eye_clip_valid); }
void vr_clip_matrix(float out[16],bool world,bool flip_y,bool depth_minus_one,const void *uniforms,uint64_t bytes) {
    if(eye<0) { vr_identity(out); return; }
    if(!eye_clip_valid[world][flip_y]) {
        vr_clip_matrix_uncached(eye_clip_cache[world][flip_y],world,flip_y,depth_minus_one,uniforms,bytes);
        eye_clip_valid[world][flip_y]=true;
    }
    memcpy(out,eye_clip_cache[world][flip_y],16*sizeof(float));
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

void vr_frame(InterpFrame *frame,NvnTexture *texture,uint32_t epoch) {
    if(!frame) return;
    bool completed=false;
    freeze_projection();
    XrPosef simulation_head=frame_head;
    unsigned check_samples=1;
    const char *sample_env=getenv("ODYSSEY_VR_CHECK_SAMPLES");
    if(offscreen_check && sample_env && atoi(sample_env)>1) check_samples=(unsigned)atoi(sample_env);
    if(check_samples>4) check_samples=4;
    bool high_rate=rate_requested() && !offscreen_check;
    multiple_samples=high_rate || check_samples>1;
    bool blend=false;
    bool head_motion=previous_camera_valid && vr_head_changed(previous_head,simulation_head);
    const char *motion_interp=getenv("ODYSSEY_VR_MOTION_INTERP");
    bool stable_camera=!(motion_interp && atoi(motion_interp)) && (head_motion || previous_head_motion);
    previous_head_motion=head_motion;
    if(stable_camera) motion_frames++;
    if(multiple_samples && !stable_camera) {
        uint64_t match_at=SDL_GetTicksNS();
        blend=interp_frame_begin(frame);
        cost_add(C_MATCH,match_at);
        if(!interp_replay_ready()) goto done;
    } else if(!interp_frame_current(frame)) goto done;
    if(!previous_camera_valid) blend=false;
    for(int i=0;i<16;i++)
        if(fabsf(frame_projection[i]-previous_projection[i])>.0001f) blend=false;
    rate_stats(true,false,false);
    if(offscreen_check) {
        extern uint64_t renderer_present_count(void);
        unsigned long at=strtoul(getenv("ODYSSEY_VR_OFFSCREEN_CHECK"),NULL,10);
        unsigned long count=1;
        const char *count_env=getenv("ODYSSEY_VR_CHECK_FRAMES");
        if(count_env && strtoul(count_env,NULL,10)>0) count=strtoul(count_env,NULL,10);
        bool capture = renderer_present_count()>=at;
        if(renderer_present_count()+30>=at) {
            extern void renderer_save_texture_tex(TexBackend *,const char *);
            extern TexBackend *vk_tex_replayed(NvnTexture *);
            extern uint32_t g_interp_replay_epoch;
            if(capture) renderer_save_texture_tex(vk_tex(texture,true),"build/vr_reference.png");
            for(unsigned sample=0;sample<check_samples;sample++) {
                float alpha=(sample+.5f)/check_samples;
                bool prepared=check_samples>1 && blend && interp_prepare(alpha);
                if(!prepared) interp_use_current();
                frame_head=prepared?vr_pose_blend(previous_head,simulation_head,alpha):simulation_head;
                if(capture && check_samples>1) LOG("[vr-check] sample %u/%u alpha %.3f blended %d\n",sample+1,check_samples,alpha,prepared);
            for(int order=0;order<2;order++) if(interp_replay_lock()) {
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
                }
                extern bool g_debug_frame;
                g_debug_frame=capture && i==1 && getenv("ODYSSEY_VR_CHECK_PASSES");
                renderer_interp_replay();
                g_debug_frame=false;
                if(capture) interp_replay_report();
                if(capture) {
                    char path[160];
                    if(check_samples>1) snprintf(path,sizeof path,"build/vr_rate_%llu_s%u_%c.png",(unsigned long long)renderer_present_count(),sample,i?'R':'L');
                    else if(count>1) snprintf(path,sizeof path,"build/vr_sequence_%llu_%c.png",(unsigned long long)renderer_present_count(),i?'R':'L');
                    else snprintf(path,sizeof path,"build/vr_neutral_%s.png",i?"right":"left");
                    renderer_save_texture_tex(vk_tex_replayed(texture),path);
                }
                interp_replay_unlock();
            }
            }
            completed=true;
            if(capture) {
                LOG("[vr-check] captured sequence frame %llu\n",(unsigned long long)renderer_present_count());
                if(renderer_present_count()>=at+count-1) { LOG("[vr-check] neutral replay capture complete\n"); fflush(stderr); exit(0); }
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
    int64_t period=fs.predictedDisplayPeriod;
    if(period>0) {
        atomic_store(&display_period_ns,period);
        atomic_store(&target_vr_hz,(float)(1e9/period));
    }
    if(high_rate && !headset_samples) {
        vr_rate_begin(&refresh_clock,fs.predictedDisplayTime);
        multiple_samples=vr_rate_next_within(&refresh_clock,fs.predictedDisplayTime,period);
    }
    float alpha=high_rate?vr_rate_alpha(&refresh_clock,fs.predictedDisplayTime):1;
    uint64_t prep_at=SDL_GetTicksNS();
    bool prepared=high_rate && blend && interp_prepare(alpha);
    cost_add(C_PREP,prep_at);
    if(!prepared) interp_use_current();
    frame_head=prepared?vr_pose_blend(previous_head,simulation_head,alpha):simulation_head;
    XrFrameBeginInfo bi={.type=XR_TYPE_FRAME_BEGIN_INFO};
    if(!xr_ok(pBeginFrame(session,&bi),"begin frame")) goto done;
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
            center=views[0].pose;
            center.position.x=(views[0].pose.position.x+views[1].pose.position.x)*.5f;
            center.position.y=(views[0].pose.position.y+views[1].pose.position.y)*.5f;
            center.position.z=(views[0].pose.position.z+views[1].pose.position.z)*.5f;
            centered=true; LOG("[vr] view recentered\n");
        }
        if(rendered) {
            update_camera_target();
            XrPosef head=relative_pose(views[0].pose),right=relative_pose(views[1].pose);
            head.position.x=(head.position.x+right.position.x)*.5f;
            head.position.y=(head.position.y+right.position.y)*.5f;
            head.position.z=(head.position.z+right.position.z)*.5f;
            pthread_mutex_lock(&camera_lock);
            tracking_head=head;
            native_head=head;
            pthread_mutex_unlock(&camera_lock);
        }
        recenter_down=key;
    }
    bool acquired[2]={false,false};
    uint64_t pair_seq=0;
    if(rendered) for(int i=0;i<2;i++) {
        uint32_t index;
        XrSwapchainImageAcquireInfo ai={.type=XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if(!xr_ok(pAcquireSwapchainImage(targets[i].swapchain,&ai,&index),"acquire eye")) { rendered=false; break; }
        XrSwapchainImageWaitInfo swi={.type=XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,.timeout=XR_INFINITE_DURATION};
        if(!xr_ok(pWaitSwapchainImage(targets[i].swapchain,&swi),"wait eye")) { rendered=false; break; }
        acquired[i]=true;
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
            rendered=copy_eye(eye_image,i,index);
            if(rendered) pair_seq=renderer_fence_seq();
            interp_replay_unlock();
        } else rendered=false;
        eye=-1;

        projection[i]=(XrCompositionLayerProjectionView){.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
            .pose=rendered_eye_pose(i),.fov=frame_fov,.subImage={.swapchain=targets[i].swapchain,
                .imageRect={{0,0},{(int32_t)targets[i].width,(int32_t)targets[i].height}},.imageArrayIndex=0}};
        if(!rendered) break;
    }
    if(pair_seq) {
        uint64_t gpu_at=SDL_GetTicksNS();
        renderer_submit();
        renderer_wait_seq(pair_seq);
        cost_add(C_GPUWAIT,gpu_at);
    }
    for(int i=0;i<2;i++) if(acquired[i]) {
        XrSwapchainImageReleaseInfo ri={.type=XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        if(!xr_ok(pReleaseSwapchainImage(targets[i].swapchain,&ri),"release eye")) rendered=false;
    }
    XrCompositionLayerProjection layer={.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION,.space=space,.viewCount=2,.views=projection};
    const XrCompositionLayerBaseHeader *layers[]={(XrCompositionLayerBaseHeader *)&layer};
    XrFrameEndInfo ei={.type=XR_TYPE_FRAME_END_INFO,.displayTime=fs.predictedDisplayTime,
        .environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE,.layerCount=rendered?1:0,.layers=layers};
    uint64_t end_at=SDL_GetTicksNS();
    xr_ok(pEndFrame(session,&ei),"end frame");
    cost_add(C_END,end_at);
    completed=rendered;
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
}

// Scale the 2D layout within the existing eye image, without moving it into a 3D plane.
float vr_ui_scale(void) {
    static float value=-1;
    if(value<0) {
        const char *e=getenv("ODYSSEY_VR_UI_SCALE");
        value=e?strtof(e,NULL):.65f;
        if(!isfinite(value) || value<.35f || value>1) value=.65f;
        LOG("[vr] UI layout scale %.0f%%\n",value*100);
    }
    return eye>=0?value:1;
}
