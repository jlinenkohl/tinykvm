#pragma once

#include <tinykvm/c_api.h>

#include <stddef.h>
#include <stdint.h>

int tkvm_load_file(const char* path, unsigned char** out_data, size_t* out_size);
uint64_t tkvm_host_compute_checksum(uint64_t rounds);
int tkvm_guest_copy_roundtrip(tkvm_machine_t* machine, uint64_t guest_addr, size_t len, unsigned char seed);
