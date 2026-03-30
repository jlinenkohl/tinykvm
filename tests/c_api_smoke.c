#include <tinykvm/c_api.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/c_runner_utils.h"

#ifndef TKVM_CAPI_VERSION_NUMBER
#error "Missing TKVM_CAPI_VERSION_NUMBER"
#endif
#ifndef TKVM_CAPI_FEATURE_TYPED_ERRORS
#error "Missing TKVM_CAPI_FEATURE_TYPED_ERRORS"
#endif
#ifndef TKVM_CAPI_FEATURE_GUEST_COPY
#error "Missing TKVM_CAPI_FEATURE_GUEST_COPY"
#endif
#ifndef TKVM_CAPI_FEATURE_VMCALL_U64_ARRAY
#error "Missing TKVM_CAPI_FEATURE_VMCALL_U64_ARRAY"
#endif
#ifndef TKVM_CAPI_FEATURE_FORK_RESET
#error "Missing TKVM_CAPI_FEATURE_FORK_RESET"
#endif

static int require_last_error_contains(const char* needle, const char* where)
{
	const char* err = tkvm_last_error();
	if (err == NULL || err[0] == '\0') {
		fprintf(stderr, "expected non-empty tkvm_last_error at %s\n", where);
		return -1;
	}
	if (needle != NULL && strstr(err, needle) == NULL) {
		fprintf(stderr, "expected tkvm_last_error to contain '%s' at %s, got: %s\n", needle, where, err);
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
	long rv = 0;
	int rc = TKVM_OK;
	uint64_t addr = 0;
	int full_reset = 0;

	if (tkvm_load_file(guest, &binary, &binary_size) != 0) {
		fprintf(stderr, "failed to load guest file: %s\n", guest);
		return 1;
	}

	struct tkvm_options opts;
	tkvm_options_set_defaults(&opts);
	opts.max_mem = 0x10000000ULL; /* 256MB */
	opts.max_cow_mem = 2U * 1024U * 1024U; /* 2MB */
	if (tkvm_init(0) != TKVM_OK) {
		fprintf(stderr, "tkvm_init failed: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (tkvm_machine_create(NULL, binary_size, &opts, &machine) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from create(NULL, ...), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (require_last_error_contains("Invalid arguments", "create(NULL)") != 0) {
		free(binary);
		return 1;
	}
	if (tkvm_machine_create(binary, 0, &opts, &machine) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from create(size=0), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (tkvm_machine_create(binary, binary_size, &opts, NULL) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from create(out=NULL), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (tkvm_machine_run(NULL, 0.1f) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from run(NULL), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (tkvm_machine_setup_linux(NULL, NULL, 0, NULL, 0) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from setup_linux(NULL), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (tkvm_machine_return_value(NULL, &rv) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from return_value(NULL, ...), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (tkvm_machine_return_value(NULL, NULL) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from return_value(NULL, NULL), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (tkvm_machine_vmcall_addr_u64(NULL, 0x1, NULL, 0) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from vmcall_addr_u64(NULL, ...), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (tkvm_machine_prepare_copy_on_write(NULL, 4096) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from prepare_copy_on_write(NULL), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	if (tkvm_machine_fork(NULL, &opts, &forked) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from fork(NULL, ...), got: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}

	if (tkvm_machine_create(binary, binary_size, &opts, &machine) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_create failed: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	free(binary);

	if (tkvm_machine_copy_to_guest(machine, 0, NULL, 1) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from copy_to_guest(data=NULL,len>0), got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_copy_from_guest(machine, NULL, 0, 1) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from copy_from_guest(dst=NULL,len>0), got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_address_of(machine, NULL, &addr) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from address_of(symbol=NULL), got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_address_of(machine, "test_return", NULL) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from address_of(out=NULL), got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_vmcall0(machine, NULL) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from vmcall0(symbol=NULL), got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_vmcall_addr_u64(machine, 0, NULL, 0) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from vmcall_addr_u64(addr=0), got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_timed_vmcall0(machine, NULL, 1.0f) != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument from timed_vmcall0(symbol=NULL), got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}

	{
		const char* vm_argv[] = {"kvmtest", "Hello World!\n"};
		const char* vm_env[] = {"LC_TYPE=C", "LC_ALL=C", "USER=root"};
		if (tkvm_machine_setup_linux(machine, vm_argv, 2, vm_env, 3) != TKVM_OK) {
			fprintf(stderr, "tkvm_machine_setup_linux failed: %s\n", tkvm_last_error());
			tkvm_machine_destroy(machine);
			return 1;
		}
	}

	if (tkvm_machine_run(machine, 2.0f) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_run failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK || rv != 0x31337) {
		fprintf(stderr, "unexpected return value from guest main: %ld\n", rv);
		tkvm_machine_destroy(machine);
		return 1;
	}

	rc = tkvm_machine_address_of(machine, "symbol_that_does_not_exist", &addr);
	if (rc != TKVM_ERR_SYMBOL_NOT_FOUND) {
		fprintf(stderr, "expected symbol-not-found from address_of, got: %d (%s)\n",
			rc, tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (require_last_error_contains("Symbol not found", "address_of(missing)") != 0) {
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_vmcall0(machine, "symbol_that_does_not_exist") != TKVM_ERR_SYMBOL_NOT_FOUND) {
		fprintf(stderr, "expected symbol-not-found from vmcall0, got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (require_last_error_contains("Symbol not found", "vmcall0(missing)") != 0) {
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_timed_vmcall0(machine, "test_loop", 1.0f) != TKVM_ERR_TIMEOUT) {
		fprintf(stderr, "expected timeout from timed_vmcall0(test_loop), got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (require_last_error_contains(NULL, "timed_vmcall0(timeout)") != 0) {
		tkvm_machine_destroy(machine);
		return 1;
	}

	if (tkvm_machine_address_of(machine, "test_return", &addr) != TKVM_OK || addr == 0) {
		fprintf(stderr, "tkvm_machine_address_of failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_vmcall0(machine, "test_return") != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_vmcall0 failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK || rv != 0x31337) {
		fprintf(stderr, "unexpected return value from test_return: %ld\n", rv);
		tkvm_machine_destroy(machine);
		return 1;
	}

	if (tkvm_machine_vmcall0(machine, "test_malloc") != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_vmcall0(test_malloc) failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK || rv == 0) {
		fprintf(stderr, "unexpected return value from test_malloc: %ld\n", rv);
		tkvm_machine_destroy(machine);
		return 1;
	}
	{
		if (tkvm_guest_copy_roundtrip(machine, (uint64_t)rv, 16, 0xA0) != 0) {
			fprintf(stderr, "copy roundtrip mismatch\n");
			tkvm_machine_destroy(machine);
			return 1;
		}
	}

	{
		const uint64_t args[] = {42};
		if (tkvm_machine_vmcall_u64(machine, "write_value", args, 1) != TKVM_OK) {
			fprintf(stderr, "tkvm_machine_vmcall_u64(write_value) failed: %s\n", tkvm_last_error());
			tkvm_machine_destroy(machine);
			return 1;
		}
	}
	if (tkvm_machine_vmcall_addr_u64(machine, addr, NULL, 0) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_vmcall_addr_u64(test_return) failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK || rv != 0x31337) {
		fprintf(stderr, "unexpected return value from vmcall_addr_u64 test_return: %ld\n", rv);
		tkvm_machine_destroy(machine);
		return 1;
	}
	{
		const uint64_t args[] = {42};
		if (tkvm_machine_vmcall_u64(machine, "test_is_value", args, 1) != TKVM_OK) {
			fprintf(stderr, "tkvm_machine_vmcall_u64(test_is_value) failed: %s\n", tkvm_last_error());
			tkvm_machine_destroy(machine);
			return 1;
		}
	}
	rc = tkvm_machine_vmcall_u64(machine, "test_return", NULL, 7);
	if (rc != TKVM_INVALID_ARGUMENT) {
		fprintf(stderr, "expected invalid-argument for vmcall_u64 >6 args, got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK || rv != 0x31337) {
		fprintf(stderr, "unexpected return value from test_is_value: %ld\n", rv);
		tkvm_machine_destroy(machine);
		return 1;
	}

	if (tkvm_machine_timed_vmcall0(machine, "test_return", 1.0f) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_timed_vmcall0 failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK || rv != 0x31337) {
		fprintf(stderr, "unexpected return value from timed test_return: %ld\n", rv);
		tkvm_machine_destroy(machine);
		return 1;
	}

	if (tkvm_machine_fork(machine, &opts, &forked) != TKVM_ERR_INVALID_STATE) {
		fprintf(stderr, "expected invalid-state from fork before CoW prep, got: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (require_last_error_contains("not prepared", "fork-before-cow") != 0) {
		tkvm_machine_destroy(machine);
		return 1;
	}

	if (tkvm_machine_prepare_copy_on_write(machine, 65536) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_prepare_copy_on_write failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_fork(machine, &opts, &forked) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_fork failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	{
		tkvm_machine_t* nested = NULL;
		if (tkvm_machine_fork(forked, &opts, &nested) != TKVM_ERR_INVALID_STATE) {
			fprintf(stderr, "expected invalid-state from fork(forked), got: %s\n", tkvm_last_error());
			tkvm_machine_destroy(forked);
			tkvm_machine_destroy(machine);
			return 1;
		}
		if (require_last_error_contains("prepared", "fork-from-forked") != 0) {
			tkvm_machine_destroy(forked);
			tkvm_machine_destroy(machine);
			return 1;
		}
		if (nested != NULL) {
			tkvm_machine_destroy(nested);
		}
	}
	if (tkvm_machine_vmcall0(forked, "test_return") != TKVM_OK) {
		fprintf(stderr, "forked vmcall failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(forked);
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_return_value(forked, &rv) != TKVM_OK || rv != 0x31337) {
		fprintf(stderr, "unexpected return value from forked test_return: %ld\n", rv);
		tkvm_machine_destroy(forked);
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_reset_to(forked, machine, &opts, &full_reset) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_reset_to failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(forked);
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (full_reset != 0 && full_reset != 1) {
		fprintf(stderr, "unexpected full_reset flag: %d\n", full_reset);
		tkvm_machine_destroy(forked);
		tkvm_machine_destroy(machine);
		return 1;
	}

	tkvm_machine_destroy(forked);

	tkvm_machine_destroy(machine);

	return 0;
}
