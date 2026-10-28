# ==============================================================================
# Makefile: Linux Signal-Based Process Controller (OSSP Hackathon)
# ==============================================================================

CC ?= gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -pedantic -D_GNU_SOURCE
BIN_DIR = ../bin

TARGETS = $(BIN_DIR)/proc_ctrl $(BIN_DIR)/dummy_worker

.PHONY: all clean test help

all: $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/proc_ctrl: proc_controller.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ proc_controller.c

$(BIN_DIR)/dummy_worker: dummy_worker.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ dummy_worker.c

test: all
	chmod +x test_signals.sh
	./test_signals.sh

clean:
	rm -rf $(BIN_DIR)/*.o $(TARGETS)

help:
	@echo "Available build targets:"
	@echo "  make        - Build both proc_ctrl and dummy_worker into ../bin/"
	@echo "  make test   - Run automated POSIX signal test suite"
	@echo "  make clean  - Remove compiled binaries"
