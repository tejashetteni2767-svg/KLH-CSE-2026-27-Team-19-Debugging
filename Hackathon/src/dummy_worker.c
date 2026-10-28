/**
 * ============================================================================
 * Project: Linux Signal-Based Process Controller (OSSP Project)
 * File: src/dummy_worker.c
 * Description: Controllable target worker process demonstrating POSIX signal
 *              handling, state preservation across SIGSTOP/SIGCONT, graceful
 *              termination with SIGTERM, and configuration reload with SIGHUP.
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
#include <stdbool.h>
#include <time.h>
#include <errno.h>

// Helper to write without compiler warning
static inline void safe_write(int fd, const char *buf, size_t len) {
    ssize_t ret = write(fd, buf, len);
    (void)ret;
}

// Volatile signal counters and state flags (async-signal-safe)
static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_hup_received = 0;
static volatile sig_atomic_t g_usr1_received = 0;
static volatile sig_atomic_t g_usr2_received = 0;
static volatile sig_atomic_t g_sigint_count = 0;
static unsigned long long g_iteration = 0;
static char g_workload_name[64] = "standard_compute";
static int g_heartbeat_interval_ms = 1000;

// Signal handler for SIGTERM (Graceful Shutdown)
static void handle_sigterm(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)context;
    // Async-signal-safe write
    const char msg[] = "\n[WORKER] Received SIGTERM (15)! Initiating graceful shutdown cleanup...\n";
    safe_write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    if (info) {
        char sender_buf[64];
        int len = snprintf(sender_buf, sizeof(sender_buf), 
                           "[WORKER] Signal dispatched by sender PID: %d, UID: %d\n", 
                           info->si_pid, info->si_uid);
        if (len > 0) safe_write(STDOUT_FILENO, sender_buf, len);
    }
    g_running = 0;
}

// Signal handler for SIGHUP (Config Reload / Soft Restart)
static void handle_sighup(int sig, siginfo_t *info, void *context) {
    (void)sig; (void)info; (void)context;
    const char msg[] = "\n[WORKER] Received SIGHUP (1)! Queued configuration reload...\n";
    safe_write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    g_hup_received = 1;
}

// Signal handler for SIGINT (Terminal Interrupt)
static void handle_sigint(int sig, siginfo_t *info, void *context) {
    (void)sig; (void)info; (void)context;
    const char msg[] = "\n[WORKER] Caught SIGINT (2)! (Use SIGTERM or kill utility for shutdown)\n";
    safe_write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    g_sigint_count++;
    if (g_sigint_count >= 3) {
        const char force_msg[] = "[WORKER] 3 SIGINT signals caught. Force exiting.\n";
        safe_write(STDOUT_FILENO, force_msg, sizeof(force_msg) - 1);
        g_running = 0;
    }
}

// Signal handler for SIGUSR1 / SIGUSR2
static void handle_sigusr(int sig, siginfo_t *info, void *context) {
    (void)info; (void)context;
    if (sig == SIGUSR1) {
        g_usr1_received = 1;
    } else if (sig == SIGUSR2) {
        g_usr2_received = 1;
    }
}

// Setup POSIX sigaction handlers
static void setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO | SA_RESTART;

    // Block all other standard signals while handling
    sigfillset(&sa.sa_mask);

    // 1. SIGTERM - Graceful termination
    sa.sa_sigaction = handle_sigterm;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction(SIGTERM)");
    }

    // 2. SIGHUP - Configuration reload
    sa.sa_sigaction = handle_sighup;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction(SIGHUP)");
    }

    // 3. SIGINT - Interrupt handle
    sa.sa_sigaction = handle_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT)");
    }

    // 4. SIGUSR1 & SIGUSR2 - User telemetry
    sa.sa_sigaction = handle_sigusr;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    // DEMONSTRATION OF POSIX RULE: Attempting to catch SIGKILL & SIGSTOP
    // Kernel MUST return EINVAL because SIGKILL & SIGSTOP cannot be caught, blocked, or ignored!
    printf("[WORKER] Educational POSIX Verification:\n");
    if (sigaction(SIGKILL, &sa, NULL) == -1) {
        printf("  -> Attempt to catch SIGKILL (9): Failed as expected with errno %d (%s)\n", 
               errno, strerror(errno));
    }
    if (sigaction(SIGSTOP, &sa, NULL) == -1) {
        printf("  -> Attempt to catch SIGSTOP (19): Failed as expected with errno %d (%s)\n", 
               errno, strerror(errno));
    }
}

// Simulate configuration reload
static void perform_config_reload(void) {
    printf("[WORKER] >>> SIGHUP Reload Executing: Re-reading worker config parameters...\n");
    snprintf(g_workload_name, sizeof(g_workload_name), "reloaded_workload_%llu", g_iteration);
    printf("[WORKER] >>> Config reloaded: Workload ID = '%s', Heartbeat Interval = %d ms\n", 
           g_workload_name, g_heartbeat_interval_ms);
    g_hup_received = 0;
}

// Display diagnostics for SIGUSR1
static void perform_diagnostics(void) {
    printf("==================== WORKER DIAGNOSTICS ====================\n");
    printf("  PID: %d | PPID: %d | Workload: %s\n", getpid(), getppid(), g_workload_name);
    printf("  Iteration Ticks: %llu\n", g_iteration);
    printf("  SIGINT Intercepted: %d times\n", g_sigint_count);
    printf("  Memory Page Size: %ld bytes\n", sysconf(_SC_PAGESIZE));
    printf("============================================================\n");
    g_usr1_received = 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        strncpy(g_workload_name, argv[1], sizeof(g_workload_name) - 1);
    }
    if (argc > 2) {
        g_heartbeat_interval_ms = atoi(argv[2]);
        if (g_heartbeat_interval_ms <= 0) g_heartbeat_interval_ms = 1000;
    }

    pid_t pid = getpid();
    printf("================================================================\n");
    printf("  TARGET WORKER STARTED [PID: %d, PPID: %d]\n", pid, getppid());
    printf("  Workload Type: %s | Heartbeat Interval: %d ms\n", g_workload_name, g_heartbeat_interval_ms);
    printf("  Supported Signals:\n");
    printf("    - SIGSTOP (19) : Kernel Pause (uninterceptable)\n");
    printf("    - SIGCONT (18) : Kernel Resume\n");
    printf("    - SIGTERM (15) : Graceful Shutdown\n");
    printf("    - SIGKILL (9)  : Immediate Force Kill (uninterceptable)\n");
    printf("    - SIGHUP  (1)  : Live Configuration Reload\n");
    printf("    - SIGUSR1 (10) : Print Diagnostics\n");
    printf("================================================================\n");

    setup_signals();

    time_t start_time = time(NULL);

    while (g_running) {
        g_iteration++;
        time_t now = time(NULL);
        double elapsed = difftime(now, start_time);

        printf("[PID %d] [Tick %04llu | %04.0fs] Workload: %s | State: TASK_RUNNING\n", 
               pid, g_iteration, elapsed, g_workload_name);
        fflush(stdout);

        // Check for queued signal actions
        if (g_hup_received) {
            perform_config_reload();
        }
        if (g_usr1_received) {
            perform_diagnostics();
        }
        if (g_usr2_received) {
            printf("[WORKER] >>> SIGUSR2: Resetting iteration tick counter to 0.\n");
            g_iteration = 0;
            g_usr2_received = 0;
        }

        // Sleep for requested heartbeat interval
        usleep(g_heartbeat_interval_ms * 1000);
    }

    // Graceful cleanup phase
    printf("[WORKER] Performing cleanup tasks before exiting...\n");
    printf("  -> Flushing internal buffers.\n");
    printf("  -> Releasing allocated resources.\n");
    printf("  -> Total execution ticks completed: %llu\n", g_iteration);
    printf("[WORKER] Graceful termination complete. Exiting with code 0.\n");
    return 0;
}
