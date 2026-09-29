# Mini Linux Debugger

OSSP project: a lightweight Linux-based graphical debugger written in C.

## Features

- GTK graphical interface
- Process creation using fork()
- Program execution using exec()
- Process synchronization using waitpid()
- Debugging using ptrace()
- Continue execution
- Single-step execution
- Software breakpoint using INT 3
- CPU register inspection
- Memory inspection
- Process stop

## Requirements

Ubuntu Linux, GCC, GTK 3 development package, Git.

Install:

sudo apt update
sudo apt install build-essential gcc gdb libgtk-3-dev git

## Build

make

## Run GUI

./bin/debugger_gui

## Run command-line version

./bin/my_debugger ./bin/test_program

## Breakpoint

Build with -O0 and -g. Find a function address with:

nm -n bin/test_program | grep calculate

Enter the address in the GUI breakpoint field as hexadecimal.

Note:
This educational version is intended for x86-64 Linux. Address layout can vary with ASLR. For a simple classroom demonstration, you may temporarily disable ASLR for the test environment:

setarch $(uname -m) -R ./bin/debugger_gui

Only use this on your own test program/system.
