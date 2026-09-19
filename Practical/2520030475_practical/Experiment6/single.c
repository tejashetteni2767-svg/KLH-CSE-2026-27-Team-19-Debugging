#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void signal_handler(int sig) {
    if (sig == SIGINT)
        printf("\nSIGINT received: Ctrl+C pressed\n");
    else if (sig == SIGTERM)
        printf("\nSIGTERM received\n");
    else if (sig == SIGUSR1)
        printf("\nSIGUSR1 received\n");
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGUSR1, signal_handler);

    printf("Process ID: %d\n", getpid());
    printf("Waiting for signals...\n");

    while (1) {
        pause();
    }

    return 0;
}
