#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/shader_fingerprint.h"
static uint64_t legacy(const uint8_t *p,size_t n) {
    uint64_t h=1469598103934665603ull;
    while(n--) { h^=*p++;h*=1099511628211ull; }
    return h;
}
int main(void) {
    uint8_t bytes[512];for(int i=0;i<512;i++)bytes[i]=(uint8_t)(i*37);
    uint64_t before=nvn_shader_fingerprint(bytes,sizeof bytes);
    assert(before==legacy(bytes,sizeof bytes));
    bytes[321]^=0x80;uint64_t after=nvn_shader_fingerprint(bytes,sizeof bytes);
    assert(after==legacy(bytes,sizeof bytes) && after!=before);
    assert(nvn_shader_fingerprint(bytes,0)==1469598103934665603ull);
    puts("Fragment fingerprint equivalence / shader replacement tests passed");
}
