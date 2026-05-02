/*
 * ============================================================
 * Project : Hospital Patient Triage & Bed Allocator
 * File    : scheduling.h
 * Purpose : CPU scheduling simulation declarations
 * ============================================================
 */

#ifndef SCHEDULING_H
#define SCHEDULING_H

#include "hospital.h"

typedef struct {
    int patient_id;
    int priority;
    int burst_time;
    int arrival_time;   /* relative, seconds from start */
    int waiting_time;
    int turnaround_time;
    int finish_time;
    char name[64];
} SchedEntry;

void simulate_fcfs    (SchedEntry *jobs, int n, FILE *fp);
void simulate_sjf     (SchedEntry *jobs, int n, FILE *fp);
void simulate_priority(SchedEntry *jobs, int n, FILE *fp);
void simulate_rr      (SchedEntry *jobs, int n, int quantum, FILE *fp);

void print_sched_metrics(SchedEntry *jobs, int n, FILE *fp, const char *algo_name);

#endif
