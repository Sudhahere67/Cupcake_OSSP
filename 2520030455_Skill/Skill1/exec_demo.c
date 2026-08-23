#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Executing the ls command...\n");

    execlp("ls", "ls", "-l", NULL);

    perror("exec failed");

    return 1;
}