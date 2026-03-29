#include <stdint.h>
#include <stdio.h>

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

int main(void)
{
	const uint64_t rounds = 250000;
	const uint64_t checksum = compute_checksum(rounds);
	printf("rounds=%llu checksum=0x%llx\n",
		(unsigned long long)rounds,
		(unsigned long long)checksum);
	return 0xAA;
}
