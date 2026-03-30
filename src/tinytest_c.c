#include <tinykvm/c_api.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c_runner_utils.h"

#define GUEST_MEMORY   0x10000000ULL
#define GUEST_WORK_MEM (2U * 1024U * 1024U)
#define GUEST_OK       0x31337L

static int require_symbol(tkvm_machine_t* machine, const char* name)
{
	uint64_t addr = 0;
	if (tkvm_machine_address_of(machine, name, &addr) != TKVM_OK || addr == 0) {
		fprintf(stderr, "Missing symbol '%s': %s\n", name, tkvm_last_error());
		return -1;
	}
	return 0;
}

static int require_ret(tkvm_machine_t* machine, long expected, const char* where)
{
	long rv = 0;
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK) {
		fprintf(stderr, "%s return_value failed: %s\n", where, tkvm_last_error());
		return -1;
	}
	if (rv != expected) {
		fprintf(stderr, "%s expected %ld got %ld\n", where, expected, rv);
		return -1;
	}
	return 0;
}

int main(int argc, char** argv)
{
	const char* guest = (argc > 1) ? argv[1] : "./guest/tests/glibc_test";
	unsigned char* binary = NULL;
	size_t binary_size = 0;
	tkvm_machine_t* machine = NULL;
	tkvm_machine_t* forked = NULL;
	int full_reset = 0;
	long rv = 0;

	if (tkvm_load_file(guest, &binary, &binary_size) != 0) {
		fprintf(stderr, "Failed to load guest file: %s\n", guest);
		return 1;
	}
	if (tkvm_init(0) != TKVM_OK) {
		fprintf(stderr, "tkvm_init failed: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}

	struct tkvm_options opts;
	tkvm_options_set_defaults(&opts);
	opts.max_mem = GUEST_MEMORY;
	opts.max_cow_mem = GUEST_WORK_MEM;

	if (tkvm_machine_create(binary, binary_size, &opts, &machine) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_create failed: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	free(binary);

	{
		const char* vm_argv[] = {"kvmtest", "Hello World!\n"};
		const char* vm_env[] = {"LC_TYPE=C", "LC_ALL=C", "USER=root"};
		if (tkvm_machine_setup_linux(machine, vm_argv, 2, vm_env, 3) != TKVM_OK) {
			fprintf(stderr, "setup_linux failed: %s\n", tkvm_last_error());
			tkvm_machine_destroy(machine);
			return 1;
		}
	}

	if (tkvm_machine_run(machine, 2.0f) != TKVM_OK) {
		fprintf(stderr, "machine_run failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (require_ret(machine, GUEST_OK, "main") != 0) {
		tkvm_machine_destroy(machine);
		return 1;
	}
	printf("*** C Program startup OK\n");

	if (require_symbol(machine, "test_return") != 0 ||
		require_symbol(machine, "test_loop") != 0 ||
		require_symbol(machine, "test_compute_checksum") != 0) {
		tkvm_machine_destroy(machine);
		return 1;
	}

	if (tkvm_machine_vmcall0(machine, "test_return") != TKVM_OK || require_ret(machine, GUEST_OK, "test_return") != 0) {
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_vmcall0(machine, "test_syscall") != TKVM_OK || require_ret(machine, 555, "test_syscall") != 0) {
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_vmcall0(machine, "test_read") != TKVM_OK || require_ret(machine, 200, "test_read") != 0) {
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_vmcall0(machine, "test_malloc") != TKVM_OK) {
		fprintf(stderr, "test_malloc vmcall failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK || rv == 0) {
		fprintf(stderr, "test_malloc returned invalid ptr: %ld\n", rv);
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_guest_copy_roundtrip(machine, (uint64_t)rv, 32, 0x70) != 0) {
		fprintf(stderr, "guest copy roundtrip failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}

	{
		const uint64_t args[] = {42};
		if (tkvm_machine_vmcall_u64(machine, "write_value", args, 1) != TKVM_OK) {
			fprintf(stderr, "write_value vmcall failed: %s\n", tkvm_last_error());
			tkvm_machine_destroy(machine);
			return 1;
		}
		if (tkvm_machine_vmcall_u64(machine, "test_is_value", args, 1) != TKVM_OK || require_ret(machine, GUEST_OK, "test_is_value") != 0) {
			tkvm_machine_destroy(machine);
			return 1;
		}
	}

	{
		const uint64_t rounds = 200000;
		const uint64_t args[] = {rounds};
		if (tkvm_machine_vmcall_u64(machine, "test_compute_checksum", args, 1) != TKVM_OK) {
			fprintf(stderr, "test_compute_checksum vmcall failed: %s\n", tkvm_last_error());
			tkvm_machine_destroy(machine);
			return 1;
		}
		if (tkvm_machine_return_value(machine, &rv) != TKVM_OK || (uint64_t)rv != tkvm_host_compute_checksum(rounds)) {
			fprintf(stderr, "test_compute_checksum mismatch\n");
			tkvm_machine_destroy(machine);
			return 1;
		}
	}

	if (tkvm_machine_timed_vmcall0(machine, "test_loop", 1.0f) != TKVM_ERR_TIMEOUT) {
		fprintf(stderr, "Expected timeout from test_loop, got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	printf("*** C Timeout OK\n");

	if (tkvm_machine_fork(machine, &opts, &forked) != TKVM_ERR_INVALID_STATE) {
		fprintf(stderr, "Expected invalid-state before CoW prep, got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}

	if (tkvm_machine_prepare_copy_on_write(machine, 65536) != TKVM_OK) {
		fprintf(stderr, "prepare_copy_on_write failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_fork(machine, &opts, &forked) != TKVM_OK) {
		fprintf(stderr, "fork failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_vmcall0(forked, "test_return") != TKVM_OK || require_ret(forked, GUEST_OK, "forked test_return") != 0) {
		tkvm_machine_destroy(forked);
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_reset_to(forked, machine, &opts, &full_reset) != TKVM_OK) {
		fprintf(stderr, "reset_to failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(forked);
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (full_reset != 0 && full_reset != 1) {
		fprintf(stderr, "Invalid full_reset value: %d\n", full_reset);
		tkvm_machine_destroy(forked);
		tkvm_machine_destroy(machine);
		return 1;
	}
	printf("*** C Fork/Reset OK\n");

	tkvm_machine_destroy(forked);
	tkvm_machine_destroy(machine);
	printf("Nice! C API tests passed.\n");
	return 0;
}
