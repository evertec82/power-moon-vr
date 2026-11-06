#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/command_snapshot.h"
int main(void){CommandSnapshot s={0};
 uint64_t *source=malloc(128);for(int i=0;i<16;i++)source[i]=100+i;
 assert(command_snapshot_append(&s,source,128));memset(source,0,128);free(source);
 for(int i=0;i<16;i++)assert(((uint64_t *)s.data)[i]==100u+i);
 for(int j=0;j<2048;j++){uint64_t cmd[8]={(uint64_t)j,64};assert(command_snapshot_append(&s,cmd,sizeof cmd));}
 assert(((uint64_t *)s.data)[15]==115);
 assert(!command_snapshot_append(&s,NULL,8));assert(!command_snapshot_append(&s,s.data,7));
 size_t old=s.size;assert(!command_snapshot_append(&s,s.data,CMD_SNAPSHOT_LIMIT));assert(s.size==old);
 s.size=0;uint64_t next[2]={7,16};assert(command_snapshot_append(&s,next,16));assert(s.size==16&&((uint64_t *)s.data)[0]==7);
 free(s.data);puts("Owned commands survive source overwrite/free, growth and frame reuse; invalid/oversize records rejected");}
