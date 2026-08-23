#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    char command[100];

    printf("Enter Linux command: ");
    scanf("%99s", command);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        printf("Child PID: %d\n", getpid());
        printf("Executing command...\n");

        execlp(command, command, (char *)NULL);

        perror("exec failed");
        exit(1);
    } 
    else {
        printf("Parent PID: %d\n", getpid());
        printf("Child PID: %d\n", pid);

        wait(NULL);

        printf("Child process completed.\n");
    }

    return 0;
}