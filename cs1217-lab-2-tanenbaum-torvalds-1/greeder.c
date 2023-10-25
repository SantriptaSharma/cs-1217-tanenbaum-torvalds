#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	int i = 0;

	if (argc < 3) exit();

	printf(1, "running greeder instance %s/%s at %d\n", argv[1], argv[2], uptime());

	while (1)
	{
		i += 50;
	}
	
	exit();
}
