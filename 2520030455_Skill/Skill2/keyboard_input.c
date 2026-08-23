#include <stdio.h>
#include <unistd.h>
#include <termios.h>

int main() {
    struct termios oldt, newt;
    char ch;
    char buffer[100];
    int index = 0;

    tcgetattr(STDIN_FILENO, &oldt);

    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);

    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    printf("Enter command: ");

    while (1) {
        ch = getchar();

        if (ch == '\n') {
            buffer[index] = '\0';
            printf("\nYou entered: %s\n", buffer);
            break;
        }

        if (ch == 127 || ch == '\b') {
            if (index > 0) {
                index--;
                printf("\b \b");
                fflush(stdout);
            }
        } else {
            if (index < 99) {
                buffer[index++] = ch;
                putchar(ch);
                fflush(stdout);
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return 0;
}