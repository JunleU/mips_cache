#ifndef _CACHE_H_
#define _CACHE_H_

#include "pipe.h"

#define ICACHE_SLOT_SIZE 32
#define DCACHE_SLOT_SIZE 32
#define ICACHE_WAYS_NUM  4
#define DCACHE_WAYS_NUM  8
#define ICACHE_SETS_NUM  64
#define DCACHE_SETS_NUM  256

#define ICACHE_SLOTS_NUM ICACHE_SETS_NUM * ICACHE_WAYS_NUM
#define DCACHE_SLOTS_NUM DCACHE_SETS_NUM * DCACHE_WAYS_NUM

typedef struct inst_slot
{
    uint8_t bytes[ICACHE_SLOT_SIZE];

} islot;

typedef struct data_slot
{
    uint8_t bytes[DCACHE_SLOT_SIZE];

} dslot;

typedef struct Inst_Cache
{
    islot *slots;
    uint32_t *tags;
    uint8_t *valid, *lru;

    uint32_t stat_hits, stat_misses;

} ICache;

typedef struct Data_Cache
{
    dslot *slots;
    uint32_t *tags;
    uint8_t *valid, *lru, *dirty;

    uint32_t stat_hits, stat_misses;

} DCache;

extern ICache inst_cache;
extern DCache data_cache;
extern Pipe_State pipe;

void icache_init();
void dcache_init();

uint32_t icache_read_32(uint32_t address);
uint32_t dcache_read_32(uint32_t address);

void dcache_write_32(uint32_t address, uint32_t value);

#endif
