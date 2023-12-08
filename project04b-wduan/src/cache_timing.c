#include "cache_timing.h"

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

const size_t REPEATS = 100000;

int main() {
    uint64_t sum_miss = 0;
    uint64_t sum_hit = 0;

    // TODO: Implement the algorithm as described in the specification here
    for (size_t i = 0; i < REPEATS; i++) {
        page_t *page = malloc(sizeof(page_t));
        flush_cache_line((void *) page);
        uint64_t miss_time = time_read((void *) page);
        uint64_t hit_time = time_read((void *) page);
        if (hit_time < miss_time) {
            sum_miss += miss_time;
            sum_hit += hit_time;
        }
    }

    printf("average miss = %" PRIu64 "\n", sum_miss / REPEATS);
    printf("average hit  = %" PRIu64 "\n", sum_hit / REPEATS);
}
