#!/bin/bash
# ============================================================
# Project : Hospital Patient Triage & Bed Allocator
# Script  : start_hospital.sh
# Purpose : Initialize IPC resources and launch the
#           admissions manager process in background.
# Usage   : ./start_hospital.sh [--strategy best|first|worst]
# ============================================================

STRATEGY="${2:-best}"
PID_FILE="/tmp/hospital_admissions.pid"
DISCHARGE_FIFO="/tmp/discharge_fifo"

echo "============================================"
echo "  HOSPITAL PATIENT TRIAGE & BED ALLOCATOR  "
echo "  Starting up...                            "
echo "  Strategy : $STRATEGY                      "
echo "  Ward     : ICU=4  Isolation=4  General=12 "
echo "  Total Care Units: 28                      "
echo "============================================"

# Remove stale IPC resources if they exist
ipcrm -M 0xBEDF00D 2>/dev/null
sem_unlink_cmd="python3 -c 'import ctypes; lib=ctypes.CDLL(None); lib.sem_unlink(b\"/sem_icu_limit\"); lib.sem_unlink(b\"/sem_iso_limit\")'"
eval "$sem_unlink_cmd" 2>/dev/null || true

# Remove stale FIFO
rm -f "$DISCHARGE_FIFO"

# Check binary exists
if [ ! -f "./admissions" ]; then
    echo "ERROR: ./admissions binary not found. Run 'make all' first."
    exit 1
fi

# Launch admissions in background
./admissions --strategy "$STRATEGY" &
ADMISSIONS_PID=$!
echo $ADMISSIONS_PID > "$PID_FILE"

echo "[START] Admissions manager launched (PID=$ADMISSIONS_PID)"
echo "[START] PID saved to $PID_FILE"
echo "[START] Hospital is OPEN. Use triage.sh to register patients."
echo ""
echo "  To admit a patient:  ./scripts/triage.sh <name> <age> <severity>"
echo "  To stop:             ./scripts/stop_hospital.sh"
echo "============================================"
