#define _GNU_SOURCE
#include "../include/debugger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <stdarg.h>

static void msg(char *out, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out, n, fmt, ap);
    va_end(ap);
}

static int wait_for_child(Debugger *dbg, int *status)
{
    if (waitpid(dbg->pid, status, 0) == -1)
        return -1;

    if (WIFEXITED(*status) || WIFSIGNALED(*status)) {
        dbg->running = 0;
        dbg->stopped = 0;
    } else if (WIFSTOPPED(*status)) {
        dbg->stopped = 1;
    }
    return 0;
}

static int read_word(Debugger *dbg, unsigned long long addr,
                     unsigned long long *value)
{
    errno = 0;
    unsigned long long v =
        (unsigned long long)ptrace(PTRACE_PEEKDATA, dbg->pid,
                                   (void *)(uintptr_t)addr, NULL);
    if (v == (unsigned long long)-1 && errno != 0)
        return -1;
    *value = v;
    return 0;
}

static int write_word(Debugger *dbg, unsigned long long addr,
                      unsigned long long value)
{
    if (ptrace(PTRACE_POKEDATA, dbg->pid,
               (void *)(uintptr_t)addr,
               (void *)(uintptr_t)value) == -1)
        return -1;
    return 0;
}

static int restore_breakpoint_instruction(Debugger *dbg)
{
    if (!dbg->breakpoint_set)
        return 0;

    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, dbg->pid, NULL, &regs) == -1)
        return -1;

    /* If stopped because INT3 was hit, RIP points one byte after INT3. */
    if (regs.rip == dbg->breakpoint_addr + 1) {
        if (write_word(dbg, dbg->breakpoint_addr, dbg->saved_word) == -1)
            return -1;

        regs.rip = dbg->breakpoint_addr;
        if (ptrace(PTRACE_SETREGS, dbg->pid, NULL, &regs) == -1)
            return -1;

        /* Execute the restored original instruction. */
        if (ptrace(PTRACE_SINGLESTEP, dbg->pid, NULL, NULL) == -1)
            return -1;

        int status;
        if (wait_for_child(dbg, &status) == -1)
            return -1;

        if (!dbg->running)
            return 0;

        /* Put INT3 back. */
        unsigned long long patched =
            (dbg->saved_word & ~0xffULL) | 0xccULL;

        if (write_word(dbg, dbg->breakpoint_addr, patched) == -1)
            return -1;
    }

    return 0;
}

void debugger_init(Debugger *dbg)
{
    memset(dbg, 0, sizeof(*dbg));
}

int debugger_launch(Debugger *dbg, const char *program,
                    char *message, size_t message_size)
{
    if (dbg->running) {
        msg(message, message_size, "A program is already running.");
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        msg(message, message_size, "fork() failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("PTRACE_TRACEME");
            _exit(1);
        }

        execl(program, program, (char *)NULL);
        perror("exec");
        _exit(1);
    }

    dbg->pid = pid;
    dbg->running = 1;
    dbg->stopped = 0;
    dbg->breakpoint_set = 0;

    int status;
    if (wait_for_child(dbg, &status) == -1) {
        msg(message, message_size, "waitpid() failed: %s", strerror(errno));
        return -1;
    }

    if (WIFSTOPPED(status)) {
        msg(message, message_size,
            "Program launched.\nPID: %d\nStatus: stopped and ready for debugging.",
            pid);
        return 0;
    }

    msg(message, message_size, "Program ended during launch.");
    return -1;
}

int debugger_set_breakpoint(Debugger *dbg, unsigned long long addr,
                            char *message, size_t message_size)
{
    if (!dbg->running || !dbg->stopped) {
        msg(message, message_size,
            "Launch the program first. It must be stopped.");
        return -1;
    }

    if (dbg->breakpoint_set) {
        msg(message, message_size,
            "This demo supports one breakpoint at a time.");
        return -1;
    }

    unsigned long long original;
    if (read_word(dbg, addr, &original) == -1) {
        msg(message, message_size,
            "Cannot read address 0x%llx: %s",
            addr, strerror(errno));
        return -1;
    }

    unsigned long long patched =
        (original & ~0xffULL) | 0xccULL;

    if (write_word(dbg, addr, patched) == -1) {
        msg(message, message_size,
            "Cannot write breakpoint: %s", strerror(errno));
        return -1;
    }

    dbg->breakpoint_addr = addr;
    dbg->saved_word = original;
    dbg->breakpoint_set = 1;

    msg(message, message_size,
        "Breakpoint set at 0x%llx\nOriginal instruction byte saved.",
        addr);
    return 0;
}

int debugger_continue(Debugger *dbg, char *message, size_t message_size)
{
    if (!dbg->running || !dbg->stopped) {
        msg(message, message_size, "No stopped program to continue.");
        return -1;
    }

    if (restore_breakpoint_instruction(dbg) == -1) {
        msg(message, message_size,
            "Could not prepare breakpoint: %s", strerror(errno));
        return -1;
    }

    if (!dbg->running) {
        msg(message, message_size, "Program exited.");
        return 0;
    }

    if (ptrace(PTRACE_CONT, dbg->pid, NULL, NULL) == -1) {
        msg(message, message_size,
            "ptrace(PTRACE_CONT) failed: %s", strerror(errno));
        return -1;
    }

    int status;
    if (wait_for_child(dbg, &status) == -1) {
        msg(message, message_size, "waitpid() failed: %s", strerror(errno));
        return -1;
    }

    if (WIFEXITED(status)) {
        msg(message, message_size,
            "Program exited normally.\nExit code: %d",
            WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        msg(message, message_size,
            "Program terminated by signal %d.", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status);

        if (sig == SIGTRAP && dbg->breakpoint_set) {
            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, dbg->pid, NULL, &regs) == 0 &&
                regs.rip == dbg->breakpoint_addr + 1) {
                /* Keep INT3 installed after reporting the breakpoint. */
                unsigned long long patched =
                    (dbg->saved_word & ~0xffULL) | 0xccULL;
                write_word(dbg, dbg->breakpoint_addr, patched);

                msg(message, message_size,
                    "Breakpoint hit at 0x%llx.\nProgram is stopped.",
                    dbg->breakpoint_addr);
            } else {
                msg(message, message_size,
                    "Program stopped by SIGTRAP.");
            }
        } else {
            msg(message, message_size,
                "Program stopped by signal %d.", sig);
        }
    }

    return 0;
}

int debugger_step(Debugger *dbg, char *message, size_t message_size)
{
    if (!dbg->running || !dbg->stopped) {
        msg(message, message_size, "No stopped program to step.");
        return -1;
    }

    if (restore_breakpoint_instruction(dbg) == -1) {
        msg(message, message_size,
            "Could not prepare breakpoint: %s", strerror(errno));
        return -1;
    }

    if (!dbg->running) {
        msg(message, message_size, "Program exited.");
        return 0;
    }

    if (ptrace(PTRACE_SINGLESTEP, dbg->pid, NULL, NULL) == -1) {
        msg(message, message_size,
            "ptrace(PTRACE_SINGLESTEP) failed: %s", strerror(errno));
        return -1;
    }

    int status;
    if (wait_for_child(dbg, &status) == -1) {
        msg(message, message_size, "waitpid() failed: %s", strerror(errno));
        return -1;
    }

    if (dbg->running && dbg->breakpoint_set) {
        unsigned long long patched =
            (dbg->saved_word & ~0xffULL) | 0xccULL;
        write_word(dbg, dbg->breakpoint_addr, patched);
    }

    if (WIFSTOPPED(status)) {
        msg(message, message_size,
            "One instruction executed.\nProgram is stopped.");
    } else if (WIFEXITED(status)) {
        msg(message, message_size,
            "Program exited with code %d.", WEXITSTATUS(status));
    } else {
        msg(message, message_size, "Single-step completed.");
    }

    return 0;
}

int debugger_stop(Debugger *dbg, char *message, size_t message_size)
{
    if (!dbg->running) {
        msg(message, message_size, "No program is running.");
        return -1;
    }

    if (kill(dbg->pid, SIGKILL) == -1) {
        msg(message, message_size,
            "kill() failed: %s", strerror(errno));
        return -1;
    }

    waitpid(dbg->pid, NULL, 0);

    dbg->running = 0;
    dbg->stopped = 0;
    dbg->breakpoint_set = 0;

    msg(message, message_size, "Program stopped and process terminated.");
    return 0;
}

int debugger_get_registers(Debugger *dbg, char *message, size_t message_size)
{
    if (!dbg->running || !dbg->stopped) {
        msg(message, message_size,
            "Program must be running and stopped.");
        return -1;
    }

#if defined(__x86_64__)
    struct user_regs_struct r;

    if (ptrace(PTRACE_GETREGS, dbg->pid, NULL, &r) == -1) {
        msg(message, message_size,
            "PTRACE_GETREGS failed: %s", strerror(errno));
        return -1;
    }

    msg(message, message_size,
        "REGISTERS\n"
        "RAX = 0x%llx\n"
        "RBX = 0x%llx\n"
        "RCX = 0x%llx\n"
        "RDX = 0x%llx\n"
        "RSI = 0x%llx\n"
        "RDI = 0x%llx\n"
        "RSP = 0x%llx\n"
        "RBP = 0x%llx\n"
        "RIP = 0x%llx\n"
        "EFLAGS = 0x%llx",
        (unsigned long long)r.rax,
        (unsigned long long)r.rbx,
        (unsigned long long)r.rcx,
        (unsigned long long)r.rdx,
        (unsigned long long)r.rsi,
        (unsigned long long)r.rdi,
        (unsigned long long)r.rsp,
        (unsigned long long)r.rbp,
        (unsigned long long)r.rip,
        (unsigned long long)r.eflags);

    return 0;
#else
    msg(message, message_size,
        "Register display in this project is implemented for x86-64 Linux.");
    return -1;
#endif
}

int debugger_read_memory(Debugger *dbg, unsigned long long addr,
                         char *message, size_t message_size)
{
    if (!dbg->running || !dbg->stopped) {
        msg(message, message_size,
            "Program must be running and stopped.");
        return -1;
    }

    unsigned long long value;

    if (read_word(dbg, addr, &value) == -1) {
        msg(message, message_size,
            "Memory read failed at 0x%llx: %s",
            addr, strerror(errno));
        return -1;
    }

    msg(message, message_size,
        "MEMORY\nAddress: 0x%llx\nValue:   0x%llx",
        addr, value);

    return 0;
}
