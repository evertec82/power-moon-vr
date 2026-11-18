#pragma once
#include <stdbool.h>
#include <stdint.h>

// Game-thread clock. Draw deadlines follow the display grid; game updates keep
// their original 60 Hz grid, including across refresh changes and brief stalls.
typedef struct {
    uint64_t next_tick, next_draw, last_start;
    unsigned hz, draw_remainder, tick_remainder;
} OdysseyDrawClock;
typedef struct { uint64_t deadline; bool tick; float lead; } OdysseyDrawStep;

static uint64_t draw_period(unsigned hz, unsigned *remainder) {
    unsigned total = 1000000000u + *remainder;
    *remainder = total % hz;
    return total / hz;
}

static OdysseyDrawStep odyssey_draw_step(OdysseyDrawClock *c, uint64_t now, unsigned hz) {
    if (!c->hz || now < c->last_start || now - c->last_start > 250000000 ||
        (now > c->next_tick && now - c->next_tick > 250000000)) {
        *c = (OdysseyDrawClock){.next_tick = now};
    }
    if (hz != c->hz) {
        c->next_draw = c->next_tick;
        c->draw_remainder = 0;
        c->hz = hz;
    }
    c->last_start = now;
    // If a visual-only slot is already late and the next tick is due, give
    // the slot to gameplay. Never build a queue of missed optional draws.
    if (c->next_draw < c->next_tick && now >= c->next_tick) {
        c->next_draw = c->next_tick;
        c->draw_remainder = 0;
    }
    uint64_t due = c->next_draw;
    bool tick = due + 1 >= c->next_tick;
    if (tick) c->next_tick += draw_period(60, &c->tick_remainder);
    c->next_draw += draw_period(hz, &c->draw_remainder);
    // Sample the animation near the display time. Never extrapolate farther
    // than half a tick or carry a prediction through a gameplay catch-up.
    double phase = 1.0 + ((int64_t)due - (int64_t)c->next_tick) * 60.0 / 1e9;
    float lead = hz == 60 ? 0 : (float)(phase - 0.5);
    if (lead > 0.5f) lead = 0.5f;
    if (lead < -0.5f) lead = -0.5f;
    return (OdysseyDrawStep){due, tick, lead};
}

typedef struct {
    unsigned hz, delivered, windows;
    uint64_t since, work;
    bool paused;
} OdysseyDrawBudget;

// Drop back to the release cadence if adding draws costs enough stereo pairs
// to lose the benefit. Probe again only after sustained spare render time.
static bool odyssey_draw_budget(OdysseyDrawBudget *b, uint64_t now, unsigned hz, bool delivered, uint64_t work) {
    bool was = b->paused;
    if (hz <= 60 || b->hz != hz || now < b->since || now - b->since > 2500000000ull)
        *b = (OdysseyDrawBudget){.hz=hz, .since=now};
    if (hz <= 60) return was;
    if (delivered) { ++b->delivered; b->work += work; }
    uint64_t elapsed = now - b->since;
    if (elapsed < 1000000000) return was != b->paused;
    double fps = b->delivered * 1e9 / elapsed;
    // The fallback measures a 60 Hz source. If there is enough room to beat it,
    // retry the headset grid even when every single display slot cannot fit.
    bool change = b->paused ? fps >= 57 && b->work < b->delivered * 15000000.0 : fps < 58;
    b->windows = change ? b->windows + 1 : 0;
    if (b->windows >= (b->paused ? 3u : 2u)) {
        b->paused = !b->paused;
        b->windows = 0;
    }
    b->since = now;
    b->delivered = 0;
    b->work = 0;
    return was != b->paused;
}
