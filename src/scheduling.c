/*
 * ============================================================
 * Project : Hospital Patient Triage & Bed Allocator
 * File    : scheduling.c
 * Purpose : FCFS, SJF, Priority, Round-Robin simulations
 *           Outputs Gantt-style log to schedule_log.txt
 * ============================================================
 */

#include "scheduling.h"
#include <string.h>

/* ── helpers ── */

static void reset_times(SchedEntry *jobs, int n) {
    for (int i = 0; i < n; i++) {
        jobs[i].waiting_time    = 0;
        jobs[i].turnaround_time = 0;
        jobs[i].finish_time     = 0;
    }
}

void print_sched_metrics(SchedEntry *jobs, int n, FILE *fp, const char *algo_name)
{
    double avg_wt = 0, avg_tat = 0;
    for (int i = 0; i < n; i++) {
        avg_wt  += jobs[i].waiting_time;
        avg_tat += jobs[i].turnaround_time;
    }
    avg_wt  /= n;
    avg_tat /= n;

    printf("[SCHED] %s → Avg Wait=%.2fs  Avg Turnaround=%.2fs\n",
           algo_name, avg_wt, avg_tat);
    if (fp) {
        fprintf(fp, "\n=== %s Metrics ===\n", algo_name);
        fprintf(fp, "%-12s %-6s %-6s %-6s %-6s\n",
                "Patient", "Burst", "Wait", "TAT", "Finish");
        for (int i = 0; i < n; i++)
            fprintf(fp, "%-12s %-6d %-6d %-6d %-6d\n",
                    jobs[i].name, jobs[i].burst_time,
                    jobs[i].waiting_time, jobs[i].turnaround_time,
                    jobs[i].finish_time);
        fprintf(fp, "Average Wait=%.2f  Average Turnaround=%.2f\n\n",
                avg_wt, avg_tat);
        fflush(fp);
    }
}

/* ─────────────────────────────────────────────
 * FCFS
 * ───────────────────────────────────────────── */
void simulate_fcfs(SchedEntry *jobs, int n, FILE *fp)
{
    reset_times(jobs, n);
    int time = 0;
    if (fp) fprintf(fp, "\n=== FCFS Gantt ===\n");

    for (int i = 0; i < n; i++) {
        if (time < jobs[i].arrival_time) time = jobs[i].arrival_time;
        jobs[i].waiting_time    = time - jobs[i].arrival_time;
        jobs[i].finish_time     = time + jobs[i].burst_time;
        jobs[i].turnaround_time = jobs[i].finish_time - jobs[i].arrival_time;

        if (fp) fprintf(fp, "t=%d  [P%d %-10s burst=%d]\n",
                        time, jobs[i].patient_id, jobs[i].name, jobs[i].burst_time);
        time = jobs[i].finish_time;
    }
    print_sched_metrics(jobs, n, fp, "FCFS");
}

/* ─────────────────────────────────────────────
 * SJF (non-preemptive)
 * ───────────────────────────────────────────── */
void simulate_sjf(SchedEntry *jobs, int n, FILE *fp)
{
    reset_times(jobs, n);
    /* sort by burst_time */
    SchedEntry sorted[MAX_PATIENTS];
    memcpy(sorted, jobs, n * sizeof(SchedEntry));
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (sorted[i].burst_time > sorted[j].burst_time) {
                SchedEntry tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }

    int time = 0;
    if (fp) fprintf(fp, "\n=== SJF Gantt ===\n");
    for (int i = 0; i < n; i++) {
        if (time < sorted[i].arrival_time) time = sorted[i].arrival_time;
        sorted[i].waiting_time    = time - sorted[i].arrival_time;
        sorted[i].finish_time     = time + sorted[i].burst_time;
        sorted[i].turnaround_time = sorted[i].finish_time - sorted[i].arrival_time;

        if (fp) fprintf(fp, "t=%d  [P%d %-10s burst=%d]\n",
                        time, sorted[i].patient_id, sorted[i].name, sorted[i].burst_time);
        time = sorted[i].finish_time;
    }
    /* copy results back */
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (jobs[j].patient_id == sorted[i].patient_id) {
                jobs[j].waiting_time    = sorted[i].waiting_time;
                jobs[j].finish_time     = sorted[i].finish_time;
                jobs[j].turnaround_time = sorted[i].turnaround_time;
            }
    print_sched_metrics(jobs, n, fp, "SJF");
}

/* ─────────────────────────────────────────────
 * Priority (triage level, lower = higher priority)
 * ───────────────────────────────────────────── */
void simulate_priority(SchedEntry *jobs, int n, FILE *fp)
{
    reset_times(jobs, n);
    SchedEntry sorted[MAX_PATIENTS];
    memcpy(sorted, jobs, n * sizeof(SchedEntry));
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (sorted[i].priority > sorted[j].priority) {
                SchedEntry tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }

    int time = 0;
    if (fp) fprintf(fp, "\n=== Priority Scheduling Gantt ===\n");
    for (int i = 0; i < n; i++) {
        if (time < sorted[i].arrival_time) time = sorted[i].arrival_time;
        sorted[i].waiting_time    = time - sorted[i].arrival_time;
        sorted[i].finish_time     = time + sorted[i].burst_time;
        sorted[i].turnaround_time = sorted[i].finish_time - sorted[i].arrival_time;

        if (fp) fprintf(fp, "t=%d  [P%d %-10s pri=%d burst=%d]\n",
                        time, sorted[i].patient_id, sorted[i].name,
                        sorted[i].priority, sorted[i].burst_time);
        time = sorted[i].finish_time;
    }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (jobs[j].patient_id == sorted[i].patient_id) {
                jobs[j].waiting_time    = sorted[i].waiting_time;
                jobs[j].finish_time     = sorted[i].finish_time;
                jobs[j].turnaround_time = sorted[i].turnaround_time;
            }
    print_sched_metrics(jobs, n, fp, "Priority");
}

/* ─────────────────────────────────────────────
 * Round Robin
 * ───────────────────────────────────────────── */
void simulate_rr(SchedEntry *jobs, int n, int quantum, FILE *fp)
{
    reset_times(jobs, n);
    int remaining[MAX_PATIENTS];
    for (int i = 0; i < n; i++) remaining[i] = jobs[i].burst_time;

    int time = 0, done = 0;
    if (fp) fprintf(fp, "\n=== Round Robin (quantum=%d) Gantt ===\n", quantum);

    while (done < n) {
        int progress = 0;
        for (int i = 0; i < n; i++) {
            if (remaining[i] <= 0) continue;
            progress = 1;
            int run = remaining[i] < quantum ? remaining[i] : quantum;
            if (fp) fprintf(fp, "t=%d  [P%d %-10s run=%d rem=%d]\n",
                            time, jobs[i].patient_id, jobs[i].name,
                            run, remaining[i] - run);
            time           += run;
            remaining[i]   -= run;
            if (remaining[i] == 0) {
                jobs[i].finish_time     = time;
                jobs[i].turnaround_time = time - jobs[i].arrival_time;
                jobs[i].waiting_time    = jobs[i].turnaround_time - jobs[i].burst_time;
                done++;
            }
        }
        if (!progress) break;
    }
    print_sched_metrics(jobs, n, fp, "RoundRobin");
}
