#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/syscalls.h>

#define FIXED_SCALE 1000000

SYSCALL_DEFINE2(float_add, unsigned long long, num1, unsigned long long, num2)
{
	unsigned long long fixed_num1, fixed_num2;

	fixed_num1 = (unsigned long long) num1 * FIXED_SCALE;
	fixed_num2 = (unsigned long long) num2 * FIXED_SCALE;

	return fixed_num1 + fixed_num2;
}
