#!/bin/bash
# ============================================================
# Project : Hospital Patient Triage & Bed Allocator
# Script  : stress_test.sh
# Purpose : Automated stress test – spawns 20 rapid patient
#           arrivals to test concurrent process handling.
# Usage   : Run AFTER admissions is started in another terminal
# ============================================================

NAMES=("Ali" "Sara" "Ahmed" "Fatima" "Hassan" "Ayesha" "Omar" "Zara"
       "Bilal" "Nadia" "Usman" "Hina" "Tariq" "Sana" "Imran" "Maryam"
       "Kamran" "Rabia" "Faisal" "Sadia")

echo "============================================"
echo "  STRESS TEST – 20 rapid patient arrivals   "
echo "============================================"

for i in $(seq 0 19); do
    NAME="${NAMES[$i]}"
    AGE=$((20 + RANDOM % 60))
    SEVERITY=$((1 + RANDOM % 10))
    echo "[STRESS] Sending: $NAME age=$AGE sev=$SEVERITY"
    # In actual use this pipes to admissions stdin; here we just show output
    ./scripts/triage.sh "$NAME" "$AGE" "$SEVERITY"
    sleep 0.2
done

echo ""
echo "[STRESS] All 20 patients submitted."
echo "============================================"
