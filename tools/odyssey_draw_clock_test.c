#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../runtime/odyssey_draw_clock.h"

int main(void) {
    const unsigned rates[] = {60, 72, 80, 90, 120, 144, 240};
    for (unsigned r = 0; r < sizeof rates / sizeof rates[0]; ++r) {
        unsigned rate = rates[r], ticks = 0;
        uint64_t now = 1000000000, origin = now;
        OdysseyDrawClock c = {0};
        for (unsigned i = 0; i < rate * 10; ++i) {
            OdysseyDrawStep s = odyssey_draw_step(&c, now, rate);
            assert(s.deadline == origin + (uint64_t)i * 1000000000 / rate);
            assert(fabsf(s.lead) <= 0.50001f);
            ticks += s.tick;
            now = s.deadline + 1000000;
        }
        assert(ticks == 600);
    }
    OdysseyDrawClock c = {0};
    OdysseyDrawStep first = odyssey_draw_step(&c, 1000000000, 90);
    assert(first.tick);
    OdysseyDrawStep late = odyssey_draw_step(&c, 1030000000, 90);
    assert(late.tick && late.deadline == 1016666666);
    assert(odyssey_draw_step(&c, 1031000000, 60).tick);
    uint64_t next = c.next_tick;
    assert(odyssey_draw_step(&c, 1035000000, 120).deadline == next);
    OdysseyDrawStep resumed = odyssey_draw_step(&c, 2000000000, 90);
    assert(resumed.tick && resumed.deadline == 2000000000);
    OdysseyDrawBudget b = {0};
    uint64_t t = 1000000000;
    for (int i=0; i<220; ++i, t+=11111111)
        odyssey_draw_budget(&b,t,90,(i & 1) == 0,13000000);
    assert(b.paused);
    for (int i=0; i<240; ++i, t+=16666667) odyssey_draw_budget(&b,t,90,true,16000000);
    assert(b.paused);
    for (int i=0; i<300; ++i, t+=16666667) odyssey_draw_budget(&b,t,90,true,13000000);
    assert(!b.paused);
    puts("Native draw cadence: 60 ticks/s at 60-240 Hz, late frames, rate changes and suspend passed");
}
