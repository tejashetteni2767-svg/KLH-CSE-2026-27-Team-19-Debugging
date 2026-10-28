# Linux Signal-Based Process Controller
### High-Performance POSIX Systems Utility & Interactive Kernel Telemetry Dashboard
**Open Source Software Practices (OSSP) Hackathon Showcase Project**

[![GitHub Repository](https://img.shields.io/badge/GitHub-Repository-181717?style=for-the-badge&logo=github)](https://github.com/allamcharanteja2008-arch/Linux-Based-Signal-Process-Controller)
![POSIX Compliant](https://img.shields.io/badge/Standard-POSIX.1--2008-00d2ff?style=for-the-badge)
![Linux Kernel](https://img.shields.io/badge/Kernel-Linux%206.x-10b981?style=for-the-badge)
![GCC C11](https://img.shields.io/badge/Language-C11%20%2F%20GCC-f59e0b?style=for-the-badge)
![Web Dashboard](https://img.shields.io/badge/UI-Modern%20Web%20Dashboard-8b5cf6?style=for-the-badge)

Repository Link: [https://github.com/allamcharanteja2008-arch/Linux-Based-Signal-Process-Controller](https://github.com/allamcharanteja2008-arch/Linux-Based-Signal-Process-Controller)

---

## 🌟 Executive Summary

In enterprise Linux environments, cloud servers, and container orchestration platforms (Docker, Kubernetes), processes frequently encounter deadlocks, memory pressure, or require non-disruptive configuration hot-reloads. Naively terminating processes with brutal kills causes connection drops, cache loss, and file corruption.

This project delivers a **two-tier Linux Process Control Solution**:
1. **Core C Systems Utility (`proc_ctrl`)**: A robust, standard C (C11) command-line interface that pauses (`SIGSTOP`), resumes (`SIGCONT`), gracefully terminates (`SIGTERM`), forcefully evicts (`SIGKILL`), and hot-reloads (`SIGHUP`) processes, while parsing `/proc/[pid]/status` to decode 64-bit signal bitmasks in microsecond intervals.
2. **Interactive Faculty Evaluation Dashboard**: A self-contained, responsive web application featuring real-time process monitoring, an interactive Linux Task State Machine visualizer, live syscall streaming, 4 hands-on chaos experiments, syntax-annotated C source code viewers, and a **10-slide academic slide deck with speaker viva notes**.

---

## 🛠️ Complete Source Code Breakdown

| Component | Path | Language / Tech | Description |
| :--- | :--- | :--- | :--- |
| **CLI Controller** | [`src/proc_controller.c`](src/proc_controller.c) | C11 (POSIX.1-2008) | Primary utility executing `kill()`, `fork()`, `execvp()`, and parsing `/proc/[pid]/status`. |
| **Target Worker** | [`src/dummy_worker.c`](src/dummy_worker.c) | C11 | Controllable workload demonstrating signal handling (`SIGTERM`, `SIGHUP`, `SIGINT`, `SIGUSR1`). |
| **Build System** | [`src/Makefile`](src/Makefile) | GNU Make | Automated compilation with strict flags (`-Wall -Wextra -O2 -pedantic -std=c11`). |
| **Test Suite** | [`src/test_signals.sh`](src/test_signals.sh) | Bash | Automated 7-step test bench verifying pause, resume, reload, graceful stop, and kill. |
| **Backend Daemon** | [`server.js`](server.js) | Node.js, Express, WS | REST & WebSocket bridge linking web UI with native Linux/WSL system calls. |
| **Web Dashboard** | [`web/index.html`](web/index.html) | HTML5 | Multi-tab interactive UI, live state machine SVG, and faculty presentation slides. |
| **Styling** | [`web/css/styles.css`](web/css/styles.css) | Modern CSS3 | Glassmorphism, cyber dark / solarized / light themes, and print-ready dossier layout. |
| **App Engine** | [`web/js/app.js`](web/js/app.js) | JavaScript (ES6+) | Process management engine, `/proc` bitmask decoder, and chaos lab controller. |
| **Slide Deck** | [`web/js/presentation.js`](web/js/presentation.js) | JavaScript | 10-slide viva presentation controller with keyboard shortcuts & speaker notes. |

---

## 🚀 Setup, Compilation, and Execution Instructions

### 1. Prerequisites
- **For Web Dashboard:** Node.js (v18+) and npm.
- **For Native C Compilation:** GCC (supporting C11/POSIX.1-2008) and Make on Linux or WSL2 (Windows Subsystem for Linux).

---

### 2. Clone the Repository
```bash
git clone https://github.com/allamcharanteja2008-arch/Linux-Based-Signal-Process-Controller.git
cd Linux-Based-Signal-Process-Controller
```

---

### 3. Option A: Launch the Web Dashboard & Presentation
```bash
# 1. Install dependencies (one-time)
npm install

# 2. Launch web server daemon
npm start
```
Access the dashboard at: **`http://localhost:3000`**

> **Zero-Install Standalone Presentation Mode:** You can also directly open `web/index.html` in any web browser without running Node.js or Linux! It features a high-fidelity virtual kernel simulation engine.

---

### 4. Option B: Build & Run Native C Binaries on Linux / WSL

The C utility is fully standards-compliant and compiles with zero warnings under `-Wall -Wextra -O2 -pedantic`:

```bash
# Compile both proc_ctrl and dummy_worker into bin/
make -C src

# Run the automated 7-step test suite
make -C src test
```

#### CLI Command Examples
```bash
# 1. Launch a background target worker
./bin/proc_ctrl spawn ./bin/dummy_worker worker_service 1000

# 2. Inspect kernel telemetry and decoded signal masks
./bin/proc_ctrl status <PID>

# 3. Pause process execution (SIGSTOP 19)
./bin/proc_ctrl pause <PID>

# 4. Resume stopped execution (SIGCONT 18)
./bin/proc_ctrl resume <PID>

# 5. Hot reload configuration without PID change (SIGHUP 1)
./bin/proc_ctrl restart <PID>

# 6. Graceful shutdown with resource cleanup (SIGTERM 15)
./bin/proc_ctrl terminate <PID>

# 7. Forceful uncatchable kill (SIGKILL 9)
./bin/proc_ctrl kill <PID>
```

---

## 📊 Core Signal Reference & Kernel Dispositions

| Signal Name | Number | Default Action | Catchable? | Blockable? | Controller System Role |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **`SIGSTOP`** | 19 | Stop Process | ❌ **NO** | ❌ **NO** | **Freeze process execution immediately in kernel scheduler** |
| **`SIGCONT`** | 18 | Continue / Resume | ✅ YES | ✅ YES | **Wake up suspended process back to `TASK_RUNNING`** |
| **`SIGTERM`** | 15 | Terminate | ✅ YES | ✅ YES | **Cooperative graceful shutdown request (cleanup buffers)** |
| **`SIGKILL`** | 9 | Force Kill | ❌ **NO** | ❌ **NO** | **Preemptive eviction by kernel (bypasses user space)** |
| **`SIGHUP`** | 1 | Terminate | ✅ YES | ✅ YES | **In-place daemon configuration reload (preserves PID)** |
| **`SIGINT`** | 2 | Interrupt | ✅ YES | ✅ YES | **Terminal interrupt (Ctrl+C)** |
| **`SIGCHLD`** | 17 | Ignore | ✅ YES | ✅ YES | **Asynchronous child reaping to eliminate zombies** |

---

## 🧠 Key Academic Concepts for Faculty Presentation

### 1. Why `SIGSTOP` & `SIGKILL` Cannot be Caught or Ignored
In POSIX operating systems, user-space code installs custom signal handlers using the `sigaction(2)` system call. When the kernel delivers `SIGKILL` or `SIGSTOP`, it **deliberately bypasses the process's signal descriptor table (`task_struct->sighand`)**. The scheduler halts or terminates the process immediately in kernel mode. If user processes could intercept these signals, malicious programs or infinite loops could render an OS unkillable. Attempting `sigaction(SIGKILL, ...)` returns `-1` with errno `EINVAL`.

### 2. Cooperative Shutdown (`SIGTERM`) vs Preemptive Eviction (`SIGKILL`)
- **`SIGTERM` (15):** The process receives the signal, invokes its `handle_sigterm()` handler, flushes open file buffers, closes database connections, and calls `exit(0)`.
- **`SIGKILL` (9):** The kernel instantly tears down the memory address space (`mm_struct`) and removes the process from the runqueue. Unwritten buffers are lost.

### 3. SIGHUP Hot Reload vs Respawn Restart
- **In-place reload (`SIGHUP`):** Process keeps its original PID, open file descriptors, listening network sockets, and shared memory segments while re-parsing config files.
- **Respawn Restart:** Kills the original process, invokes `fork()` and `execvp()`, generating a new PID.

### 4. Zombie Process (`TASK_ZOMBIE`) Prevention
When a child process terminates, its memory is reclaimed, but its entry in the kernel process table remains so the parent can inspect its exit status. If the parent does not call `waitpid()`, the process becomes a **Zombie (`Z`)**. Our controller installs an asynchronous `SIGCHLD` handler that calls `waitpid(-1, &status, WNOHANG)` in a non-blocking loop to reap finished children immediately.

---

## 📂 Project Architecture

```
OSSP Hackathon/
├── bin/                       # Compiled Linux ELF binaries
│   ├── proc_ctrl              # Process Controller CLI utility
│   └── dummy_worker           # Controllable target worker process
├── src/                       # POSIX C Source Code & Tests
│   ├── proc_controller.c      # Core CLI utility implementation
│   ├── dummy_worker.c         # Target worker with signal handlers
│   ├── Makefile               # GCC build system (-Wall -Wextra -O2)
│   └── test_signals.sh        # Automated 7-step test bench script
├── web/                       # Faculty-Ready Web Platform
│   ├── index.html             # Multi-tab dashboard & presentation deck
│   ├── css/
│   │   └── styles.css         # Modern cyber UI, glassmorphism, responsive
│   └── js/
│       ├── app.js             # Process controller engine & state visualizer
│       └── presentation.js    # Slide deck navigator & speaker notes
├── server.js                  # Node.js Express + WebSocket backend daemon
├── package.json               # Project metadata & npm scripts
└── README.md                  # Comprehensive technical documentation
```

---

## 🎓 Faculty Presentation Guide & Viva Cheatsheet

The website includes a dedicated **Faculty Presentation Tab** featuring a 10-slide academic deck.

### Keyboard Shortcuts During Viva:
- **`Arrow Right` / `Space`**: Next Slide
- **`Arrow Left`**: Previous Slide
- **`F` Key**: Toggle Fullscreen Presentation Mode
- **`💡 Speaker Notes`**: Toggles live evaluation talking points for the presenter.

---

## 🏆 Evaluation Rubrics Alignment (OSSP Hackathon)

1. **System Programming Depth**: Clean, modular C11 code utilizing POSIX system calls (`kill`, `sigaction`, `waitpid`, `fork`, `execvp`, `sysconf`).
2. **Kernel Introspection**: Custom algorithm parsing `/proc/[pid]/status` and decoding 64-bit hexadecimal signal bitmasks (`SigCgt`, `SigBlk`, `SigIgn`).
3. **Robustness & Reliability**: Zero compiler warnings, automated test verification suite, and async-signal-safe handlers.
4. **Interactive Showcase**: Dual-engine web application featuring real-time state machine diagrams, chaos experiments, and viva preparation.
