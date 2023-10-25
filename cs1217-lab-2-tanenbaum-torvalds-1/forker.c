#include "types.h"
#include "stat.h"
#include "user.h"

char *LUT[11] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };

// The forker program creates copies of its argument n times by forking, 
// and keeps track of the total time elapsed to complete the task.
// Each forked instance is also provided its index and the total number of instances,
// for batching tasks.
// Forker can also be used as a simple timer by running it with n = 1
int
main(int argc, char *argv[])
{
	if (argc < 3) 
	{
		printf(1, "usage: %s <executable> <count>\n", argv[0]);
		printf(1, "the index of the instance and the total count is passed as argv[1] and argv[2]\n");
		exit();
	}

  	int n = atoi(argv[2]);

	if (n < 1 || n > 10)
	{
		printf(1, "count must be at least 1 and at most 10\n");
		exit();
	}
	
	char *args[3];
	args[0] = argv[1];
	args[2] = LUT[n];

	uint startTimer = uptime();
	for (int i = 0; i < n; i++)
	{
		args[1] = LUT[i];
		int ret = fork();

		if (ret == 0)
		{
			exec(args[0], args);
		}
	}

	// wait for all children to exit
	while (wait() != -1);
	uint endTimer = uptime();
	printf(1, "forker started at %d, ended at %d, time elapsed %d\n", startTimer, endTimer, endTimer - startTimer);
	exit();
}
