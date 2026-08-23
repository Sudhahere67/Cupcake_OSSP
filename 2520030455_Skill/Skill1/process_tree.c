#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    pid_t child1, child2;

    printf("Main Parent PID: %d\n", getpid());

    child1 = fork();

    if (child1 == 0) {
        printf("Child 1: PID = %d, PPID = %d\n",
               getpid(), getppid());
        sleep(5);
        return 0;
    }

    child2 = fork();

    if (child2 == 0) {
        printf("Child 2: PID = %d, PPID = %d\n",
               getpid(), getppid());
        sleep(5);
        return 0;
    }

    printf("Parent created Child 1 PID: %d\n", child1);
    printf("Parent created Child 2 PID: %d\n", child2);

    wait(NULL);
    wait(NULL);

    printf("All child processes completed.\n");

    return 0;
}