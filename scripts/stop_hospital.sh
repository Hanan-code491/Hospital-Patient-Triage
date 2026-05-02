#!/bin/bash
# ============================================================
# Project : Hospital Patient Triage & Bed Allocator
# Script  : stop_hospital.sh
# Purpose : Send SIGTERM to admissions, clean up all IPC
#           resources, print final summary.
# Usage   : ./stop_hospital.sh
# ============================================================

PID_FILE="/tmp/hospital_admissions.pid"
DISCHARGE_FIFO="/tmp/discharge_fifo"

echo "============================================"
echo "  HOSPITAL SHUTDOWN INITIATED               "
echo "============================================"

# Send SIGTERM to admissions process
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if kill -0 "$PID" 2>/dev/null; then
        echo "[STOP] Sending SIGTERM to admissions (PID=$PID)..."
        kill -SIGTERM "$PID"
        sleep 2
        # Force kill if still running
        if kill -0 "$PID" 2>/dev/null; then
            echo "[STOP] Force killing PID=$PID"
            kill -9 "$PID"
        fi
        echo "[STOP] Admissions manager stopped."
    else
        echo "[STOP] Admissions process (PID=$PID) already stopped."
    fi
    rm -f "$PID_FILE"
else
    echo "[STOP] No PID file found. Trying pkill..."
    pkill -SIGTERM -f ./admissions 2>/dev/null || true
fi

# Clean up shared memory
echo "[STOP] Removing shared memory segment (key=0xBEDF00D)..."
ipcrm -M 0xBEDF00D 2>/dev/null && echo "[STOP] Shared memory removed." || echo "[STOP] No shared memory to remove."

# Clean up named semaphores
echo "[STOP] Unlinking named semaphores..."
python3 -c "
import ctypes, sys
lib = ctypes.CDLL(None)
for name in [b'/sem_icu_limit', b'/sem_iso_limit']:
    ret = lib.sem_unlink(name)
    print(f'  sem_unlink({name.decode()}): {\"OK\" if ret == 0 else \"not found\"}')
" 2>/dev/null || echo "[STOP] Could not unlink semaphores (may already be removed)."

# Remove FIFO
if [ -p "$DISCHARGE_FIFO" ]; then
    rm -f "$DISCHARGE_FIFO"
    echo "[STOP] Discharge FIFO removed."
fi

# Print log summary
echo ""
echo "============================================"
echo "  FINAL SUMMARY                             "
echo "============================================"

if [ -f "logs/schedule_log.txt" ]; then
    echo "[LOGS] Schedule log: logs/schedule_log.txt"
    tail -5 logs/schedule_log.txt
fi

if [ -f "logs/memory_log.txt" ]; then
    echo ""
    echo "[LOGS] Memory log: logs/memory_log.txt"
    tail -5 logs/memory_log.txt
fi

echo ""
echo "[STOP] Hospital shutdown complete."
echo "============================================"
