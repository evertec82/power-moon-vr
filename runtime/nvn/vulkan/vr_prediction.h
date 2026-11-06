#pragma once
#include <stdbool.h>
#include <stdint.h>
static bool vr_prediction_target(int64_t last_display,uint64_t last_host,uint64_t now,int64_t lead,int64_t *out){
 if(last_display<=0||now<last_host||now-last_host>UINT64_C(250000000))return false;
 if(lead<0)lead=0;if(lead>INT64_C(25000000))lead=INT64_C(25000000);
 *out=last_display+(int64_t)(now-last_host)+lead;return true;
}
static int64_t vr_prediction_lead(int64_t lead,int64_t error){
 if(error>INT64_C(100000000)||error< -INT64_C(100000000))return lead;
 lead+=error/8;return lead<0?0:lead>INT64_C(25000000)?INT64_C(25000000):lead;
}
