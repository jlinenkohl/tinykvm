#include "c_runner_utils.h"

#include <string.h>
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

int tkvm_guest_copy_roundtrip(tkvm_machine_t* machine, uint64_t guest_addr, size_t len, unsigned char seed)
{
	if (machine == NULL || guest_addr == 0 || len == 0) {
		return -1;
	}

	unsigned char* outbuf = (unsigned char*)malloc(len);
	unsigned char* inbuf = (unsigned char*)malloc(len);
	if (outbuf == NULL || inbuf == NULL) {
		free(outbuf);
		free(inbuf);
		return -1;
	}

	for (size_t i = 0; i < len; i++) {
		outbuf[i] = (unsigned char)(seed + (unsigned char)i);
	}
	memset(inbuf, 0, len);

	if (tkvm_machine_copy_to_guest(machine, guest_addr, outbuf, len) != TKVM_OK) {
		free(outbuf);
		free(inbuf);
		return -1;
	}
	if (tkvm_machine_copy_from_guest(machine, inbuf, guest_addr, len) != TKVM_OK) {
		free(outbuf);
		free(inbuf);
		return -1;
	}

	int ok = (memcmp(outbuf, inbuf, len) == 0) ? 0 : -1;
	free(outbuf);
	free(inbuf);
	return ok;
}
