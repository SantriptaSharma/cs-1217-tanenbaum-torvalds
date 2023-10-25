#include "types.h"
#include "user.h"
#include "benchinfo.h"

#define PROCS 40

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
            cpu_bound(i);
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

