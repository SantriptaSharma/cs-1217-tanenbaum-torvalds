#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "benchinfo.h"

#define PROCS 45

int fib(int n)
{
	if (n <= 1) return 0;
	if (n == 2) return 1;
	return fib(n - 1) + fib (n - 2);
}

void heavy_calc(int id) {
    // uint start = uptime(), end;
    // printf(1, "%d: started at %d\n", id, start);

	/*int ans = */
    fib(id - 1);

    // end = uptime();
    // printf(1, "%d: ended at %d, elapsed %d, calc'd %d\n", id, end, end - start, ans);

    exit();
}

void cpu_bound(int id) {
    int i;

    // uint start = uptime(), end;
    // printf(1, "%d: started at %d\n", id, start);

    for (i = 0; i < 1; i++) {
          i = i * 2;
    }

    // end = uptime();
    // printf(1, "%d: ended at %d, elapsed %d\n", id, end, end - start);

    exit();
}

void io_bound(int id) {
    int i, fd;
    char buf[512];

    // uint start = uptime(), end;
    // printf(1, "%d: started at %d\n", id, start);

    fd = open("tempfile", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(2, "Error opening tempfile\n");
        exit();
    }

    for (i = 0; i < 30; i++) {
        memset(buf, 'A' + i, sizeof(buf));
        write(fd, buf, sizeof(buf));
        sleep(1);
    }

    close(fd);
    
    // end = uptime();

    // printf(1, "%d: ended at %d, elapsed %d\n", id, end, end - start);

    exit();
}

int main() {
    int i;
    uint start = uptime(), end;

    printf(1, "task starting: %d\n");

    for (i = 0; i < PROCS; i++) {
        int rc = fork();
        if (rc < 0) {
            printf(2, "Fork failed");
            exit();
        }

        if (rc == 0) {
            uint selector = i % 3;

			switch (selector)
			{
				case 0:
					io_bound(i);
				break;

				case 1:
					cpu_bound(i);
				break;
				
				case 2:
					heavy_calc(i);
				break;
			}
        }
    }
        
    while (wait() != -1);

    end = uptime();
    printf(1, "all tasks ended: %d, elapsed %d\n", end, end - start);
    printf(1,"");

    struct benchinfo b;
    if (benchinfo(&b) < 0) exit();

    printf(1, "children: %d, child ticks: %d\n", b.children, b.childticks);


    exit();
}