# ============================================================
# Project : Hospital Patient Triage & Bed Allocator
# File    : Makefile
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -pthread
SRCDIR  = src
LOGDIR  = logs

ADMISSIONS_SRCS = $(SRCDIR)/admissions.c $(SRCDIR)/bed_allocator.c $(SRCDIR)/scheduling.c
PATIENT_SRCS    = $(SRCDIR)/patient_simulator.c

.PHONY: all clean run test

all: admissions patient_simulator
	@echo "Build complete."

admissions: $(ADMISSIONS_SRCS) $(SRCDIR)/hospital.h $(SRCDIR)/bed_allocator.h $(SRCDIR)/scheduling.h
	$(CC) $(CFLAGS) -o admissions $(ADMISSIONS_SRCS)

patient_simulator: $(PATIENT_SRCS) $(SRCDIR)/hospital.h
	$(CC) $(CFLAGS) -o patient_simulator $(PATIENT_SRCS)

run: all
	@mkdir -p $(LOGDIR)
	@chmod +x scripts/*.sh
	./admissions --strategy best

test: all
	@mkdir -p $(LOGDIR)
	@chmod +x scripts/*.sh
	@echo "Running basic smoke test..."
	@echo "TestPatient 30 9" | timeout 20 ./admissions --strategy best || true
	@echo "Smoke test complete. Check logs/ for output."

clean:
	rm -f admissions patient_simulator
	rm -f /tmp/discharge_fifo
	rm -f /tmp/hospital_admissions.pid
	ipcrm -M 0xBEDF00D 2>/dev/null || true
	@echo "Clean complete."
