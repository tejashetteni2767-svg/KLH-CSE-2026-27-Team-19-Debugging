#include "../include/debugger.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <program>\n", argv[0]);
        return 1;
    }

    Debugger dbg;
    debugger_init(&dbg);

    char message[2048];

    if (debugger_launch(&dbg, argv[1],
                         message, sizeof(message)) == -1) {
        puts(message);
        return 1;
    }

    puts(message);

    while (dbg.running) {
        printf("\n(debugger) ");
        fflush(stdout);

        char command[100];

        if (!fgets(command, sizeof(command), stdin))
            break;

        if (command[0] == 'c') {
            debugger_continue(&dbg, message, sizeof(message));
        } else if (command[0] == 's') {
            debugger_step(&dbg, message, sizeof(message));
        } else if (command[0] == 'r') {
            debugger_get_registers(&dbg, message, sizeof(message));
        } else if (command[0] == 'x') {
            debugger_stop(&dbg, message, sizeof(message));
        } else if (command[0] == 'q') {
            if (dbg.running)
                debugger_stop(&dbg, message, sizeof(message));
            break;
        } else {
            snprintf(message, sizeof(message),
                     "Commands: c=continue, s=step, r=registers, x=stop, q=quit");
        }

        puts(message);
    }

    return 0;
}
