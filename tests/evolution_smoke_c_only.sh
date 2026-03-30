#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${ROOT_DIR}"

# Keep smoke tests non-interactive unless explicitly requested.
if [[ "${TKVM_ALLOW_DEBUG:-0}" != "1" ]]; then
	unset DEBUG GDB VMCALL FORK
fi

cmake -S . -B build
cmake --build build -j --target capi_smoke simplekvm_c tinytest_c

(
	cd guest/tests
	sh build.sh
)

./build/capi_smoke

simple_c_output="$(./build/simplekvm_c ./guest/tests/glibc_test "Hello World!")"
printf "%s\n" "${simple_c_output}"
if ! grep -q "Hello World!" <<<"${simple_c_output}"; then
	echo "simplekvm_c output missing greeting"
	exit 1
fi
if ! grep -q "Return value: 201527" <<<"${simple_c_output}"; then
	echo "simplekvm_c output missing expected return value 201527"
	exit 1
fi

tinytest_c_output="$(./build/tinytest_c ./guest/tests/glibc_test)"
printf "%s\n" "${tinytest_c_output}"
if ! grep -q "Nice! C API tests passed." <<<"${tinytest_c_output}"; then
	echo "tinytest_c did not report success"
	exit 1
fi

set +e
compute_output="$(./guest/tests/glibc_compute_test)"
compute_status=$?
set -e
printf "%s\n" "${compute_output}"
if [[ ${compute_status} -ne 170 ]]; then
	echo "Expected glibc_compute_test exit code 170, got ${compute_status}"
	exit 1
fi
if ! grep -q "checksum=0x" <<<"${compute_output}"; then
	echo "compute test output missing checksum"
	exit 1
fi
