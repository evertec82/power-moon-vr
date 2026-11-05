#pragma once
#include <stdbool.h>
#include <stdint.h>
#define VR_SIM_PERIOD_NS UINT64_C(16666667)
#define VR_SIM_RESET_NS UINT64_C(250000000)
// Absolute deadlines preserve missed steps during ordinary render stalls. Long
// loading/suspension gaps are rebased instead of fast-forwarding gameplay.
typedef struct { uint64_t next; bool valid; } VrSimulationClock;
static uint64_t vr_simulation_step(VrSimulationClock *clock,uint64_t now) {
    if(!clock->valid || (now>clock->next && now-clock->next>VR_SIM_RESET_NS) ||
       (clock->next>now && clock->next-now>VR_SIM_RESET_NS)) {
        clock->next=now; clock->valid=true;
    }
    uint64_t deadline=clock->next;
    clock->next+=VR_SIM_PERIOD_NS;
    return deadline;
}
// Render admission has a configurable debt budget for isolated comparisons.
// Keep the confirmed 2-ms policy by default; no extra frames are queued.
static bool vr_simulation_late_budget(uint64_t now,uint64_t deadline,uint64_t budget) {
    return now>deadline && now-deadline>budget;
}
static bool vr_simulation_late(uint64_t now,uint64_t deadline) {
    return vr_simulation_late_budget(now,deadline,UINT64_C(2000000));
}
