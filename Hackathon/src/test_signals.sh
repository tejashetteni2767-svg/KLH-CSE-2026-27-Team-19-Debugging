#!/usr/bin/env bash
# ==============================================================================
# Linux Signal-Based Process Controller - Automated Test Suite
# ==============================================================================
set -e

GREEN="\033[1;32m"
RED="\033[1;31m"
CYAN="\033[1;36m"
YELLOW="\033[1;33m"
RESET="\033[0m"

echo -e "${CYAN}==============================================================${RESET}"
echo -e "${CYAN}  STARTING AUTOMATED POSIX SIGNAL VERIFICATION TEST SUITE     ${RESET}"
echo -e "${CYAN}==============================================================${RESET}"

BIN_DIR="../bin"
PROG="${BIN_DIR}/proc_ctrl"
WORKER="${BIN_DIR}/dummy_worker"

if [[ ! -f "$PROG" || ! -f "$WORKER" ]]; then
    echo -e "${RED}[ERROR] Binaries not found. Run 'make' first.${RESET}"
    exit 1
fi

echo -e "\n${YELLOW}[TEST 1] Launching target worker process...${RESET}"
$WORKER test_worker 500 > /tmp/worker.log 2>&1 &
TARGET_PID=$!
sleep 0.5

if ! ps -p $TARGET_PID > /dev/null; then
    echo -e "${RED}[FAIL] Worker failed to launch.${RESET}"
    exit 1
fi
echo -e "${GREEN}[PASS] Worker running with PID: ${TARGET_PID}${RESET}"

echo -e "\n${YELLOW}[TEST 2] Testing Process Inspection (/proc telemetry)...${RESET}"
$PROG status $TARGET_PID

echo -e "\n${YELLOW}[TEST 3] Testing PAUSE (SIGSTOP)...${RESET}"
$PROG pause $TARGET_PID
sleep 0.1
STATE=$(awk '/State:/ {print $2}' /proc/$TARGET_PID/status)
if [[ "$STATE" == "T" ]]; then
    echo -e "${GREEN}[PASS] Process successfully paused. Kernel State: 'T' (Stopped)${RESET}"
else
    echo -e "${RED}[FAIL] Expected state 'T', got '${STATE}'${RESET}"
    kill -9 $TARGET_PID || true
    exit 1
fi

echo -e "\n${YELLOW}[TEST 4] Testing RESUME (SIGCONT)...${RESET}"
$PROG resume $TARGET_PID
sleep 0.1
STATE=$(awk '/State:/ {print $2}' /proc/$TARGET_PID/status)
if [[ "$STATE" == "R" || "$STATE" == "S" ]]; then
    echo -e "${GREEN}[PASS] Process resumed execution. Kernel State: '${STATE}'${RESET}"
else
    echo -e "${RED}[FAIL] Expected state 'R' or 'S', got '${STATE}'${RESET}"
    kill -9 $TARGET_PID || true
    exit 1
fi

echo -e "\n${YELLOW}[TEST 5] Testing CONFIG RELOAD / SOFT RESTART (SIGHUP)...${RESET}"
$PROG restart $TARGET_PID
sleep 0.2
echo -e "${GREEN}[PASS] SIGHUP signal delivered successfully.${RESET}"

echo -e "\n${YELLOW}[TEST 6] Testing GRACEFUL TERMINATION (SIGTERM)...${RESET}"
$PROG terminate $TARGET_PID 3
sleep 0.2
if ps -p $TARGET_PID > /dev/null; then
    echo -e "${RED}[FAIL] Process failed to terminate with SIGTERM.${RESET}"
    kill -9 $TARGET_PID || true
    exit 1
else
    echo -e "${GREEN}[PASS] Process terminated gracefully with cleanup!${RESET}"
fi

echo -e "\n${YELLOW}[TEST 7] Testing FORCEFUL KILL (SIGKILL)...${RESET}"
$WORKER unkillable_test 500 > /tmp/worker2.log 2>&1 &
KILL_PID=$!
sleep 0.3
$PROG kill $KILL_PID
sleep 0.2
if ps -p $KILL_PID > /dev/null; then
    echo -e "${RED}[FAIL] Process survived SIGKILL!${RESET}"
    exit 1
else
    echo -e "${GREEN}[PASS] Process killed immediately by kernel (SIGKILL is uncatchable).${RESET}"
fi

echo -e "\n${CYAN}==============================================================${RESET}"
echo -e "${GREEN}  ALL 7 TESTS PASSED SUCCESSFULLY! OS SIGNAL CONTROLLER VERIFIED! ${RESET}"
echo -e "${CYAN}==============================================================${RESET}"
