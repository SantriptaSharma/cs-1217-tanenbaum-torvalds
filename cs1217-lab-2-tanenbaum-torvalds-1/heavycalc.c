#include "types.h"
#include "stat.h"
#include "user.h"

#define COUNT 5

int fib(int n)
{
	if (n <= 1) return 0;
	if (n == 2) return 1;
	return fib(n - 1) + fib (n - 2);
}

int
main(int argc, char *argv[])
{
	if (argc < 3) exit();

	int i = atoi(argv[1]);

	int results[COUNT];

	int first = i * COUNT + 1;

	printf(1, "instance %d starting at %d\n", i, uptime());
	
	for (int j = first; j <= first + COUNT - 1; j++)
	{
		results[j - first] = fib(j);
	}

	printf(1, "instance %d completed at %d\n", i, uptime());
	printf(1, "first: %d, last: %d\n", results[0], results[COUNT - 1]);
	exit();
}
