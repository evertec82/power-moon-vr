#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define CMD_SNAPSHOT_LIMIT ((size_t)64*1024*1024)
typedef struct {uint8_t *data;size_t size,capacity;} CommandSnapshot;
static bool command_snapshot_append(CommandSnapshot *s,const void *record,size_t size){
 if(!record||size<8||(size&7)||size>CMD_SNAPSHOT_LIMIT-s->size)return false;
 size_t need=s->size+size;
 if(need>s->capacity){size_t cap=s->capacity?s->capacity:65536;while(cap<need)cap*=2;
  void *p=realloc(s->data,cap);if(!p)return false;s->data=p;s->capacity=cap;}
 memcpy(s->data+s->size,record,size);s->size=need;return true;
}
