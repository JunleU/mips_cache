#include "cache.h"
#include <stdlib.h>
#include <string.h>

ICache inst_cache;
DCache data_cache;

void icache_init()
{
    inst_cache.slots = malloc(ICACHE_SLOTS_NUM * sizeof(islot));
    memset(inst_cache.slots, 0, ICACHE_SLOTS_NUM * sizeof(islot));
    inst_cache.tags = malloc(ICACHE_SLOTS_NUM * sizeof(uint32_t));
    memset(inst_cache.tags, 0, ICACHE_SLOTS_NUM * sizeof(uint32_t));
    inst_cache.valid = malloc(ICACHE_SLOTS_NUM);
    memset(inst_cache.valid, 0, ICACHE_SLOTS_NUM);
    inst_cache.lru = malloc(ICACHE_SLOTS_NUM);
    memset(inst_cache.lru, 0, ICACHE_SLOTS_NUM);

    inst_cache.stat_hits = 0;
    inst_cache.stat_misses = 0;
}

void dcache_init()
{
    data_cache.slots = malloc(DCACHE_SLOTS_NUM * sizeof(dslot));
    memset(data_cache.slots, 0, DCACHE_SLOTS_NUM * sizeof(dslot));
    data_cache.tags = malloc(DCACHE_SLOTS_NUM * sizeof(uint32_t));
    memset(data_cache.tags, 0, DCACHE_SLOTS_NUM * sizeof(uint32_t));
    data_cache.valid = malloc(DCACHE_SLOTS_NUM);
    memset(data_cache.valid, 0, DCACHE_SLOTS_NUM);
    data_cache.lru = malloc(DCACHE_SLOTS_NUM);
    memset(data_cache.lru, 0, DCACHE_SLOTS_NUM);
    data_cache.dirty = malloc(DCACHE_SLOTS_NUM);
    memset(data_cache.dirty, 0, DCACHE_SLOTS_NUM);

    data_cache.stat_hits = 0;
    data_cache.stat_misses = 0;
}

uint32_t icache_read_32(uint32_t address)
{
    uint32_t cache_offset = ((address >> 5) & 0x3F) * ICACHE_WAYS_NUM;
    uint32_t tag = address >> 11;
    int i;
    for (i = cache_offset; i < cache_offset + ICACHE_WAYS_NUM; i++) {
        if (inst_cache.valid[i] && inst_cache.tags[i] == tag) {
            inst_cache.stat_hits++;
            /* update lru */
            int j;
            for (j = cache_offset; j < cache_offset + ICACHE_WAYS_NUM; j++)
                if (inst_cache.valid[j] && inst_cache.lru[j] < inst_cache.lru[i])
                    inst_cache.lru[j]++;
            inst_cache.lru[i] = 0;

            uint32_t slot_offset = address & 0x1F;
            return 
                (inst_cache.slots[i].bytes[slot_offset + 3] << 24) |
                (inst_cache.slots[i].bytes[slot_offset + 2] << 16) |
                (inst_cache.slots[i].bytes[slot_offset + 1] <<  8) |
                (inst_cache.slots[i].bytes[slot_offset + 0] <<  0);
        }
    }
    
    /* TODO: model stall */
    if (pipe.fetch_stall == 0)
        pipe.fetch_stall = 50;
    else
        pipe.fetch_stall--;

    if (pipe.fetch_stall > 0)
        return 0;

    for (i = cache_offset; i < cache_offset + ICACHE_WAYS_NUM; i++) {
        if (!inst_cache.valid[i] || inst_cache.lru[i] == 3) {
            int j;
            for (j = cache_offset; j < cache_offset + ICACHE_WAYS_NUM; j++)
                if (inst_cache.valid[j] && j != i)
                    inst_cache.lru[j]++;
            inst_cache.lru[i] = 0;
            inst_cache.valid[i] = 1;
            
            /* copy from mem to slot (using read_mem_32) */
            uint32_t mem_offset = address & 0xFFFFFFE0;
            uint32_t slot_offset = address & 0x1F, ret_val;
            for (j = 0; j < ICACHE_SLOT_SIZE; j += 4) {
                uint32_t val = mem_read_32(mem_offset + j);
                if (j == slot_offset) ret_val = val;
                inst_cache.slots[i].bytes[j + 0] = (val >>  0) & 0xFF;
                inst_cache.slots[i].bytes[j + 1] = (val >>  8) & 0xFF;
                inst_cache.slots[i].bytes[j + 2] = (val >> 16) & 0xFF;
                inst_cache.slots[i].bytes[j + 3] = (val >> 24) & 0xFF;
            }

            inst_cache.tags[i] = tag;
            inst_cache.stat_misses++;

            return ret_val;
        }
    }
}

uint32_t dcache_read_32(uint32_t address)
{
    uint32_t cache_offset = ((address >> 5) & 0xFF) * DCACHE_WAYS_NUM;
    uint32_t tag = address >> 13;
    int i;
    for (i = cache_offset; i < cache_offset + DCACHE_WAYS_NUM; i++) {
        if (data_cache.valid[i] && data_cache.tags[i] == tag) {
            data_cache.stat_hits++;
            /* update lru */
            int j;
            for (j = cache_offset; j < cache_offset + DCACHE_WAYS_NUM; j++)
                if (data_cache.valid[j] && data_cache.lru[j] < data_cache.lru[i])
                    data_cache.lru[j]++;
            data_cache.lru[i] = 0;

            uint32_t slot_offset = address & 0x1F;
            return 
                (data_cache.slots[i].bytes[slot_offset + 3] << 24) |
                (data_cache.slots[i].bytes[slot_offset + 2] << 16) |
                (data_cache.slots[i].bytes[slot_offset + 1] <<  8) |
                (data_cache.slots[i].bytes[slot_offset + 0] <<  0);
        }
    }
    
    /* TODO: model stall */
    if (pipe.mem_stall == 0)
        pipe.mem_stall = 50;
    else
        pipe.mem_stall--; 
    /* if a memory access is in progress, decrement cycles until its end */
        
    if (pipe.mem_stall > 0)
        return 0;

    for (i = cache_offset; i < cache_offset + DCACHE_WAYS_NUM; i++) {
        if (!data_cache.valid[i] || data_cache.lru[i] == 7) {
            int j;

            uint32_t mem_offset;
            if (!data_cache.valid[i])
                data_cache.valid[i] = 1;
            else if (data_cache.dirty[i]) {
                pipe.mem_stall = 50;
                mem_offset = data_cache.tags[i] << 13 | (address & 0x1FE0);
                for (j = 0; j < DCACHE_SLOT_SIZE; j += 4)
                    mem_write_32(mem_offset + j,
                        data_cache.slots[i].bytes[j + 0] <<  0 |
                        data_cache.slots[i].bytes[j + 1] <<  8 |
                        data_cache.slots[i].bytes[j + 2] << 16 |
                        data_cache.slots[i].bytes[j + 3] << 24);
                data_cache.dirty[i] = 0;
            }

            if (pipe.mem_stall > 0)
                return 0;
            
            for (j = cache_offset; j < cache_offset + DCACHE_WAYS_NUM; j++)
                if (data_cache.valid[j] && j != i)
                    data_cache.lru[j]++;
            data_cache.lru[i] = 0;
            
            /* copy from mem to slot (using read_mem_32) */
            mem_offset = address & 0xFFFFFFE0;
            uint32_t slot_offset = address & 0x1F, ret_val;
            for (j = 0; j < DCACHE_SLOT_SIZE; j += 4) {
                uint32_t val = mem_read_32(mem_offset + j);
                if (j == slot_offset) ret_val = val;
                data_cache.slots[i].bytes[j + 0] = (val >>  0) & 0xFF;
                data_cache.slots[i].bytes[j + 1] = (val >>  8) & 0xFF;
                data_cache.slots[i].bytes[j + 2] = (val >> 16) & 0xFF;
                data_cache.slots[i].bytes[j + 3] = (val >> 24) & 0xFF;
            }

            data_cache.tags[i] = tag;
            data_cache.stat_misses++;

            return ret_val;
        }
    }
}

void dcache_write_32(uint32_t address, uint32_t value)
{
    uint32_t cache_offset = ((address >> 5) & 0xFF) * DCACHE_WAYS_NUM;
    uint32_t tag = address >> 13;
    int i;
    for (i = cache_offset; i < cache_offset + DCACHE_WAYS_NUM; i++) {
        if (data_cache.valid[i] && data_cache.tags[i] == tag) {
            /* don't need to update lru */

            uint32_t slot_offset = address & 0x1F;
            data_cache.slots[i].bytes[slot_offset + 0] = (value >>  0) & 0xFF;
            data_cache.slots[i].bytes[slot_offset + 1] = (value >>  8) & 0xFF;
            data_cache.slots[i].bytes[slot_offset + 2] = (value >> 16) & 0xFF;
            data_cache.slots[i].bytes[slot_offset + 3] = (value >> 24) & 0xFF;

            data_cache.dirty[i] = 1;
            return;
        }
    }
    /* there is always a read before a write, so always hit */
}