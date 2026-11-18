// The extra native draw preparation and skeletal sampling are adapted from
// Super Odyssey PC Recomp. Only a warmed-up StageScene is eligible in VR.
#include "../rt.h"
#include "../odyssey_native_draw.h"
#include "../odyssey_draw_clock.h"
#include "../odyssey_pose.h"
#include "../ui/menu.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static _Atomic bool extra_draw;
static _Atomic uint64_t present_deadline;
static _Atomic unsigned wanted_hz;
static _Atomic bool over_budget;
static OdysseyDrawBudget budget;  // render owner

bool odyssey_skip_extra_particle_update(void) { return atomic_load(&extra_draw); }
uint64_t odyssey_native_present_deadline(void) { return atomic_exchange(&present_deadline, 0); }
unsigned odyssey_native_requested_hz(void) { return atomic_load(&wanted_hz); }
void odyssey_native_delivery(unsigned desired, bool delivered, bool draw_only, uint64_t work) {
    if (!desired) return;
    uint64_t now = plat_now_ns();
    static uint64_t since;
    static unsigned pairs, extras;
    if (desired <= 60 || !since) { since = now; pairs = extras = 0; }
    if (desired > 60 && delivered) { ++pairs; extras += draw_only; }
    if (now - since >= 2000000000) {
        LOG("[native-draw] %.2f stereo pairs/s; %u of %u from draws between gameplay ticks\n",
            pairs * 1e9 / (now - since), extras, pairs);
        since = now; pairs = extras = 0;
    }
    if (odyssey_draw_budget(&budget, now, desired, delivered, work)) {
        atomic_store(&over_budget, budget.paused);
        LOG("[native-draw] %s\n", budget.paused ? "stereo delivery below 58/s; resuming the 60 Hz draw cadence" :
            "draw budget reset; headset cadence available");
    }
}

#ifndef __APPLE__
static GuestFn framework_calc, controller_calc, game_movement, player_movement, proc_draw, stage_draw;
static uint64_t texture_allocator, texture_calc, prepare_graphics, get_camera;
static bool installed, stage_seen, stage_last_draw;
static uint64_t last_stage, simulation_tick, retry_camera_tick;
static unsigned hz;
static OdysseyDrawClock clock_state;
static OdysseyDrawStep step;

unsigned vr_native_display_hz(void);
void vr_native_draw_begin(void);
void vr_record_stage_camera(void);
bool vr_resample_draw_camera(float *fields);
void input_native_sample(bool tick);
void input_native_tick_end(void);
static unsigned observed_draws, observed_ticks, observed_extra, observed_camera;
static uint64_t report_at;

// The framework invokes controller calculation before GameSystem::movement.
// Decide once, so an extra draw consumes neither a button edge nor a physics tick.
static void calc(Cpu *c) {
    vr_native_draw_begin();
    unsigned desired = stage_last_draw && odyssey_stage_ready_for_draws() && !menu_is_open() ? vr_native_display_hz() : 60;
    atomic_store(&wanted_hz, desired);
    hz = atomic_load(&over_budget) || simulation_tick < retry_camera_tick ? 60 : desired;
    step = odyssey_draw_step(&clock_state, plat_now_ns(), hz);
    atomic_store(&present_deadline, step.deadline);
    atomic_store(&extra_draw, !step.tick);
    framework_calc(c);
}
static void controller(Cpu *c) {
    input_native_sample(step.tick);
    if (!step.tick) return;
    controller_calc(c);
    input_native_tick_end();
}
static void movement(Cpu *c) {
    if (!step.tick) return;
    ++observed_ticks;
    ++simulation_tick;
    bool sample = hz > 60 && odyssey_pose_enabled();
    if (sample) odyssey_pose_begin();
    game_movement(c);
    if (sample) odyssey_pose_end(simulation_tick);
}
static void player(Cpu *c) {
    odyssey_pose_player(c->x[0]);
    player_movement(c);
}
static void draw(Cpu *c) {
    if (!step.tick) {
        uint64_t allocator = *(const uint64_t *)(uintptr_t)texture_allocator;
        if (allocator) { Cpu q = *c; q.x[0] = allocator; guest_call(&q, texture_calc); }
    }
    stage_seen = false;
    proc_draw(c);
    stage_last_draw = stage_seen;
    if (!stage_seen) last_stage = 0;
    observed_draws++;
    uint64_t now = plat_now_ns();
    if (!report_at) report_at = now;
    if (now - report_at >= 2000000000) {
        double seconds = (now - report_at) / 1e9;
        LOG("[native-draw] target %u Hz; %.2f native draws/s, %.2f gameplay ticks/s; %u extra stage draws, %u camera samples\n",
            hz, observed_draws / seconds, observed_ticks / seconds, observed_extra, observed_camera);
        observed_draws = observed_ticks = observed_extra = observed_camera = 0;
        report_at = now;
    }
}
static void stage(Cpu *c) {
    uint64_t scene = c->x[0];
    bool continuous = hz > 60 && last_stage == scene;
    stage_seen = true;
    float saved[21], *camera = NULL;
    if (!step.tick) {
        ++observed_extra;
        if (continuous) {
            Cpu q = *c;
            q.x[0] = scene + 0x18;  // StageScene's IUseCamera, view zero
            q.x[1] = 0;
            guest_call(&q, get_camera);
            if (q.x[0] >= HEAP_BASE && q.x[0] <= HEAP_BASE + HEAP_MAX - 0x60) {
                camera = (float *)(uintptr_t)(q.x[0] + 8);
                memcpy(saved, camera, sizeof saved);
                if (vr_resample_draw_camera(camera)) ++observed_camera;
                else camera = NULL;
            }
        }
        if (!camera) retry_camera_tick = simulation_tick + 60;
        // Recreate per-draw resources, including the dynamic shadow targets.
        Cpu graphics = *c;
        guest_call(&graphics, prepare_graphics);
    }
    vr_record_stage_camera();
    if (continuous && step.lead != 0 && odyssey_pose_enabled()) odyssey_pose_draw(c, step.lead);
    stage_draw(c);
    if (camera) memcpy(camera, saved, sizeof saved);
    last_stage = scene;
}
void odyssey_native_draw_boot(void) {
    const char *request = getenv("ODYSSEY_VR_NATIVE_DRAW");
    const char *vr = getenv("ODYSSEY_VR"), *sim = getenv("ODYSSEY_VR_SIMULATION");
    if (!request || !atoi(request) || !vr || !atoi(vr) || !sim || !atoi(sim)) return;
    clock_state = (OdysseyDrawClock){0};
    step = (OdysseyDrawStep){.tick = true};
    last_stage = simulation_tick = retry_camera_tick = report_at = 0;
    observed_draws = observed_ticks = observed_extra = observed_camera = 0;
    stage_seen = stage_last_draw = false;
    atomic_store(&present_deadline, 0);
    atomic_store(&extra_draw, false);
    atomic_store(&wanted_hz, 60);
    atomic_store(&over_budget, false);
    budget = (OdysseyDrawBudget){0};
    if (installed) return;
    const Module *main = loader_module("main");
    const unsigned char id[] = {0x3c,0xa1,0x2d,0xfa,0xaf,0x9c,0x82,0xda};
    if (!main || memcmp(main->build_id, id, sizeof id)) return;
    struct { const char *name; GuestFn *original; GuestFn replacement; uint64_t address; } hooks[] = {
        {"_ZN4sead15GameFrameworkNx9procCalc_Ev", &framework_calc, calc},
        {"_ZN4sead13ControllerMgr4calcEv", &controller_calc, controller},
        {"_ZN10GameSystem8movementEv", &game_movement, movement},
        {"_ZN19PlayerActorHakoniwa8movementEv", &player_movement, player},
        {"_ZN2al15GameFrameworkNx9procDraw_Ev", &proc_draw, draw},
        {"_ZNK10StageScene8drawMainEv", &stage_draw, stage},
    };
    texture_allocator = loader_find_symbol("_ZN3agl3utl23DynamicTextureAllocator9sInstanceE");
    texture_calc = loader_find_symbol("_ZN3agl3utl23DynamicTextureAllocator4calcEv");
    prepare_graphics = loader_find_symbol("_ZN2al18updateGraphicsPrevEPNS_5SceneE");
    get_camera = loader_find_symbol("_ZN2al15getLookAtCameraEPKNS_10IUseCameraEi");
    if (!texture_allocator || !dispatch_lookup(texture_calc) || !dispatch_lookup(prepare_graphics) ||
        !dispatch_lookup(get_camera) || !odyssey_pose_enabled()) {
        LOG("[native-draw] missing stage preparation or animation support; using stock cadence\n");
        return;
    }
    for (unsigned i = 0; i < sizeof hooks / sizeof hooks[0]; ++i) {
        hooks[i].address = loader_find_symbol(hooks[i].name);
        *hooks[i].original = dispatch_lookup(hooks[i].address);
        if (!*hooks[i].original) { LOG("[native-draw] %s unavailable; using stock cadence\n", hooks[i].name); return; }
    }
    for (unsigned i = 0; i < sizeof hooks / sizeof hooks[0]; ++i)
        hle_override(hooks[i].address, hooks[i].replacement, hooks[i].name);
    installed = true;
    LOG("[native-draw] native stage draws enabled; gameplay and controller updates use 60 Hz\n");
}
#else
void odyssey_native_draw_boot(void) {}
#endif
