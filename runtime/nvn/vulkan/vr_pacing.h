#pragma once
#include <stdbool.h>
#include <stdint.h>
#define VR_GAME_PERIOD_NS INT64_C(16666667)
typedef struct { int64_t start,deadline; unsigned samples; bool valid; int64_t source; bool source_valid; } VrRateClock;
static void vr_rate_begin(VrRateClock *clock,int64_t display_time) {
    int64_t gap=display_time-clock->deadline;
    if(!clock->valid || gap>4*VR_GAME_PERIOD_NS || gap< -4*VR_GAME_PERIOD_NS) {
        clock->start=display_time;
        clock->deadline=display_time+VR_GAME_PERIOD_NS;
        clock->valid=true;
    } else {
        clock->start=clock->deadline;
        clock->deadline+=VR_GAME_PERIOD_NS;
    }
    clock->samples=0;
}
static float vr_rate_alpha(const VrRateClock *clock,int64_t display_time) {
    double alpha=(double)(display_time-clock->start)/VR_GAME_PERIOD_NS;
    return alpha<0?0:alpha>1?1:(float)alpha;
}
static bool vr_rate_next_within(const VrRateClock *clock,int64_t display_time,int64_t display_period) {
    return display_period>0 && display_time+display_period < clock->deadline-display_period/1000;
}
static bool vr_rate_more(VrRateClock *clock,int64_t display_time,int64_t display_period) {
    clock->samples++;
    return clock->samples<8 && vr_rate_next_within(clock,display_time,display_period);
}

// Source deadlines advance even when a visual frame is skipped for simulation
// catch-up. Do not create a backlog of display samples for those missed frames.
static void vr_rate_begin_source(VrRateClock *clock,int64_t display_time,int64_t source_deadline) {
    int64_t elapsed=source_deadline-clock->source;
    if(clock->valid&&clock->source_valid&&elapsed>0&&elapsed<=INT64_C(250000000))
        clock->deadline+=elapsed-VR_GAME_PERIOD_NS;
    vr_rate_begin(clock,display_time);
    clock->source=source_deadline;clock->source_valid=true;
}
