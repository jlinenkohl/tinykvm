#include <tinykvm/c_api.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int load_file(const char* path, unsigned char** out_data, size_t* out_size)
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

int main(int argc, char** argv)
{
	const char* guest = (argc > 1) ? argv[1] : "./guest/tests/glibc_test";
	unsigned char* binary = NULL;
	size_t binary_size = 0;
	tkvm_machine_t* machine = NULL;
	tkvm_machine_t* forked = NULL;
	long rv = 0;
	uint64_t addr = 0;
	int full_reset = 0;

	if (load_file(guest, &binary, &binary_size) != 0) {
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
		unsigned char outbuf[16];
		unsigned char inbuf[16];
		for (size_t i = 0; i < sizeof(outbuf); i++) outbuf[i] = (unsigned char)(0xA0 + i);
		memset(inbuf, 0, sizeof(inbuf));
		if (tkvm_machine_copy_to_guest(machine, (uint64_t)rv, outbuf, sizeof(outbuf)) != TKVM_OK) {
			fprintf(stderr, "tkvm_machine_copy_to_guest failed: %s\n", tkvm_last_error());
			tkvm_machine_destroy(machine);
			return 1;
		}
		if (tkvm_machine_copy_from_guest(machine, inbuf, (uint64_t)rv, sizeof(inbuf)) != TKVM_OK) {
			fprintf(stderr, "tkvm_machine_copy_from_guest failed: %s\n", tkvm_last_error());
			tkvm_machine_destroy(machine);
			return 1;
		}
		if (memcmp(outbuf, inbuf, sizeof(outbuf)) != 0) {
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
	if (tkvm_machine_vmcall_u64(machine, "test_return", NULL, 7) == TKVM_OK) {
		fprintf(stderr, "vmcall_u64 unexpectedly accepted >6 arguments\n");
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
