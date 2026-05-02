# Hospital Patient Triage & Bed Allocator

**CL2006 – Operating Systems Lab**
**Spring 2026 | FAST-NUCES**

---

## Overview

This project is a **system-level simulation of a hospital emergency room** implemented in C.
It models real-world workflows such as **patient admission, triage, bed allocation, treatment, and discharge**, while demonstrating core **Operating Systems concepts**.

The system integrates:

* Process creation and management
* Inter-Process Communication (IPC)
* Multithreading and synchronization
* CPU scheduling algorithms
* Memory allocation strategies

---

## Features

* Interactive patient admission system
* Severity-based triage handling
* Multiple bed allocation strategies:

  * First-Fit
  * Best-Fit
  * Worst-Fit
* Concurrent handling using threads
* IPC using pipes, FIFO, and shared memory
* Scheduling simulation after shutdown
* Logging for memory and scheduling analysis

---

## Build Instructions

```bash
make all      # Compile all binaries
make run      # Build and run with default (Best-Fit)
make test     # Run smoke test
make clean    # Remove binaries and IPC artifacts
```

---

## Usage

### Start the System

```bash
./admissions --strategy best
```

### Available Strategies

```bash
./admissions --strategy first
./admissions --strategy best
./admissions --strategy worst
```

---

### Input Format

Enter patient details in the following format:

```
<name> <age> <severity>
```

### Example

```
Ahmed 35 9
Sara 22 5
Omar 45 2
quit
```

* Higher severity → higher priority
* `quit` → terminates input and starts scheduling simulation

---

## System Architecture

### Components

* **Admissions Process**

  * Accepts input
  * Creates child processes for triage
* **Triage Script**

  * Validates patient data
  * Sends output via pipe
* **Shared Memory**

  * Stores ward/bed information
* **Scheduler Thread**

  * Allocates beds based on strategy
* **Nurse Threads**

  * Handle discharge events via FIFO

---

## IPC Mechanisms Used

| Mechanism         | Purpose                           |
| ----------------- | --------------------------------- |
| Pipe              | Triage → Admissions communication |
| FIFO (Named Pipe) | Discharge notifications           |
| Shared Memory     | Global ward state                 |
| Signals           | Child process handling            |

---

## Threading Model

| Thread       | Responsibility        |
| ------------ | --------------------- |
| Receptionist | Accepts patient input |
| Scheduler    | Assigns beds          |
| Nurse (×3)   | Handles discharge     |

---

## OS Concepts Implemented

| Concept              | Description                      |
| -------------------- | -------------------------------- |
| `fork()` / `execv()` | Process creation for triage      |
| `SIGCHLD`            | Non-blocking child cleanup       |
| Mutex                | Protects shared bed data         |
| Condition Variable   | Waits for bed availability       |
| Semaphores           | ICU / Isolation limits           |
| Priority Queue       | Patient scheduling               |
| Memory Allocation    | First-Fit, Best-Fit, Worst-Fit   |
| Fragmentation        | Internal & External tracking     |
| CPU Scheduling       | FCFS, SJF, Priority, Round Robin |

---

## Memory Management

* Dynamic partition allocation
* Coalescing of free blocks
* External fragmentation calculation
* Paging simulation for internal fragmentation

---

## Scheduling Algorithms

* First Come First Serve (FCFS)
* Shortest Job First (SJF)
* Priority Scheduling
* Round Robin (RR)

---

## Project Structure

```
.
├── src/
│   ├── admissions.c
│   ├── bed_allocator.c
│   ├── scheduling.c
│   ├── patient_simulator.c
│   ├── hospital.h
│   ├── bed_allocator.h
│   └── scheduling.h
├── scripts/
│   ├── triage.sh
│   ├── start_hospital.sh
│   ├── stop_hospital.sh
│   └── stress_test.sh
├── logs/
├── Makefile
└── README.md
```

---

## Logs

| File                    | Description                   |
| ----------------------- | ----------------------------- |
| `logs/schedule_log.txt` | Scheduling timeline + metrics |
| `logs/memory_log.txt`   | Fragmentation statistics      |

---

## Testing

### Run Smoke Test

```bash
make test
```

### Stress Test

```bash
./scripts/stress_test.sh
```

---

## Memory Leak Detection

```bash
valgrind --leak-check=full ./admissions --strategy best
```

---

## Requirements

* GCC with POSIX thread support
* Linux environment (Ubuntu recommended)
* Bash shell

---

## Key Highlights

* Combines **processes + threads + IPC** in one system
* Demonstrates **real-world OS problem solving**
* Includes **multiple scheduling and allocation strategies**
* Provides **logging and testing utilities**

---

## Author

**Ammar**
BS Computer Science – FAST-NUCES

---

If you want an even higher-tier submission, I can next:

* add a **diagram (looks amazing in README)**
* or compress this into a **1-minute viva explanation script** (super useful for lab exams)
