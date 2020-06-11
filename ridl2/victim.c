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

int main() {
	char* buffer = malloc(1);
	*buffer = 'a';
	while (1) {
        	_mm_clflush(buffer);
    		_mm_mfence();
		volatile char c = *buffer;
	}
}
