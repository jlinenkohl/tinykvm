#include <tinykvm/c_api.h>

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
	long rv = 0;
	uint64_t addr = 0;

	if (load_file(guest, &binary, &binary_size) != 0) {
		fprintf(stderr, "failed to load guest file: %s\n", guest);
		return 1;
	}

	struct tkvm_options opts;
	tkvm_options_set_defaults(&opts);
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

	if (tkvm_machine_vmcall1_u64(machine, "write_value", 42) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_vmcall1_u64(write_value) failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_vmcall1_u64(machine, "test_is_value", 42) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_vmcall1_u64(test_is_value) failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK || rv != 0x31337) {
		fprintf(stderr, "unexpected return value from test_is_value: %ld\n", rv);
		tkvm_machine_destroy(machine);
		return 1;
	}

	tkvm_machine_destroy(machine);

	return 0;
}
