/*
 * ============================================================
 * Project : Hospital Patient Triage & Bed Allocator
 * File    : hospital.h
 * Purpose : Shared header - structs, constants, macros
 * ============================================================
 */

#ifndef HOSPITAL_H
#define HOSPITAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/* ── Constants ── */
#define MAX_BEDS        20
#define ICU_COUNT        4
#define ISOLATION_COUNT  4
#define GENERAL_COUNT   12
#define TOTAL_CARE_UNITS 28   /* ICU:4*3 + ISO:4*2 + GEN:12*1 */

#define MAX_PATIENTS     50
#define QUEUE_SIZE       20
#define PAGE_SIZE         2   /* care units per page */

#define SHM_KEY         0xBEDF00D
#define DISCHARGE_FIFO  "/tmp/discharge_fifo"
#define SEM_ICU         "/sem_icu_limit"
#define SEM_ISO         "/sem_iso_limit"
#define SEM_QUEUE_FULL  "/sem_queue_full"
#define SEM_QUEUE_EMPTY "/sem_queue_empty"

/* ── Data Structures (required by manual) ── */

typedef struct {
    int    patient_id;
    char   name[64];
    int    age;
    int    severity;      /* 1–10 raw severity from triage */
    int    priority;      /* 1–5 computed triage priority  */
    int    care_units;    /* memory units required         */
    time_t arrival_time;
} PatientRecord;

typedef struct {
    int  partition_id;
    int  start_unit;      /* index in ward array   */
    int  size;            /* number of care units  */
    int  is_free;         /* 1 = FREE, 0 = OCCUPIED */
    int  patient_id;      /* -1 if free             */
    char bed_type[16];    /* "ICU","GENERAL","ISOLATION" */
} BedPartition;

/* Shared memory layout */
typedef struct {
    BedPartition beds[MAX_BEDS];
    int          bed_count;
    int          total_patients_served;
    /* page table: which patient occupies each page */
    int          page_table[TOTAL_CARE_UNITS / PAGE_SIZE + 1];
} SharedWard;

/* Priority queue node */
typedef struct {
    PatientRecord patient;
    int           burst_time;   /* for SJF simulation */
} QueueNode;

/* ── Inline helpers ── */

static inline int severity_to_priority(int sev) {
    if (sev >= 9) return 1;
    if (sev >= 7) return 2;
    if (sev >= 5) return 3;
    if (sev >= 3) return 4;
    return 5;
}

static inline int priority_to_care_units(int priority) {
    if (priority <= 2) return 3;   /* ICU */
    if (priority == 3) return 2;   /* Isolation */
    return 1;                       /* General */
}

static inline const char *priority_to_bed_type(int priority) {
    if (priority <= 2) return "ICU";
    if (priority == 3) return "ISOLATION";
    return "GENERAL";
}

#endif /* HOSPITAL_H */
