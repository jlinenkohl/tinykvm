#include <tinykvm/c_api.h>

int main(void)
{
	struct tkvm_options opts;
	tkvm_options_set_defaults(&opts);

	/* Compile/link smoke test for the C ABI surface. */
	(void)tkvm_last_error();
	return (opts.max_mem != 0) ? 0 : 1;
}
