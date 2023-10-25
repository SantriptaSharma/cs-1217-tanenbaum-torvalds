#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "benchinfo.h"

#define PROCS 40

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

    printf(1, "task starting: %d\n", start);

    for (i = 0; i < PROCS; i++) {
        int rc = fork();
        if (rc < 0) {
            printf(2, "Fork failed\n");
            exit();
        }

        if (rc == 0) {
            io_bound(i);
        }
    }

    while (wait() != -1);

    end = uptime();
    printf(1, "all tasks ended: %d, elapsed %d\n", end, end - start);

    struct benchinfo b;
    if (benchinfo(&b) < 0) exit();

    printf(1, "children: %d, child ticks: %d\n", b.children, b.childticks);

    exit();
}

