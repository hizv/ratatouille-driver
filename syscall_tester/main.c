#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

#define FIXED_SCALE 1000000

int main(void)
{
	unsigned long long ret;
    float result;

	ret = syscall (451, 3, 5);
    result = (float)ret / FIXED_SCALE;

	printf ("Value: %f\n", result);
}
