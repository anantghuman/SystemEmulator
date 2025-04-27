/**************************************************************************
 * C S 429 system emulator
 *
 * cache.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions, both dirty and clean.  The replacement policy is LRU.
 *     The cache is a writeback cache.
 *
 * Copyright (c) 2021, 2023, 2024, 2025.
 * Authors: M. Hinton, Z. Leeper.
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/
#include "cache.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ADDRESS_LENGTH 64

/* Counters used to record cache statistics in printSummary().
   test-cache uses these numbers to verify correctness of the cache. */

// Increment when a miss occurs
int miss_count = 0;

// Increment when a hit occurs
int hit_count = 0;

// Increment when a dirty eviction occurs
int dirty_eviction_count = 0;

// Increment when a clean eviction occurs
int clean_eviction_count = 0;

/* STUDENT TO-DO: add more globals, structs, macros if necessary */
uword_t next_lru;

static size_t _log(size_t x) {
    size_t result = 0;
    while (x >>= 1) {
        result++;
    }
    return result;
}

/*
 * Initialize the cache according to specified arguments
 * Called by cache-runner so do not modify the function signature
 *
 * The code provided here shows you how to initialize a cache structure
 * defined above. It's not complete and feel free to modify/add code.
 */
cache_t *create_cache(int A_in, int B_in, int C_in, int d_in) {
    /* see cache-runner for the meaning of each argument */
    cache_t *cache = malloc(sizeof(cache_t));
    cache->A = A_in;
    cache->B = B_in;
    cache->C = C_in;
    cache->d = d_in;
    unsigned int S = cache->C / (cache->A * cache->B);

    cache->sets = (cache_set_t *)calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++) {
        cache->sets[i].lines =
            (cache_line_t *)calloc(cache->A, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->A; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].lru = 0;
            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].data = calloc(cache->B, sizeof(byte_t));
        }
    }

    /* TODO: add more code for initialization */
    next_lru = 0;
    return cache;
}

cache_t *create_checkpoint(cache_t *cache) {
    unsigned int S = (unsigned int)cache->C / (cache->A * cache->B);
    cache_t *copy_cache = malloc(sizeof(cache_t));
    memcpy(copy_cache, cache, sizeof(cache_t));
    copy_cache->sets = (cache_set_t *)calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++) {
        copy_cache->sets[i].lines =
            (cache_line_t *)calloc(cache->A, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->A; j++) {
            memcpy(&copy_cache->sets[i].lines[j], &cache->sets[i].lines[j],
                   sizeof(cache_line_t));
            copy_cache->sets[i].lines[j].data =
                calloc(cache->B, sizeof(byte_t));
            memcpy(copy_cache->sets[i].lines[j].data,
                   cache->sets[i].lines[j].data, sizeof(byte_t));
        }
    }

    return copy_cache;
}

void display_set(cache_t *cache, unsigned int set_index) {
    unsigned int S = (unsigned int)cache->C / (cache->A * cache->B);
    if (set_index < S) {
        cache_set_t *set = &cache->sets[set_index];
        for (unsigned int i = 0; i < cache->A; i++) {
            printf("Valid: %d Tag: %llx Lru: %lld Dirty: %d\n",
                   set->lines[i].valid, set->lines[i].tag, set->lines[i].lru,
                   set->lines[i].dirty);
        }
    } else {
        printf("Invalid Set %d. 0 <= Set < %d\n", set_index, S);
    }
}

/*
 * Free allocated memory. Feel free to modify it
 */
void free_cache(cache_t *cache) {
    unsigned int S = (unsigned int)cache->C / (cache->A * cache->B);
    for (unsigned int i = 0; i < S; i++) {
        for (unsigned int j = 0; j < cache->A; j++) {
            free(cache->sets[i].lines[j].data);
        }
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

unsigned int get_tag(uword_t addr, cache_t *cache) {
    unsigned int S = cache->C / (cache->A * cache->B);
    unsigned int s = _log(S);
    unsigned int b = _log(cache->B);
    return addr >> (b + s);
}

unsigned int get_index(uword_t addr, cache_t *cache) {
    unsigned int S = cache->C / (cache->A * cache->B);
    unsigned int s = _log(S);
    unsigned int b = _log(cache->B);
    return (addr >> b) & ((1 << s) - 1);
}
/* STUDENT TO-DO:
 * Get the line for address contained in the cache
 * On hit, return the cache line holding the address
 * On miss, returns NULL
 */
cache_line_t *get_line(cache_t *cache, uword_t addr) {
    // Student TODO
    unsigned i = get_index(addr, cache);
    unsigned t = get_tag(addr, cache);
    cache_line_t *line = (cache->sets)[i].lines;
    for (int j = 0; j < cache->A; j++) {
        if (line[j].valid && line[j].tag == t) {
            return &line[j];
        }
    }
    return NULL;
}

/* STUDENT TO-DO:
 * Select the line to fill with the new cache line
 * Return the cache line selected to filled in by addr
 */
cache_line_t *select_line(cache_t *cache, uword_t addr) {
    // Student TODO
    unsigned i = get_index(addr, cache);
    unsigned t = get_tag(addr, cache);
    cache_line_t *line = (cache->sets)[i].lines;
    cache_line_t *empty = NULL;
    cache_line_t *lru = NULL;
    for (int j = 0; j < cache->A; j++) {
        if (line[j].valid && line[j].tag == t) {
            return &line[j];
        }
        if (!empty && !line[j].valid) {
            empty = &line[j];
        }
        if (lru == NULL || line[j].lru < lru->lru) {
            lru = &line[j];
        }
    }
    if (empty) return empty;
    return lru;
}

/*  STUDENT TO-DO:
 *  Check if the address is hit in the cache, updating hit and miss data.
 *  Return true if pos hits in the cache.
 */
bool check_hit(cache_t *cache, uword_t addr, operation_t operation) {
    // Student TODO
    
    unsigned i = get_index(addr, cache);
    unsigned t = get_tag(addr, cache);
    cache_line_t *line = (cache->sets)[i].lines;
    for (int j = 0; j < cache->A; j++) {
        if (line[j].valid && line[j].tag == t) {
            hit_count++;
            line[j].lru = next_lru++;
            if (operation == WRITE) {
                line[j].dirty = 1;
            }
            return true;
        }
    }
    miss_count++;
    return false;
}

/*  STUDENT TO-DO:
 *  Handles Misses, evicting from the cache if necessary.
 *  Fill out the evicted_line_t struct with info regarding the evicted line.
 */
evicted_line_t *handle_miss(cache_t *cache, uword_t addr, operation_t operation,
                            byte_t *incoming_data) {
    evicted_line_t *evicted_line = malloc(sizeof(evicted_line_t));
    evicted_line->data = calloc(cache->B, sizeof(byte_t));
    // Student TODO
    
    unsigned i = get_index(addr, cache);
    unsigned t = get_tag(addr, cache);
    cache_line_t *line = select_line(cache, addr);

    if (line->dirty == true && line->valid == true) {
        dirty_eviction_count++;
        evicted_line->dirty = true;
    } else if (line->valid == true) {
        clean_eviction_count++;
        evicted_line->dirty = false;
    }
    unsigned s = _log(cache->C / (cache->A * cache->B));
    unsigned b = _log(cache->B);
    memcpy(evicted_line->data, line->data, cache->B);
    evicted_line->addr = (line->tag << (b + s)) | (i << b);
    evicted_line->valid = line->valid;

    line->tag = t;
    line->dirty = (operation == WRITE);
    line->valid = true;
    line->lru = ++next_lru;
    if (incoming_data)
        memcpy(line->data, incoming_data, cache->B);
    return evicted_line;
}

/* STUDENT TO-DO:
 * Get 8 bytes from the cache and write it to dest.
 * Preconditon: addr is contained within the cache.
 */
void get_word_cache(cache_t *cache, uword_t addr, word_t *dest) {
    // Student TODO
    unsigned i = get_index(addr, cache);
    unsigned t = get_tag(addr, cache);
    unsigned f = addr & ((1 << (_log(cache->B))) - 1);
    cache_line_t *line = (cache->sets)[i].lines;
    for (int j = 0; j < cache->A; j++) {
        if (line[j].valid && line[j].tag == t) {
            memcpy(dest, line[j].data + f, sizeof(word_t));
            return;
        }
    }
}

/* STUDENT TO-DO:
 * Set 8 bytes in the cache to val at pos.
 * Preconditon: addr is contained within the cache.
 */
void set_word_cache(cache_t *cache, uword_t addr, word_t val) {
    // Student TODO
    unsigned i = get_index(addr, cache);
    unsigned t = get_tag(addr, cache);
    unsigned f = addr & ((1 << (_log(cache->B))) - 1);
    cache_line_t *line = (cache->sets)[i].lines;
    for (int j = 0; j < cache->A; j++) {
        if (line[j].valid && line[j].tag == t) {
            memcpy(line[j].data + f, &val, sizeof(word_t));
            return;
        }
    }
}

/*
 * Access data at memory address addr
 * If it is already in cache, increase hit_count
 * If it is not in cache, bring it in cache, increase miss count
 * Also increase eviction_count if a line is evicted
 *
 * Called by cache-runner; no need to modify it if you implement
 * check_hit() and handle_miss()
 */
void access_data(cache_t *cache, uword_t addr, operation_t operation) {
    if (!check_hit(cache, addr, operation))
        free(handle_miss(cache, addr, operation, NULL));
}
