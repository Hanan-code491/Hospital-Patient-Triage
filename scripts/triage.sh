#!/bin/bash
# ============================================================
# Project : Hospital Patient Triage & Bed Allocator
# Script  : triage.sh
# Purpose : Compute triage priority and pipe patient data
#           to the admissions manager process.
# Usage   : ./triage.sh <name> <age> <severity 1-10>
# ============================================================

# ── Input validation ──────────────────────────────────────

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <name> <age> <severity 1-10>"
    exit 1
fi

NAME="$1"
AGE="$2"
SEVERITY="$3"

# Non-empty name
if [ -z "$NAME" ]; then
    echo "ERROR: Patient name cannot be empty."
    exit 1
fi

# Numeric age
if ! [[ "$AGE" =~ ^[0-9]+$ ]]; then
    echo "ERROR: Age must be a positive integer."
    exit 1
fi

if [ "$AGE" -lt 0 ] || [ "$AGE" -gt 150 ]; then
    echo "ERROR: Age must be between 0 and 150."
    exit 1
fi

# Numeric severity
if ! [[ "$SEVERITY" =~ ^[0-9]+$ ]]; then
    echo "ERROR: Severity must be a number between 1 and 10."
    exit 1
fi

if [ "$SEVERITY" -lt 1 ] || [ "$SEVERITY" -gt 10 ]; then
    echo "ERROR: Severity must be between 1 and 10."
    exit 1
fi

# ── Compute triage priority ───────────────────────────────
#  Severity 9-10 → Priority 1 (Critical)
#  Severity 7-8  → Priority 2 (Urgent)
#  Severity 5-6  → Priority 3 (Less Urgent)
#  Severity 3-4  → Priority 4 (Non-Urgent)
#  Severity 1-2  → Priority 5 (Minor)

if   [ "$SEVERITY" -ge 9 ]; then PRIORITY=1; BED_TYPE="ICU"
elif [ "$SEVERITY" -ge 7 ]; then PRIORITY=2; BED_TYPE="ICU"
elif [ "$SEVERITY" -ge 5 ]; then PRIORITY=3; BED_TYPE="ISOLATION"
elif [ "$SEVERITY" -ge 3 ]; then PRIORITY=4; BED_TYPE="GENERAL"
else                              PRIORITY=5; BED_TYPE="GENERAL"
fi

TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")

echo "============================================"
echo "  TRIAGE ASSESSMENT"
echo "  Time     : $TIMESTAMP"
echo "  Name     : $NAME"
echo "  Age      : $AGE"
echo "  Severity : $SEVERITY / 10"
echo "  Priority : $PRIORITY (1=Critical, 5=Minor)"
echo "  Bed Type : $BED_TYPE"
echo "============================================"

# ── Send patient record to admissions via stdin (pipe) ────
# Format expected by receptionist_thread: "name age severity"
echo "$NAME $AGE $SEVERITY"
