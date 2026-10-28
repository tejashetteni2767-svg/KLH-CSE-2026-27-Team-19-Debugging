/**
 * ============================================================================
 * Project: Linux Signal-Based Process Controller (OSSP Hackathon)
 * File: src/proc_controller.c
 * Description: High-performance, robust POSIX C command-line process-control
 *              utility that manages Linux processes using signals (SIGSTOP,
 *              SIGCONT, SIGTERM, SIGKILL, SIGHUP), inspects kernel procfs,
 *              and provides both human-readable and JSON telemetry.
 * ============================================================================
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#define PROC_CTRL_VERSION "2.4.0"

// ANSI Color Palette for Terminal Output
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"
#define COLOR_DIM     "\033[2m"

// Process Information Structure parsed from /proc/[pid]
typedef struct {
    pid_t pid;
    pid_t ppid;
    char name[128];
    char state;
    char state_str[32];
    unsigned long long utime;
    unsigned long long stime;
    unsigned long vm_size_kb;
    unsigned long vm_rss_kb;
    int num_threads;
    unsigned long long sig_blk;
    unsigned long long sig_ign;
    unsigned long long sig_cgt;
    unsigned long long sig_pnd;
    char cmdline[512];
    bool exists;
} proc_info_t;

// Signal Table mapping numbers to POSIX names and descriptions
typedef struct {
    int signo;
    const char *name;
    const char *desc;
    bool can_catch;
    bool can_block;
} signal_entry_t;

static const signal_entry_t SIGNAL_TABLE[] = {
    { 1,  "SIGHUP",    "Hangup / Configuration Reload",               true,  true  },
    { 2,  "SIGINT",    "Terminal Interrupt (Ctrl+C)",                 true,  true  },
    { 3,  "SIGQUIT",   "Terminal Quit (Core Dump)",                   true,  true  },
    { 4,  "SIGILL",    "Illegal Instruction",                         true,  true  },
    { 5,  "SIGTRAP",   "Trace / Breakpoint Trap",                     true,  true  },
    { 6,  "SIGABRT",   "Process Abort",                               true,  true  },
    { 9,  "SIGKILL",   "Uncatchable Kill (Forceful Eviction)",        false, false },
    { 10, "SIGUSR1",   "User-Defined Signal 1",                       true,  true  },
    { 11, "SIGSEGV",   "Invalid Memory Reference (Segfault)",         true,  true  },
    { 12, "SIGUSR2",   "User-Defined Signal 2",                       true,  true  },
    { 13, "SIGPIPE",   "Broken Pipe write without reader",            true,  true  },
    { 14, "SIGALRM",   "Timer Alarm",                                 true,  true  },
    { 15, "SIGTERM",   "Graceful Termination Request",                true,  true  },
    { 17, "SIGCHLD",   "Child Process Stopped or Terminated",         true,  true  },
    { 18, "SIGCONT",   "Resume / Continue Execution",                 true,  true  },
    { 19, "SIGSTOP",   "Uncatchable Stop / Pause",                    false, false },
    { 20, "SIGTSTP",   "Terminal Stop (Ctrl+Z)",                      true,  true  },
    { 0,  NULL,        NULL,                                          false, false }
};

// Check if a process exists using kill(pid, 0)
static bool process_exists(pid_t pid) {
    if (pid <= 0) return false;
    if (kill(pid, 0) == 0) return true;
    return (errno == EPERM); // Process exists but we might lack root privileges
}

// Map process state character to descriptive string
static void decode_process_state(char state, char *dest, size_t max_len) {
    switch (state) {
        case 'R': snprintf(dest, max_len, "RUNNING (Active)"); break;
        case 'S': snprintf(dest, max_len, "SLEEPING (Interruptible)"); break;
        case 'D': snprintf(dest, max_len, "DISK SLEEP (Uninterruptible)"); break;
        case 'T': snprintf(dest, max_len, "STOPPED (Paused by Signal)"); break;
        case 't': snprintf(dest, max_len, "TRACED (Debugger Stop)"); break;
        case 'Z': snprintf(dest, max_len, "ZOMBIE (Terminated, Unreaped)"); break;
        case 'X': snprintf(dest, max_len, "DEAD (Terminating)"); break;
        default:  snprintf(dest, max_len, "UNKNOWN (%c)", state); break;
    }
}

// Read and parse /proc/[pid]/status, stat, cmdline
static bool read_process_info(pid_t pid, proc_info_t *info) {
    memset(info, 0, sizeof(proc_info_t));
    info->pid = pid;

    if (!process_exists(pid)) {
        info->exists = false;
        return false;
    }
    info->exists = true;

    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Name:", 5) == 0) {
            sscanf(line + 5, " %127s", info->name);
        } else if (strncmp(line, "State:", 6) == 0) {
            sscanf(line + 6, " %c", &info->state);
            decode_process_state(info->state, info->state_str, sizeof(info->state_str));
        } else if (strncmp(line, "PPid:", 5) == 0) {
            sscanf(line + 5, " %d", &info->ppid);
        } else if (strncmp(line, "Threads:", 8) == 0) {
            sscanf(line + 8, " %d", &info->num_threads);
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, " %lu", &info->vm_size_kb);
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, " %lu", &info->vm_rss_kb);
        } else if (strncmp(line, "SigBlk:", 7) == 0) {
            sscanf(line + 7, " %llx", &info->sig_blk);
        } else if (strncmp(line, "SigIgn:", 7) == 0) {
            sscanf(line + 7, " %llx", &info->sig_ign);
        } else if (strncmp(line, "SigCgt:", 7) == 0) {
            sscanf(line + 7, " %llx", &info->sig_cgt);
        } else if (strncmp(line, "SigPnd:", 7) == 0) {
            sscanf(line + 7, " %llx", &info->sig_pnd);
        }
    }
    fclose(f);

    // Read cmdline
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes = read(fd, info->cmdline, sizeof(info->cmdline) - 1);
        if (bytes > 0) {
            for (ssize_t i = 0; i < bytes; i++) {
                if (info->cmdline[i] == '\0' && i + 1 < bytes) {
                    info->cmdline[i] = ' ';
                }
            }
            info->cmdline[bytes] = '\0';
        }
        close(fd);
    }
    if (strlen(info->cmdline) == 0) {
        strncpy(info->cmdline, info->name, sizeof(info->cmdline) - 1);
    }

    return true;
}

// Convert a signal bitmask (e.g. SigCgt) to a readable list of signal names
static void decode_signal_mask(unsigned long long mask, char *out_buf, size_t out_len) {
    out_buf[0] = '\0';
    size_t written = 0;
    bool first = true;

    for (int signo = 1; signo <= 31; signo++) {
        if ((mask >> (signo - 1)) & 1ULL) {
            const char *sig_name = NULL;
            for (int i = 0; SIGNAL_TABLE[i].signo != 0; i++) {
                if (SIGNAL_TABLE[i].signo == signo) {
                    sig_name = SIGNAL_TABLE[i].name;
                    break;
                }
            }
            char temp[32];
            if (sig_name) {
                snprintf(temp, sizeof(temp), "%s", sig_name);
            } else {
                snprintf(temp, sizeof(temp), "SIG#%d", signo);
            }

            int needed = snprintf(out_buf + written, out_len - written, "%s%s", 
                                  first ? "" : ", ", temp);
            if (needed > 0 && (size_t)needed < (out_len - written)) {
                written += needed;
            }
            first = false;
        }
    }
    if (first) {
        snprintf(out_buf, out_len, "None (0x0)");
    }
}

// Pretty print process status & telemetry
static void print_process_inspect(const proc_info_t *info, bool json_mode) {
    if (json_mode) {
        char sig_cgt_str[256], sig_ign_str[256], sig_blk_str[256];
        decode_signal_mask(info->sig_cgt, sig_cgt_str, sizeof(sig_cgt_str));
        decode_signal_mask(info->sig_ign, sig_ign_str, sizeof(sig_ign_str));
        decode_signal_mask(info->sig_blk, sig_blk_str, sizeof(sig_blk_str));

        printf("{\n");
        printf("  \"pid\": %d,\n", info->pid);
        printf("  \"ppid\": %d,\n", info->ppid);
        printf("  \"name\": \"%s\",\n", info->name);
        printf("  \"state\": \"%c\",\n", info->state);
        printf("  \"state_desc\": \"%s\",\n", info->state_str);
        printf("  \"vm_size_kb\": %lu,\n", info->vm_size_kb);
        printf("  \"vm_rss_kb\": %lu,\n", info->vm_rss_kb);
        printf("  \"threads\": %d,\n", info->num_threads);
        printf("  \"sig_cgt_hex\": \"0x%016llx\",\n", info->sig_cgt);
        printf("  \"sig_cgt_names\": \"%s\",\n", sig_cgt_str);
        printf("  \"sig_ign_hex\": \"0x%016llx\",\n", info->sig_ign);
        printf("  \"sig_ign_names\": \"%s\",\n", sig_ign_str);
        printf("  \"sig_blk_hex\": \"0x%016llx\",\n", info->sig_blk);
        printf("  \"sig_blk_names\": \"%s\",\n", sig_blk_str);
        printf("  \"cmdline\": \"%s\"\n", info->cmdline);
        printf("}\n");
        return;
    }

    printf("%s┌──────────────────────────────────────────────────────────────┐%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s│ %sPROCESS INSPECTION TELEMETRY [PID: %d]%s%*s│%s\n", 
           COLOR_CYAN, COLOR_BOLD, info->pid, COLOR_RESET, (int)(32 - snprintf(NULL, 0, "%d", info->pid)), "", COLOR_CYAN);
    printf("%s├──────────────────────────────────────────────────────────────┤%s\n", COLOR_CYAN, COLOR_RESET);
    printf("│ %-16s : %s%-38s%s│\n", "Process Name", COLOR_BOLD, info->name, COLOR_RESET);
    printf("│ %-16s : %-38d│\n", "Parent PID (PPID)", info->ppid);

    const char *state_color = COLOR_GREEN;
    if (info->state == 'T') state_color = COLOR_YELLOW;
    else if (info->state == 'Z') state_color = COLOR_RED;

    printf("│ %-16s : %s%-38s%s│\n", "Kernel State", state_color, info->state_str, COLOR_RESET);
    printf("│ %-16s : %-38lu│\n", "Resident RAM (RSS)", info->vm_rss_kb);
    printf("│ %-16s : %-38lu│\n", "Virtual Memory", info->vm_size_kb);
    printf("│ %-16s : %-38d│\n", "Active Threads", info->num_threads);

    char cgt[128], ign[128], blk[128];
    decode_signal_mask(info->sig_cgt, cgt, sizeof(cgt));
    decode_signal_mask(info->sig_ign, ign, sizeof(ign));
    decode_signal_mask(info->sig_blk, blk, sizeof(blk));

    printf("%s├─────────────────── SIGNAL BITMASKS ──────────────────────────┤%s\n", COLOR_CYAN, COLOR_RESET);
    printf("│ %-16s : 0x%016llx               │\n", "SigCgt (Mask)", info->sig_cgt);
    printf("│ %-16s : %-38s│\n", "Caught Signals", cgt);
    printf("│ %-16s : 0x%016llx               │\n", "SigIgn (Mask)", info->sig_ign);
    printf("│ %-16s : %-38s│\n", "Ignored Signals", ign);
    printf("│ %-16s : 0x%016llx               │\n", "SigBlk (Mask)", info->sig_blk);
    printf("│ %-16s : %-38s│\n", "Blocked Signals", blk);
    printf("%s├────────────────────── COMMAND LINE ──────────────────────────┤%s\n", COLOR_CYAN, COLOR_RESET);
    printf("│ %-56.56s │\n", info->cmdline);
    printf("%s└──────────────────────────────────────────────────────────────┘%s\n", COLOR_CYAN, COLOR_RESET);
}

// Core action: Send a POSIX signal to a process with verification
static int dispatch_signal(pid_t pid, int signo, const char *sig_name, bool json_mode) {
    if (!process_exists(pid)) {
        if (json_mode) {
            printf("{\"status\":\"error\",\"error\":\"Process %d does not exist\",\"errno\":%d}\n", pid, ESRCH);
        } else {
            fprintf(stderr, "%s[ERROR] Process %d does not exist or permission denied.%s\n", COLOR_RED, pid, COLOR_RESET);
        }
        return -1;
    }

    proc_info_t before, after;
    read_process_info(pid, &before);

    struct timespec start_ts, end_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    // SYSTEM CALL: kill(pid, signo)
    int ret = kill(pid, signo);
    int saved_errno = errno;

    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    double latency_us = (end_ts.tv_sec - start_ts.tv_sec) * 1000000.0 + 
                        (end_ts.tv_nsec - start_ts.tv_nsec) / 1000.0;

    // Small delay to allow kernel scheduler & signal delivery to update procfs
    usleep(25000); // 25ms
    bool still_exists = read_process_info(pid, &after);

    if (json_mode) {
        printf("{\n");
        printf("  \"status\": \"%s\",\n", (ret == 0) ? "success" : "failed");
        printf("  \"pid\": %d,\n", pid);
        printf("  \"signal\": %d,\n", signo);
        printf("  \"signal_name\": \"%s\",\n", sig_name);
        printf("  \"syscall\": \"kill(%d, %s)\",\n", pid, sig_name);
        printf("  \"return_code\": %d,\n", ret);
        printf("  \"errno\": %d,\n", (ret == 0) ? 0 : saved_errno);
        printf("  \"errno_str\": \"%s\",\n", (ret == 0) ? "Success" : strerror(saved_errno));
        printf("  \"latency_us\": %.2f,\n", latency_us);
        printf("  \"state_before\": \"%c\",\n", before.state);
        printf("  \"state_after\": \"%c\",\n", still_exists ? after.state : 'X');
        printf("  \"process_alive\": %s\n", still_exists ? "true" : "false");
        printf("}\n");
        return ret;
    }

    if (ret == 0) {
        printf("%s✓ [SYSCALL SUCCESS]%s Sent %s%s (%d)%s to PID %d\n", 
               COLOR_GREEN, COLOR_RESET, COLOR_BOLD, sig_name, signo, COLOR_RESET, pid);
        printf("  System Call : kill(pid=%d, sig=%s)\n", pid, sig_name);
        printf("  Kernel Latency: %.2f µs\n", latency_us);
        printf("  State Delta : %s [%c] -> %s [%c]\n", 
               before.state_str, before.state, 
               still_exists ? after.state_str : "TERMINATED", still_exists ? after.state : 'X');
    } else {
        fprintf(stderr, "%s✗ [SYSCALL FAILED]%s kill(%d, %s): %s (errno: %d)\n", 
                COLOR_RED, COLOR_RESET, pid, sig_name, strerror(saved_errno), saved_errno);
    }
    return ret;
}

// Pause Process (SIGSTOP)
static int cmd_pause(pid_t pid, bool json) {
    return dispatch_signal(pid, SIGSTOP, "SIGSTOP", json);
}

// Resume Process (SIGCONT)
static int cmd_resume(pid_t pid, bool json) {
    return dispatch_signal(pid, SIGCONT, "SIGCONT", json);
}

// Graceful Terminate (SIGTERM) with polling wait
static int cmd_terminate(pid_t pid, int timeout_sec, bool json) {
    if (dispatch_signal(pid, SIGTERM, "SIGTERM", json) != 0) {
        return -1;
    }

    if (json) return 0; // JSON dispatch already printed

    printf("  Awaiting graceful exit (timeout %ds)... ", timeout_sec);
    fflush(stdout);

    int elapsed = 0;
    while (process_exists(pid) && elapsed < timeout_sec * 10) {
        usleep(100000); // 100ms
        elapsed++;
    }

    if (!process_exists(pid)) {
        printf("%sDone. Process terminated gracefully.%s\n", COLOR_GREEN, COLOR_RESET);
        return 0;
    } else {
        printf("%sTimed out! Process still running. (Consider using 'kill' with SIGKILL)%s\n", 
               COLOR_YELLOW, COLOR_RESET);
        return 1;
    }
}

// Force Kill Process (SIGKILL)
static int cmd_kill(pid_t pid, bool json) {
    return dispatch_signal(pid, SIGKILL, "SIGKILL", json);
}

// Restart Process (SIGHUP for reload or Fork/Exec respawn)
static int cmd_restart(pid_t pid, bool respawn_mode, bool json) {
    if (!respawn_mode) {
        // Mode A: SIGHUP configuration reload
        if (!json) {
            printf("%s[RESTART]%s Sending SIGHUP for in-place daemon reload without PID change...\n", 
                   COLOR_CYAN, COLOR_RESET);
        }
        return dispatch_signal(pid, SIGHUP, "SIGHUP", json);
    }

    // Mode B: Respawn - read cmdline, terminate old PID, launch new process
    proc_info_t info;
    if (!read_process_info(pid, &info)) {
        fprintf(stderr, "Cannot read /proc/%d/cmdline for respawning.\n", pid);
        return -1;
    }

    if (!json) {
        printf("%s[RESPAWN]%s Terminating existing process %d and relaunching cmd: '%s'...\n", 
               COLOR_YELLOW, COLOR_RESET, pid, info.cmdline);
    }

    // Terminate existing
    kill(pid, SIGTERM);
    int wait_limit = 30; // 3 seconds
    while (process_exists(pid) && wait_limit-- > 0) {
        usleep(100000);
    }
    if (process_exists(pid)) {
        kill(pid, SIGKILL);
        usleep(100000);
    }

    // Fork and exec new process
    pid_t new_pid = fork();
    if (new_pid < 0) {
        perror("fork");
        return -1;
    }
    if (new_pid == 0) {
        // Child
        setsid();
        execl("/bin/sh", "sh", "-c", info.cmdline, (char *)NULL);
        _exit(127);
    }

    // Parent
    usleep(50000);
    if (json) {
        printf("{\n  \"status\": \"success\",\n  \"old_pid\": %d,\n  \"new_pid\": %d,\n  \"cmdline\": \"%s\"\n}\n", 
               pid, new_pid, info.cmdline);
    } else {
        printf("%s✓ [RESPAWN COMPLETE]%s New Process spawned successfully: PID %s%d%s\n", 
               COLOR_GREEN, COLOR_RESET, COLOR_BOLD, new_pid, COLOR_RESET);
    }
    return 0;
}

// Spawn a new background tracked process
static int cmd_spawn(int argc, char *argv[], bool json) {
    if (argc < 3) {
        fprintf(stderr, "Usage: proc_ctrl spawn <command> [args...]\n");
        return -1;
    }

    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        return -1;
    }

    if (child_pid == 0) {
        // In Child: set process group
        setpgid(0, 0);
        execvp(argv[2], &argv[2]);
        // If execvp fails:
        fprintf(stderr, "execvp failed: %s\n", strerror(errno));
        _exit(127);
    }

    // Parent
    usleep(50000); // 50ms to allow child to exec
    if (json) {
        printf("{\n  \"status\": \"success\",\n  \"pid\": %d,\n  \"command\": \"%s\"\n}\n", child_pid, argv[2]);
    } else {
        printf("%s✓ [SPAWN SUCCESS]%s Created new process PID: %s%d%s\n", 
               COLOR_GREEN, COLOR_RESET, COLOR_BOLD, child_pid, COLOR_RESET);
        printf("  Command: %s\n", argv[2]);
        printf("  Process Group: %d\n", getpgid(child_pid));
    }
    return 0;
}

// List active running processes
static int cmd_list(const char *filter, bool json) {
    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("opendir(/proc)");
        return -1;
    }

    struct dirent *entry;
    int count = 0;

    if (json) printf("[\n");
    else {
        printf("%s%-8s %-6s %-12s %-10s %-10s %-24s%s\n", 
               COLOR_BOLD, "PID", "PPID", "STATE", "RSS(KB)", "THREADS", "COMMAND", COLOR_RESET);
        printf("───────────────────────────────────────────────────────────────────────\n");
    }

    bool first = true;
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(entry->d_name[0])) continue;

        pid_t pid = atoi(entry->d_name);
        proc_info_t info;
        if (!read_process_info(pid, &info)) continue;

        if (filter && strstr(info.name, filter) == NULL && strstr(info.cmdline, filter) == NULL) {
            continue;
        }

        if (json) {
            if (!first) printf(",\n");
            first = false;
            printf("  {\"pid\":%d,\"ppid\":%d,\"name\":\"%s\",\"state\":\"%c\",\"state_str\":\"%s\",\"rss_kb\":%lu,\"threads\":%d,\"cmdline\":\"%s\"}", 
                   info.pid, info.ppid, info.name, info.state, info.state_str, info.vm_rss_kb, info.num_threads, info.cmdline);
        } else {
            const char *col = COLOR_RESET;
            if (info.state == 'R') col = COLOR_GREEN;
            else if (info.state == 'T') col = COLOR_YELLOW;
            else if (info.state == 'Z') col = COLOR_RED;

            printf("%-8d %-6d %s%-12c%s %-10lu %-10d %-24.24s\n", 
                   info.pid, info.ppid, col, info.state, COLOR_RESET, 
                   info.vm_rss_kb, info.num_threads, info.name);
        }
        count++;
    }
    closedir(dir);

    if (json) printf("\n]\n");
    else {
        printf("───────────────────────────────────────────────────────────────────────\n");
        printf("Total Processes Listed: %d\n", count);
    }
    return 0;
}

// Print CLI usage guide
static void print_usage(const char *prog_name) {
    printf("%sLinux Signal-Based Process Controller (v%s)%s\n", COLOR_BOLD, PROC_CTRL_VERSION, COLOR_RESET);
    printf("Academic Systems Programming & Process Lifecycle Utility (OSSP Project)\n\n");
    printf("%sUSAGE:%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s <command> [arguments...] [--json]\n\n", prog_name);
    printf("%sCORE COMMANDS:%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %spause%s     <PID>              Pause process execution via SIGSTOP (19)\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sresume%s    <PID>              Resume stopped process execution via SIGCONT (18)\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sterminate%s <PID> [timeout]    Graceful termination via SIGTERM (15)\n", COLOR_GREEN, COLOR_RESET);
    printf("  %skill%s      <PID>              Immediate forceful eviction via SIGKILL (9)\n", COLOR_RED, COLOR_RESET);
    printf("  %srestart%s   <PID> [--respawn]  Send SIGHUP (1) or terminate & respawn\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sspawn%s     <cmd> [args...]    Fork and exec a new tracked background process\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sstatus%s    <PID>              Inspect process state & signal masks (/proc)\n", COLOR_CYAN, COLOR_RESET);
    printf("  %slist%s      [filter]           List processes with states and RSS memory\n", COLOR_WHITE, COLOR_RESET);
    printf("  %ssignal%s    <PID> <SIGNAME>    Send arbitrary POSIX signal (SIGUSR1, SIGINT...)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("\n%sOPTIONS:%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("  --json                           Output structured JSON for web/API integration\n");
    printf("  -h, --help                       Show this help menu\n\n");
    printf("%sEXAMPLES:%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s spawn ./bin/dummy_worker\n", prog_name);
    printf("  %s pause 1420\n", prog_name);
    printf("  %s resume 1420\n", prog_name);
    printf("  %s status 1420 --json\n", prog_name);
    printf("  %s terminate 1420\n", prog_name);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    bool json_mode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
            break;
        }
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "list") == 0) {
        const char *filter = (argc >= 3 && argv[2][0] != '-') ? argv[2] : NULL;
        return cmd_list(filter, json_mode);
    }

    if (strcmp(cmd, "spawn") == 0) {
        return cmd_spawn(argc, argv, json_mode);
    }

    if (argc < 3) {
        fprintf(stderr, "%sError: Command '%s' requires a target PID.%s\n", COLOR_RED, cmd, COLOR_RESET);
        print_usage(argv[0]);
        return 1;
    }

    pid_t target_pid = (pid_t)atoi(argv[2]);
    if (target_pid <= 0) {
        fprintf(stderr, "%sError: Invalid PID '%s'.%s\n", COLOR_RED, argv[2], COLOR_RESET);
        return 1;
    }

    if (strcmp(cmd, "pause") == 0) {
        return cmd_pause(target_pid, json_mode);
    } else if (strcmp(cmd, "resume") == 0) {
        return cmd_resume(target_pid, json_mode);
    } else if (strcmp(cmd, "terminate") == 0) {
        int timeout = (argc >= 4 && isdigit(argv[3][0])) ? atoi(argv[3]) : 5;
        return cmd_terminate(target_pid, timeout, json_mode);
    } else if (strcmp(cmd, "kill") == 0) {
        return cmd_kill(target_pid, json_mode);
    } else if (strcmp(cmd, "restart") == 0) {
        bool respawn = false;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--respawn") == 0) respawn = true;
        }
        return cmd_restart(target_pid, respawn, json_mode);
    } else if (strcmp(cmd, "status") == 0 || strcmp(cmd, "inspect") == 0) {
        proc_info_t info;
        if (!read_process_info(target_pid, &info)) {
            if (json_mode) printf("{\"error\":\"Process %d not found\"}\n", target_pid);
            else fprintf(stderr, "%sProcess %d not found.%s\n", COLOR_RED, target_pid, COLOR_RESET);
            return 1;
        }
        print_process_inspect(&info, json_mode);
        return 0;
    } else if (strcmp(cmd, "signal") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: proc_ctrl signal <PID> <SIGNAL_NAME>\n");
            return 1;
        }
        const char *sig_name = argv[3];
        int signo = -1;
        for (int i = 0; SIGNAL_TABLE[i].signo != 0; i++) {
            if (strcasecmp(SIGNAL_TABLE[i].name, sig_name) == 0 || 
                (sig_name[0] >= '0' && sig_name[0] <= '9' && atoi(sig_name) == SIGNAL_TABLE[i].signo)) {
                signo = SIGNAL_TABLE[i].signo;
                sig_name = SIGNAL_TABLE[i].name;
                break;
            }
        }
        if (signo == -1) {
            fprintf(stderr, "%sUnknown signal '%s'.%s\n", COLOR_RED, sig_name, COLOR_RESET);
            return 1;
        }
        return dispatch_signal(target_pid, signo, sig_name, json_mode);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}
