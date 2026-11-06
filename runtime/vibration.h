#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define VIBRATION_TIMEOUT_NS UINT64_C(250000000)
typedef struct { float low,high;uint64_t stamp;bool valid; } VibrationSide;
typedef struct { VibrationSide side[2]; } VibrationMixer;
typedef struct { float low,high,hand[2];uint64_t remaining[2]; } VibrationOutput;
static inline float vibration_amplitude(float a) {
    return isfinite(a)?fminf(fmaxf(a,0),1):0;
}
static inline float vibration_motor(float a) {
    a=vibration_amplitude(a);
    return a<.015f?0:(a+powf(a,.4f))*.5f;
}
static inline void vibration_publish(VibrationMixer *m,int side,float low,float high,uint64_t now) {
    if(side<0||side>1)return;
    m->side[side]=(VibrationSide){vibration_amplitude(low),vibration_amplitude(high),now,true};
}
static inline VibrationOutput vibration_output(const VibrationMixer *m,uint64_t now,bool enabled) {
    VibrationOutput o={0};
    if(!enabled)return o;
    for(int i=0;i<2;i++) {
        const VibrationSide *s=&m->side[i];
        if(!s->valid||now<s->stamp||now-s->stamp>=VIBRATION_TIMEOUT_NS)continue;
        float low=vibration_motor(s->low),high=vibration_motor(s->high);
        o.low=fmaxf(o.low,low);o.high=fmaxf(o.high,high);o.hand[i]=fmaxf(low,high);
        o.remaining[i]=VIBRATION_TIMEOUT_NS-(now-s->stamp);
    }
    return o;
}
