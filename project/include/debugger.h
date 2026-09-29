#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <sys/types.h>
#include <stdint.h>

typedef struct {
    pid_t pid;
    int running;
    int stopped;
    int breakpoint_set;
    unsigned long long breakpoint_addr;
    unsigned long long saved_word;
} Debugger;

void debugger_init(Debugger *dbg);
int debugger_launch(Debugger *dbg, const char *program, char *message, size_t message_size);
int debugger_set_breakpoint(Debugger *dbg, unsigned long long addr,
                             char *message, size_t message_size);
int debugger_continue(Debugger *dbg, char *message, size_t message_size);
int debugger_step(Debugger *dbg, char *message, size_t message_size);
int debugger_stop(Debugger *dbg, char *message, size_t message_size);
int debugger_get_registers(Debugger *dbg, char *message, size_t message_size);
int debugger_read_memory(Debugger *dbg, unsigned long long addr,
                         char *message, size_t message_size);

#endif
