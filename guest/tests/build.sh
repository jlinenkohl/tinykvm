set -e

if command -v musl-gcc >/dev/null 2>&1; then
	musl-gcc -static -O0 -ggdb3 test.c -o musl_test
	musl-gcc -static -O2 -ggdb3 compute_test.c -o musl_compute_test
fi

gcc -static -O0 -ggdb3 test.c -o glibc_test
gcc -static -O2 -ggdb3 compute_test.c -o glibc_compute_test
