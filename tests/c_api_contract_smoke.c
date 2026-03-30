#include <tinykvm/c_api.h>

#include <stdint.h>
#include <stdio.h>

static int require_true(int cond, const char* msg)
{
	if (!cond) {
		fprintf(stderr, "contract check failed: %s\n", msg);
		return -1;
	}
	return 0;
}

int main(void)
{
	if (require_true(TKVM_CAPI_VERSION_NUMBER ==
		((TKVM_CAPI_VERSION_MAJOR << 16) |
		 (TKVM_CAPI_VERSION_MINOR << 8) |
		 TKVM_CAPI_VERSION_PATCH),
		"version number composition")) {
		return 1;
	}

	if (require_true(TKVM_CAPI_FEATURE_TYPED_ERRORS == 1, "typed errors feature flag")) return 1;
	if (require_true(TKVM_CAPI_FEATURE_GUEST_COPY == 1, "guest copy feature flag")) return 1;
	if (require_true(TKVM_CAPI_FEATURE_VMCALL_U64_ARRAY == 1, "vmcall u64 array feature flag")) return 1;
	if (require_true(TKVM_CAPI_FEATURE_FORK_RESET == 1, "fork/reset feature flag")) return 1;

	if (require_true(TKVM_OK == 0, "TKVM_OK value")) return 1;
	if (require_true(TKVM_ERROR == -1, "TKVM_ERROR value")) return 1;
	if (require_true(TKVM_INVALID_ARGUMENT == -2, "TKVM_INVALID_ARGUMENT value")) return 1;
	if (require_true(TKVM_ERR_TIMEOUT == -10, "TKVM_ERR_TIMEOUT value")) return 1;
	if (require_true(TKVM_ERR_MEMORY == -11, "TKVM_ERR_MEMORY value")) return 1;
	if (require_true(TKVM_ERR_SYMBOL_NOT_FOUND == -12, "TKVM_ERR_SYMBOL_NOT_FOUND value")) return 1;
	if (require_true(TKVM_ERR_INVALID_STATE == -13, "TKVM_ERR_INVALID_STATE value")) return 1;
	if (require_true(TKVM_ERR_MACHINE == -14, "TKVM_ERR_MACHINE value")) return 1;

	if (require_true(TKVM_SNAPSHOT_DISABLED == 0, "TKVM_SNAPSHOT_DISABLED value")) return 1;
	if (require_true(TKVM_SNAPSHOT_OPEN == 1, "TKVM_SNAPSHOT_OPEN value")) return 1;
	if (require_true(TKVM_SNAPSHOT_CREATE == 2, "TKVM_SNAPSHOT_CREATE value")) return 1;
	if (require_true(TKVM_SNAPSHOT_OPEN_OR_CREATE == 3, "TKVM_SNAPSHOT_OPEN_OR_CREATE value")) return 1;

	printf("c_api_contract_smoke: OK\n");
	return 0;
}
