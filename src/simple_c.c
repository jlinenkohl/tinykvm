#include <tinykvm/c_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "c_runner_utils.h"

static double elapsed_seconds(const struct timespec* start, const struct timespec* end)
{
	double sec = (double)(end->tv_sec - start->tv_sec);
	double nsec = (double)(end->tv_nsec - start->tv_nsec) / 1e9;
	return sec + nsec;
}

int main(int argc, char** argv)
{
	const char* filename = (argc > 1) ? argv[1] : "./guest/tests/glibc_test";
	const char* message = (argc > 2) ? argv[2] : "Hello World!";

	unsigned char* binary = NULL;
	size_t binary_size = 0;
	if (tkvm_load_file(filename, &binary, &binary_size) != 0) {
		fprintf(stderr, "Failed to load guest file: %s\n", filename);
		return 1;
	}

	if (tkvm_init(0) != TKVM_OK) {
		fprintf(stderr, "tkvm_init failed: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}

	struct tkvm_options options;
	tkvm_options_set_defaults(&options);

	tkvm_machine_t* machine = NULL;
	if (tkvm_machine_create(binary, binary_size, &options, &machine) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_create failed: %s\n", tkvm_last_error());
		free(binary);
		return 1;
	}
	free(binary);

	const char* vm_argv[] = {"kvmtest", message};
	const char* vm_env[] = {"LC_TYPE=C", "LC_ALL=C", "USER=root"};
	if (tkvm_machine_setup_linux(machine, vm_argv, 2, vm_env, 3) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_setup_linux failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}

	struct timespec t0;
	struct timespec t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	if (tkvm_machine_run(machine, 2.0f) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_run failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);

	long rv = 0;
	if (tkvm_machine_return_value(machine, &rv) != TKVM_OK) {
		fprintf(stderr, "tkvm_machine_return_value failed: %s\n", tkvm_last_error());
		tkvm_machine_destroy(machine);
		return 1;
	}

	printf("Time: %.6fs Return value: %ld\n", elapsed_seconds(&t0, &t1), rv);
	tkvm_machine_destroy(machine);
	return 0;
}
