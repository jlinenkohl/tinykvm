#pragma once

#include <stddef.h>
#include <stdint.h>

int tkvm_load_file(const char* path, unsigned char** out_data, size_t* out_size);
uint64_t tkvm_host_compute_checksum(uint64_t rounds);
