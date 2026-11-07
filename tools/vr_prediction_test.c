#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_prediction.h"
int main(void){int64_t t;assert(!vr_prediction_target(0,0,0,0,&t));assert(!vr_prediction_target(100,1000,999,0,&t));assert(!vr_prediction_target(100,0,250000001,0,&t));
 assert(vr_prediction_forward(100,0)==100);
 assert(vr_prediction_forward(99,100)==100);
 assert(vr_prediction_forward(101,100)==101);
 // A half-rate/full-rate cadence switch can shorten the requested future time.
 int64_t previous=1050000000;
 assert(vr_prediction_target(1022000000,1000000000,1010000000,10000000,&t));
 assert(t<previous&&vr_prediction_forward(t,previous)==previous);
 assert(vr_prediction_target(1000000000,500000000,516666667,0,&t)&&t==1016666667);
 assert(vr_prediction_target(1000000000,0,100,50000000,&t)&&t==1025000100);
 int64_t lead=0;for(int i=0;i<100;i++)lead=vr_prediction_lead(lead,12000000-lead);assert(lead>11990000&&lead<=12000000);
 assert(vr_prediction_lead(0,-10000000)==0);assert(vr_prediction_lead(25000000,10000000)==25000000);assert(vr_prediction_lead(5,200000000)==5);puts("Fresh camera prediction targets, stale-clock fallback and bounded adaptive lead passed");}
