# Linux Systems Programming & OSSP Portfolio

## Team Members (Team 19)

| Name | Student ID |
| :--- | :--- |
| S. Teja | 2520030419 |
| A. Charanteja | 2520030418 |
| E. Ashwith Reddy | 2520030475 |

## Supervisor

**Supervisor:** Soumya

---

## 🏆 Featured Project: Linux Signal-Based Process Controller
### Open Source Software Practices (OSSP) Hackathon Showcase Project

👉 **[View Complete Project Documentation & Instructions](Hackathon/README.md)**

A high-performance POSIX systems utility and interactive kernel telemetry dashboard that manages process lifecycles via Linux signals:
- **Pause & Resume**: Freeze (`SIGSTOP` 19) and resume (`SIGCONT` 18) execution context in the kernel scheduler.
- **Graceful Termination & Force Kill**: Cooperative shutdown (`SIGTERM` 15) vs preemptive eviction (`SIGKILL` 9).
- **Configuration Hot-Reload**: In-place daemon configuration reload without PID teardown (`SIGHUP` 1).
- **Kernel Introspection**: ProcFS parser decoding 64-bit hexadecimal signal bitmasks (`SigCgt`, `SigBlk`, `SigIgn`).
- **Interactive Web Platform & Viva Deck**: 10-slide academic slide deck, live state machine diagram, and chaos lab.

### Quick Links:
- 💻 **Complete C Source Code:** [`Hackathon/src/`](Hackathon/src/)
  - [`Hackathon/src/proc_controller.c`](Hackathon/src/proc_controller.c) - CLI Process Controller
  - [`Hackathon/src/dummy_worker.c`](Hackathon/src/dummy_worker.c) - Target worker process
  - [`Hackathon/src/Makefile`](Hackathon/src/Makefile) - GCC build configuration
  - [`Hackathon/src/test_signals.sh`](Hackathon/src/test_signals.sh) - Automated 7-step test suite
- 🌐 **Web Dashboard & Presentation:** [`Hackathon/web/`](Hackathon/web/)
- 📖 **Setup, Compilation & Execution Instructions:** [`Hackathon/README.md`](Hackathon/README.md)
- 📊 **Presentation Slides & Technical Report:** [`Hackathon/Documents/`](Hackathon/Documents/)
  - `Hackathon/Documents/HACKATHON_Team19.pptx`
  - `Hackathon/Documents/Linux_Signal_Based_Process_Controller_–_Project_Documentation.pdf`

---

## Repository Structure

```text
Hackathon/  - Linux Signal-Based Process Controller (OSSP Hackathon)
project/    - Linux Graphical Debugger implementation
Practical/  - Lab practical experiments (Experiments 1 - 12)
skill/      - Skill development modules (Weeks 1 - 14)
```
