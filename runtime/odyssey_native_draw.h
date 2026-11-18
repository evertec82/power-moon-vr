#pragma once
#include "cpu.h"
#include <stdbool.h>
#include <stdint.h>

void odyssey_native_draw_boot(void);
bool odyssey_skip_extra_particle_update(void);
uint64_t odyssey_native_present_deadline(void);
bool odyssey_stage_ready_for_draws(void);
unsigned odyssey_native_requested_hz(void);
void odyssey_native_delivery(unsigned hz, bool delivered, bool draw_only, uint64_t work);
