#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ctype.h>
#include <time.h>

#define PROC_PATH "/proc"
#define LOG_FILE "process_log.txt"

/* Check whether a string contains only digits */
int is_number(const char *str) {
    if (str == NULL || *str == '\0')
        return 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit((unsigned char)str[i]))
            return 0;
    }

    return 1;
}

/* Write an event to log file */
void log_event(const char *message) {
    FILE *file = fopen(LOG_FILE, "a");

    if (file == NULL) {
        perror("Unable to open log file");
        return;
    }

    time_t now = time(NULL);
    char *time_string = ctime(&now);

    if (time_string != NULL) {
        time_string[strcspn(time_string, "\n")] = '\0';
        fprintf(file, "[%s] %s\n", time_string, message);
    }

    fclose(file);
}

/* Get process name */
void get_process_name(int pid, char *name, int size) {
    char path[100];
    FILE *file;

    snprintf(path, sizeof(path), "/proc/%d/comm", pid);

    file = fopen(path, "r");

    if (file == NULL) {
        strcpy(name, "Unknown");
        return;
    }

    if (fgets(name, size, file) == NULL) {
        strcpy(name, "Unknown");
    }

    name[strcspn(name, "\n")] = '\0';

    fclose(file);
}

/* Get process state */
char get_process_state(int pid) {
    char path[100];
    FILE *file;
    char line[1024];
    char state = '?';

    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    file = fopen(path, "r");

    if (file == NULL)
        return '?';

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "State:", 6) == 0) {
            sscanf(line, "State:\t%c", &state);
            break;
        }
    }

    fclose(file);

    return state;
}

/* Convert process state character to readable text */
const char *state_name(char state) {
    switch (state) {
        case 'R':
            return "Running";
        case 'S':
            return "Sleeping";
        case 'D':
            return "Waiting";
        case 'T':
            return "Stopped";
        case 'Z':
            return "Zombie";
        case 'I':
            return "Idle";
        default:
            return "Unknown";
    }
}

/* Get parent PID */
int get_parent_pid(int pid) {
    char path[100];
    FILE *file;
    char line[1024];
    int ppid = -1;

    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    file = fopen(path, "r");

    if (file == NULL)
        return -1;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "PPid:", 5) == 0) {
            sscanf(line, "PPid:\t%d", &ppid);
            break;
        }
    }

    fclose(file);

    return ppid;
}

/* Get memory usage in KB */
long get_memory_usage(int pid) {
    char path[100];
    FILE *file;
    char line[1024];
    long memory = 0;

    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    file = fopen(path, "r");

    if (file == NULL)
        return -1;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %ld", &memory);
            break;
        }
    }

    fclose(file);

    return memory;
}

/* Get process CPU time from /proc/[pid]/stat */
unsigned long get_process_cpu_time(int pid) {
    char path[100];
    FILE *file;
    char buffer[4096];

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    file = fopen(path, "r");

    if (file == NULL)
        return 0;

    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fclose(file);
        return 0;
    }

    fclose(file);

    /*
     * Fields 14 and 15 are utime and stime.
     * Process name may contain spaces, so locate the last ')'.
     */
    char *right_paren = strrchr(buffer, ')');

    if (right_paren == NULL)
        return 0;

    char *ptr = right_paren + 2;

    int field = 3;
    unsigned long utime = 0;
    unsigned long stime = 0;

    char *token = strtok(ptr, " ");

    while (token != NULL) {
        if (field == 14)
            utime = strtoul(token, NULL, 10);

        if (field == 15) {
            stime = strtoul(token, NULL, 10);
            break;
        }

        field++;
        token = strtok(NULL, " ");
    }

    return utime + stime;
}

/* Get total system CPU time */
unsigned long long get_total_cpu_time() {
    FILE *file;
    char line[1024];
    unsigned long long user, nice, system, idle;
    unsigned long long iowait, irq, softirq, steal;

    file = fopen("/proc/stat", "r");

    if (file == NULL)
        return 0;

    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    fclose(file);

    sscanf(line,
           "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
           &user,
           &nice,
           &system,
           &idle,
           &iowait,
           &irq,
           &softirq,
           &steal);

    return user + nice + system + idle +
           iowait + irq + softirq + steal;
}

/* Display all processes */
void list_processes() {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(PROC_PATH);

    if (dir == NULL) {
        perror("Unable to open /proc");
        return;
    }

    printf("\n%-8s %-8s %-15s %-12s %-12s\n",
           "PID", "PPID", "NAME", "STATE", "MEM(KB)");

    printf("---------------------------------------------------------------\n");

    while ((entry = readdir(dir)) != NULL) {

        if (!is_number(entry->d_name))
            continue;

        int pid = atoi(entry->d_name);

        char name[256];
        get_process_name(pid, name, sizeof(name));

        int ppid = get_parent_pid(pid);
        char state = get_process_state(pid);
        long memory = get_memory_usage(pid);

        if (memory < 0)
            continue;

        printf("%-8d %-8d %-15s %-12s %-12ld\n",
               pid,
               ppid,
               name,
               state_name(state),
               memory);
    }

    closedir(dir);
}

/* Display detailed information */
void process_details(int pid) {
    char name[256];

    get_process_name(pid, name, sizeof(name));

    int ppid = get_parent_pid(pid);
    char state = get_process_state(pid);
    long memory = get_memory_usage(pid);

    int priority = getpriority(PRIO_PROCESS, pid);

    if (memory < 0) {
        printf("\nProcess %d does not exist or cannot be accessed.\n", pid);
        return;
    }

    printf("\n========================================\n");
    printf("        PROCESS INFORMATION\n");
    printf("========================================\n");

    printf("PID              : %d\n", pid);
    printf("Parent PID       : %d\n", ppid);
    printf("Process Name     : %s\n", name);
    printf("Process State    : %s\n", state_name(state));
    printf("Memory Usage     : %ld KB\n", memory);
    printf("Priority         : %d\n", priority);

    printf("========================================\n");
}

/* Search processes by name */
void search_process() {
    char search_name[100];

    printf("Enter process name: ");
    scanf("%99s", search_name);

    DIR *dir = opendir(PROC_PATH);

    if (dir == NULL) {
        perror("Unable to open /proc");
        return;
    }

    int found = 0;

    printf("\n%-8s %-20s %-15s\n",
           "PID", "NAME", "STATE");

    printf("-----------------------------------------------\n");

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (!is_number(entry->d_name))
            continue;

        int pid = atoi(entry->d_name);

        char name[256];

        get_process_name(pid, name, sizeof(name));

        if (strstr(name, search_name) != NULL) {

            char state = get_process_state(pid);

            printf("%-8d %-20s %-15s\n",
                   pid,
                   name,
                   state_name(state));

            found = 1;
        }
    }

    closedir(dir);

    if (!found)
        printf("No matching process found.\n");
}

/* Send signal to process */
void control_process(int signal_number, const char *action) {
    int pid;

    printf("Enter PID: ");
    scanf("%d", &pid);

    if (kill(pid, signal_number) == 0) {

        printf("Process %d: %s successful.\n", pid, action);

        char log_message[200];

        snprintf(log_message,
                 sizeof(log_message),
                 "PID %d - %s",
                 pid,
                 action);

        log_event(log_message);

    } else {
        perror("Operation failed");
    }
}

/* Change process priority */
void change_priority() {
    int pid;
    int priority;

    printf("Enter PID: ");
    scanf("%d", &pid);

    printf("Enter new priority (-20 to 19): ");
    scanf("%d", &priority);

    if (priority < -20 || priority > 19) {
        printf("Invalid priority. Use -20 to 19.\n");
        return;
    }

    if (setpriority(PRIO_PROCESS, pid, priority) == 0) {

        printf("Priority changed successfully.\n");

        char log_message[200];

        snprintf(log_message,
                 sizeof(log_message),
                 "PID %d - Priority changed to %d",
                 pid,
                 priority);

        log_event(log_message);

    } else {
        perror("Unable to change priority");
    }
}

/* Monitor CPU usage */
void monitor_cpu() {
    int pid;

    printf("Enter PID: ");
    scanf("%d", &pid);

    printf("\nMonitoring CPU usage for PID %d...\n", pid);
    printf("Press Ctrl+C to stop.\n\n");

    unsigned long process_start = get_process_cpu_time(pid);
    unsigned long long total_start = get_total_cpu_time();

    sleep(1);

    unsigned long process_end = get_process_cpu_time(pid);
    unsigned long long total_end = get_total_cpu_time();

    if (process_start == 0 || process_end == 0) {
        printf("Unable to read process CPU information.\n");
        return;
    }

    unsigned long process_difference =
        process_end - process_start;

    unsigned long long total_difference =
        total_end - total_start;

    if (total_difference == 0) {
        printf("Unable to calculate CPU usage.\n");
        return;
    }

    double cpu_usage =
        ((double)process_difference /
         (double)total_difference) * 100.0;

    printf("CPU Usage: %.2f%%\n", cpu_usage);
}

/* Create a child process and execute a command */
void create_process() {
    char command[100];

    printf("Enter command to execute: ");
    scanf("%99s", command);

    pid_t pid = fork();

    if (pid < 0) {

        perror("fork failed");

    } else if (pid == 0) {

        /* Child process */
        printf("Child process created.\n");
        printf("Child PID: %d\n", getpid());

        execlp(command, command, NULL);

        perror("exec failed");
        exit(EXIT_FAILURE);

    } else {

        /* Parent process */
        printf("Parent PID: %d\n", getpid());
        printf("Created Child PID: %d\n", pid);

        waitpid(pid, NULL, 0);

        printf("Child process completed.\n");

        char log_message[200];

        snprintf(log_message,
                 sizeof(log_message),
                 "Created and executed child process PID %d",
                 pid);

        log_event(log_message);
    }
}

/* Display system memory */
void system_memory() {
    FILE *file = fopen("/proc/meminfo", "r");

    if (file == NULL) {
        perror("Unable to open /proc/meminfo");
        return;
    }

    char line[256];

    long total_memory = 0;
    long free_memory = 0;
    long available_memory = 0;

    while (fgets(line, sizeof(line), file)) {

        if (sscanf(line,
                   "MemTotal: %ld kB",
                   &total_memory) == 1)
            continue;

        if (sscanf(line,
                   "MemFree: %ld kB",
                   &free_memory) == 1)
            continue;

        if (sscanf(line,
                   "MemAvailable: %ld kB",
                   &available_memory) == 1)
            continue;
    }

    fclose(file);

    printf("\n========================================\n");
    printf("       SYSTEM MEMORY INFORMATION\n");
    printf("========================================\n");

    printf("Total Memory     : %ld KB\n", total_memory);
    printf("Free Memory      : %ld KB\n", free_memory);
    printf("Available Memory : %ld KB\n", available_memory);

    printf("========================================\n");
}

/* Main menu */
int main() {

    int choice;

    while (1) {

        printf("\n\n");
        printf("========================================\n");
        printf(" LINUX PROCESS MONITORING AND CONTROL\n");
        printf("========================================\n");

        printf("1.  List All Processes\n");
        printf("2.  Search Process\n");
        printf("3.  View Process Details\n");
        printf("4.  Monitor CPU Usage\n");
        printf("5.  Monitor System Memory\n");
        printf("6.  Terminate Process (SIGTERM)\n");
        printf("7.  Force Kill Process (SIGKILL)\n");
        printf("8.  Stop Process (SIGSTOP)\n");
        printf("9.  Resume Process (SIGCONT)\n");
        printf("10. Change Process Priority\n");
        printf("11. Create and Execute Process\n");
        printf("12. Exit\n");

        printf("========================================\n");
        printf("Enter your choice: ");

        scanf("%d", &choice);

        switch (choice) {

            case 1:
                list_processes();
                break;

            case 2:
                search_process();
                break;

            case 3: {
                int pid;

                printf("Enter PID: ");
                scanf("%d", &pid);

                process_details(pid);
                break;
            }

            case 4:
                monitor_cpu();
                break;

            case 5:
                system_memory();
                break;

            case 6:
                control_process(SIGTERM,
                                "Process termination");
                break;

            case 7:
                control_process(SIGKILL,
                                "Force kill");
                break;

            case 8:
                control_process(SIGSTOP,
                                "Process stopped");
                break;

            case 9:
                control_process(SIGCONT,
                                "Process resumed");
                break;

            case 10:
                change_priority();
                break;

            case 11:
                create_process();
                break;

            case 12:
                printf("\nExiting program...\n");
                return 0;

            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    return 0;
}
