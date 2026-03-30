#include "c_runner_utils.h"

#include <stdio.h>
#include <stdlib.h>

int tkvm_load_file(const char* path, unsigned char** out_data, size_t* out_size)
{
	FILE* f = fopen(path, "rb");
	if (f == NULL) {
		return -1;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	long size = ftell(f);
	if (size <= 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return -1;
	}
	unsigned char* data = (unsigned char*)malloc((size_t)size);
	if (data == NULL) {
		fclose(f);
		return -1;
	}
	if (fread(data, 1, (size_t)size, f) != (size_t)size) {
		free(data);
		fclose(f);
		return -1;
	}
	fclose(f);
	*out_data = data;
	*out_size = (size_t)size;
	return 0;
}

uint64_t tkvm_host_compute_checksum(uint64_t rounds)
{
	uint64_t acc = 0x243f6a8885a308d3ULL;
	uint64_t state = 0x9e3779b97f4a7c15ULL;

	for (uint64_t i = 0; i < rounds; i++) {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;

		acc ^= state + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2);
		acc = (acc << 9) | (acc >> (64 - 9));
		acc += i * 0x100000001b3ULL;
	}
	return acc & 0x7FFFFFFFFFFFFFFFULL;
}
