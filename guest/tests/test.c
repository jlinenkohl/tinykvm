#include <assert.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static long nprimes = 0;

static uint64_t compute_checksum(uint64_t rounds)
{
	uint64_t acc = 0x243f6a8885a308d3ULL;
	uint64_t state = 0x9e3779b97f4a7c15ULL;

	for (uint64_t i = 0; i < rounds; i++)
	{
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;

		acc ^= state + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2);
		acc = (acc << 9) | (acc >> (64 - 9));
		acc += i * 0x100000001b3ULL;
	}
	return acc & 0x7FFFFFFFFFFFFFFFULL;
}

int main(int argc, char** argv)
{
	char* test = (char *)malloc(14);
	strcpy(test, argv[1]);
	printf("%.*s\n", 13, test);
	free(test);

	static const int N = 1000000;
	char prime[N];
	memset(prime, 1, sizeof(prime));
	for (long n = 2; n < N; n++)
	{
		if (prime[n]) {
			nprimes += 1;
			for (long i = n*n; i < N; i += n)
				prime[i] = 0;
		}
	}
	return 0x31337;
}

__attribute__((used))
int test_return()
{
	return 0x31337;
}

__attribute__((used))
void test_ud2()
{
	asm("ud2");
}

__attribute__((used))
int test_read()
{
	assert(nprimes == 78498);
	return 200;
}

static int t = 0;

__attribute__((used))
void test_write()
{
	asm("" ::: "memory");
	assert(t == 0);
	asm("" ::: "memory");
	t = 1;
	asm("" ::: "memory");
	assert(t == 1);
}

static int cow = 0;

__attribute__((used))
int test_copy_on_write()
{
	assert(cow == 0);
	cow = 1;
	return 0x31337;
}

__attribute__((used))
long test_syscall()
{
	register long status asm("rdi") = 555;
	long ret = 60;
	asm("syscall" : "+a"(ret) : "r"(status) : "rcx", "r11", "memory");
	return ret;
}

__attribute__((used))
long test_malloc()
{

	int* p = (int *)malloc(4);

	return (uintptr_t) p;
}


__attribute__((used))
int write_value(int value)
{
	cow = value;
	return value;
}
__attribute__((used))
int test_is_value(int value)
{
	assert(cow == value);
	return 0x31337;
}

__attribute__((used))
int test_loop()
{
	while(1);
}

asm(".global vcpuid\n"
	".type vcpuid, @function\n"
	"vcpuid:\n"
	"	mov %gs:(0x0), %eax\n"
	"   ret\n");
extern int vcpuid();

__attribute__((used))
int test_vcpu()
{
	return vcpuid();
}

__attribute__((used))
long test_compute_checksum(long rounds)
{
	if (rounds <= 0) {
		rounds = 1;
	}
	return (long)compute_checksum((uint64_t)rounds);
}
