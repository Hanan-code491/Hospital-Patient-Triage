/*
 * ============================================================
 * Project : Hospital Patient Triage & Bed Allocator
 * File    : admissions.c
 * Purpose : Central admissions manager - process spawning,
 *           IPC, thread pool, scheduling, and bed allocation.
 * Compile : gcc -Wall -o admissions admissions.c bed_allocator.c scheduling.c -lpthread
 * Usage   : ./admissions [--strategy best|first|worst]
 * ============================================================
 */

#include "hospital.h"
#include "bed_allocator.h"
#include "scheduling.h"

/* ── Globals ── */
static int           g_shmid   = -1;
static SharedWard   *g_ward    = NULL;
static pthread_mutex_t g_bed_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_bed_freed   = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  g_queue_not_empty = PTHREAD_COND_INITIALIZER;

static sem_t *g_sem_icu  = SEM_FAILED;
static sem_t *g_sem_iso  = SEM_FAILED;

/* Patient priority queue (min-heap via sorted insert) */
static QueueNode  g_queue[QUEUE_SIZE];
static int        g_queue_len = 0;
static volatile int g_running = 1;

/* Scheduling log */
static FILE *g_sched_fp = NULL;
static FILE *g_mem_fp   = NULL;

/* Track PIDs for SIGCHLD reaping */
static pid_t g_child_pids[MAX_PATIENTS];
static int   g_child_count = 0;
static pthread_mutex_t g_pid_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Scheduling simulation data */
static SchedEntry g_sched_jobs[MAX_PATIENTS];
static int        g_sched_count = 0;
static pthread_mutex_t g_sched_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── SIGCHLD handler: reap zombie children ── */
static void sigchld_handler(int sig)
{
    (void)sig;
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        /* Remove from our tracked list */
        pthread_mutex_lock(&g_pid_mutex);
        for (int i = 0; i < g_child_count; i++) {
            if (g_child_pids[i] == pid) {
                g_child_pids[i] = g_child_pids[--g_child_count];
                break;
            }
        }
        pthread_mutex_unlock(&g_pid_mutex);
    }
}

/* ── SIGTERM handler: graceful shutdown ── */
static void sigterm_handler(int sig)
{
    (void)sig;
    printf("\n[ADMISSIONS] SIGTERM received – shutting down\n");
    g_running = 0;
    pthread_cond_broadcast(&g_bed_freed);
    pthread_cond_broadcast(&g_queue_not_empty);
}

/* ── Priority queue helpers (sorted insert, min-priority first) ── */
static void enqueue_patient(QueueNode *node)
{
    pthread_mutex_lock(&g_queue_mutex);
    if (g_queue_len >= QUEUE_SIZE) {
        fprintf(stderr, "[QUEUE] Full – patient %d dropped\n", node->patient.patient_id);
        pthread_mutex_unlock(&g_queue_mutex);
        return;
    }
    /* Insert maintaining ascending priority order (1 = highest) */
    int i = g_queue_len++;
    g_queue[i] = *node;
    /* bubble up */
    while (i > 0 && g_queue[i].patient.priority < g_queue[i-1].patient.priority) {
        QueueNode tmp  = g_queue[i];
        g_queue[i]     = g_queue[i-1];
        g_queue[i-1]   = tmp;
        i--;
    }
    printf("[QUEUE]  Enqueued patient %d (%s) priority=%d  queue_len=%d\n",
           node->patient.patient_id, node->patient.name,
           node->patient.priority, g_queue_len);
    pthread_cond_signal(&g_queue_not_empty);
    pthread_mutex_unlock(&g_queue_mutex);
}

static int dequeue_patient(QueueNode *out)
{
    pthread_mutex_lock(&g_queue_mutex);
    while (g_queue_len == 0 && g_running) {
        pthread_cond_wait(&g_queue_not_empty, &g_queue_mutex);
    }
    if (!g_running && g_queue_len == 0) {
        pthread_mutex_unlock(&g_queue_mutex);
        return -1;
    }
    *out = g_queue[0];
    /* Shift left */
    for (int i = 1; i < g_queue_len; i++)
        g_queue[i-1] = g_queue[i];
    g_queue_len--;
    pthread_mutex_unlock(&g_queue_mutex);
    return 0;
}

/* ── Ward initialisation ── */
static void init_ward(SharedWard *w)
{
    memset(w, 0, sizeof(*w));
    int idx = 0, unit = 0;

    /* ICU beds */
    for (int i = 0; i < ICU_COUNT; i++, idx++) {
        w->beds[idx].partition_id = idx;
        w->beds[idx].start_unit   = unit;
        w->beds[idx].size         = 3;
        w->beds[idx].is_free      = 1;
        w->beds[idx].patient_id   = -1;
        strncpy(w->beds[idx].bed_type, "ICU", 15);
        unit += 3;
    }
    /* Isolation beds */
    for (int i = 0; i < ISOLATION_COUNT; i++, idx++) {
        w->beds[idx].partition_id = idx;
        w->beds[idx].start_unit   = unit;
        w->beds[idx].size         = 2;
        w->beds[idx].is_free      = 1;
        w->beds[idx].patient_id   = -1;
        strncpy(w->beds[idx].bed_type, "ISOLATION", 15);
        unit += 2;
    }
    /* General ward beds */
    for (int i = 0; i < GENERAL_COUNT; i++, idx++) {
        w->beds[idx].partition_id = idx;
        w->beds[idx].start_unit   = unit;
        w->beds[idx].size         = 1;
        w->beds[idx].is_free      = 1;
        w->beds[idx].patient_id   = -1;
        strncpy(w->beds[idx].bed_type, "GENERAL", 15);
        unit += 1;
    }
    w->bed_count = idx;

    /* Init page table */
    for (int i = 0; i < (int)(TOTAL_CARE_UNITS / PAGE_SIZE + 1); i++)
        w->page_table[i] = -1;
}

/* ── Spawn patient_simulator child ── */
static void admit_patient(PatientRecord *p, int bed_id)
{
    char pid_str[16], pri_str[16], bed_str[16];
    snprintf(pid_str, sizeof(pid_str), "%d", p->patient_id);
    snprintf(pri_str, sizeof(pri_str), "%d", p->priority);
    snprintf(bed_str, sizeof(bed_str), "%d", bed_id);

    pid_t pid = fork();
    if (pid < 0) {
        perror("[ADMISSIONS] fork failed");
        return;
    }
    if (pid == 0) {
        /* Child: replace with patient_simulator */
        char *args[] = { "./patient_simulator", pid_str, pri_str, bed_str, p->name, NULL };
        execv("./patient_simulator", args);
        perror("[CHILD] execv failed");
        _exit(1);
    }
    /* Parent: track child PID */
    pthread_mutex_lock(&g_pid_mutex);
    if (g_child_count < MAX_PATIENTS)
        g_child_pids[g_child_count++] = pid;
    pthread_mutex_unlock(&g_pid_mutex);

    printf("[ADMISSIONS] Admitted patient %d (PID=%d) to bed %d\n",
           p->patient_id, pid, bed_id);
}

/* ════════════════════════════════════════════
 * THREAD 1: Receptionist
 * Reads patient records from the discharge FIFO
 * (for arrivals we read from stdin in this demo,
 *  discharge notifications also arrive here).
 * In the real triage flow: triage.sh → pipe → admissions stdin.
 * We read PatientRecord lines from stdin and enqueue them.
 * ════════════════════════════════════════════ */
static void *receptionist_thread(void *arg)
{
    (void)arg;
    printf("[RECEPTIONIST] Thread started\n");

    int next_id = 1;
    char line[256];

    while (g_running) {
        /* Read a line: "name age severity" */
        printf("[RECEPTIONIST] Waiting for patient input (name age severity)...\n");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        if (!g_running) break;

        char name[64];
        int  age, severity;
        if (sscanf(line, "%63s %d %d", name, &age, &severity) != 3) {
            if (strncmp(line, "quit", 4) == 0) { g_running = 0; break; }
            printf("[RECEPTIONIST] Invalid input. Format: name age severity (1-10)\n");
            continue;
        }

        if (severity < 1 || severity > 10) {
            printf("[RECEPTIONIST] Severity must be 1-10\n");
            continue;
        }

        PatientRecord p;
        p.patient_id   = next_id++;
        strncpy(p.name, name, 63);
        p.age          = age;
        p.severity     = severity;
        p.priority     = severity_to_priority(severity);
        p.care_units   = priority_to_care_units(p.priority);
        p.arrival_time = time(NULL);

        printf("[RECEPTIONIST] New patient: ID=%d %s age=%d sev=%d → priority=%d type=%s\n",
               p.patient_id, p.name, p.age, p.severity,
               p.priority, priority_to_bed_type(p.priority));

        /* Acquire semaphore based on bed type (blocks if full) */
        sem_t *sem = NULL;
        if (p.priority <= 2)      sem = g_sem_icu;
        else if (p.priority == 3) sem = g_sem_iso;

        if (sem != SEM_FAILED && sem != NULL) {
            printf("[SEM] Waiting for %s semaphore...\n",
                   p.priority <= 2 ? "ICU" : "ISOLATION");
            sem_wait(sem);
            printf("[SEM] %s semaphore acquired for patient %d\n",
                   p.priority <= 2 ? "ICU" : "ISOLATION", p.patient_id);
        }

        QueueNode node;
        node.patient    = p;
        node.burst_time = (p.priority <= 2) ? (5 + rand() % 11) :
                          (p.priority == 3) ? (3 + rand() % 8)  :
                                              (2 + rand() % 7);
        enqueue_patient(&node);

        /* Record for scheduling simulation */
        pthread_mutex_lock(&g_sched_mutex);
        if (g_sched_count < MAX_PATIENTS) {
            SchedEntry *e = &g_sched_jobs[g_sched_count++];
            e->patient_id   = p.patient_id;
            e->priority     = p.priority;
            e->burst_time   = node.burst_time;
            e->arrival_time = 0;   /* relative */
            strncpy(e->name, p.name, 63);
        }
        pthread_mutex_unlock(&g_sched_mutex);
    }

    printf("[RECEPTIONIST] Thread exiting\n");
    return NULL;
}

/* ════════════════════════════════════════════
 * THREAD 2: Scheduler
 * Dequeues highest-priority patient, finds a bed
 * using Best-Fit, then fork()+exec() the patient.
 * ════════════════════════════════════════════ */
static void *scheduler_thread(void *arg)
{
    (void)arg;
    printf("[SCHEDULER] Thread started\n");

    while (g_running) {
        QueueNode node;
        if (dequeue_patient(&node) < 0) break;

        PatientRecord *p = &node.patient;

        /* Try to find a bed; wait on condition if none available */
        int bed_id = -1;
        pthread_mutex_lock(&g_bed_mutex);
        while ((bed_id = allocate_bed(g_ward, p)) < 0 && g_running) {
            printf("[SCHEDULER] No bed for patient %d (%s) – waiting...\n",
                   p->patient_id, p->name);
            pthread_cond_wait(&g_bed_freed, &g_bed_mutex);
        }

        if (!g_running) {
            pthread_mutex_unlock(&g_bed_mutex);
            break;
        }

        print_ward_map(g_ward);
        report_fragmentation(g_ward, g_mem_fp);
        report_paging(g_ward, p, bed_id, g_mem_fp);
        pthread_mutex_unlock(&g_bed_mutex);

        admit_patient(p, bed_id);
        g_ward->total_patients_served++;
    }

    printf("[SCHEDULER] Thread exiting\n");
    return NULL;
}

/* ════════════════════════════════════════════
 * THREAD 3+: Nurse threads (one per bed type)
 * Monitor the discharge FIFO; when a patient
 * finishes treatment, free the bed and signal
 * the scheduler.
 * ════════════════════════════════════════════ */
typedef struct { const char *bed_type; int priority_max; } NurseArg;

static void *nurse_thread(void *arg)
{
    NurseArg *na = (NurseArg *)arg;
    printf("[NURSE-%s] Thread started\n", na->bed_type);

    int fd = open(DISCHARGE_FIFO, O_RDONLY);
    if (fd < 0) {
        perror("[NURSE] open FIFO");
        return NULL;
    }

    char buf[32];
    while (g_running) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            if (!g_running) break;
            usleep(100000);
            continue;
        }
        buf[n] = '\0';

        int patient_id = atoi(buf);
        if (patient_id <= 0) continue;

        printf("[NURSE-%s] Discharge notification for patient %d\n",
               na->bed_type, patient_id);

        /* Check this nurse handles this patient's bed type */
        pthread_mutex_lock(&g_bed_mutex);
        int found = 0;
        for (int i = 0; i < g_ward->bed_count; i++) {
            if (g_ward->beds[i].patient_id == patient_id &&
                strcmp(g_ward->beds[i].bed_type, na->bed_type) == 0)
            {
                found = 1;
                break;
            }
        }

        if (found) {
            printf("[WARD] Before coalescing:\n");
            print_ward_map(g_ward);

            free_bed(g_ward, patient_id);
            coalesce_free(g_ward);
            report_fragmentation(g_ward, g_mem_fp);

            printf("[WARD] After coalescing:\n");
            print_ward_map(g_ward);

            /* Release semaphore for this bed type */
            if (strcmp(na->bed_type, "ICU") == 0 && g_sem_icu != SEM_FAILED)
                sem_post(g_sem_icu);
            else if (strcmp(na->bed_type, "ISOLATION") == 0 && g_sem_iso != SEM_FAILED)
                sem_post(g_sem_iso);

            pthread_cond_broadcast(&g_bed_freed);
        }
        pthread_mutex_unlock(&g_bed_mutex);
    }

    close(fd);
    printf("[NURSE-%s] Thread exiting\n", na->bed_type);
    return NULL;
}

/* ── Run scheduling simulation on all recorded jobs ── */
static void run_scheduling_simulation(void)
{
    pthread_mutex_lock(&g_sched_mutex);
    int n = g_sched_count;
    SchedEntry jobs[MAX_PATIENTS];
    memcpy(jobs, g_sched_jobs, n * sizeof(SchedEntry));
    pthread_mutex_unlock(&g_sched_mutex);

    if (n == 0) {
        printf("[SCHED] No patients to simulate.\n");
        return;
    }

    printf("\n[SCHED] Running scheduling simulation on %d patient(s)...\n", n);

    SchedEntry copy[MAX_PATIENTS];

    memcpy(copy, jobs, n * sizeof(SchedEntry));
    simulate_fcfs(copy, n, g_sched_fp);

    memcpy(copy, jobs, n * sizeof(SchedEntry));
    simulate_priority(copy, n, g_sched_fp);

    memcpy(copy, jobs, n * sizeof(SchedEntry));
    simulate_sjf(copy, n, g_sched_fp);

    memcpy(copy, jobs, n * sizeof(SchedEntry));
    simulate_rr(copy, n, 3, g_sched_fp);
}

/* ══════════════════════════════════════════════
 * main()
 * ══════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    /* Parse --strategy flag */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--strategy") == 0 && i + 1 < argc) {
            i++;
            if      (strcmp(argv[i], "first") == 0) g_strategy = FIRST_FIT;
            else if (strcmp(argv[i], "worst") == 0) g_strategy = WORST_FIT;
            else                                     g_strategy = BEST_FIT;
        }
    }

    const char *strat_name[] = { "Best-Fit", "First-Fit", "Worst-Fit" };
    printf("═══════════════════════════════════════════\n");
    printf("  Hospital Patient Triage & Bed Allocator  \n");
    printf("  Strategy : %s\n", strat_name[g_strategy]);
    printf("  ICU:%d  Isolation:%d  General:%d\n",
           ICU_COUNT, ISOLATION_COUNT, GENERAL_COUNT);
    printf("═══════════════════════════════════════════\n\n");

    /* Signals */
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    /* Open log files */
    g_sched_fp = fopen("logs/schedule_log.txt", "w");
    g_mem_fp   = fopen("logs/memory_log.txt",   "w");
    if (!g_sched_fp) g_sched_fp = stderr;
    if (!g_mem_fp)   g_mem_fp   = stderr;
    fprintf(g_sched_fp, "=== Schedule Log – Hospital Triage ===\n\n");
    fprintf(g_mem_fp,   "=== Memory Log  – Hospital Triage ===\n\n");

    /* Shared memory */
    g_shmid = shmget(SHM_KEY, sizeof(SharedWard), IPC_CREAT | 0666);
    if (g_shmid < 0) { perror("shmget"); return 1; }
    g_ward = (SharedWard *)shmat(g_shmid, NULL, 0);
    if (g_ward == (void *)-1) { perror("shmat"); return 1; }
    init_ward(g_ward);
    printf("[SHM] Shared memory created (key=0x%X, size=%zu bytes)\n",
           SHM_KEY, sizeof(SharedWard));

    /* Named semaphores */
    sem_unlink(SEM_ICU); sem_unlink(SEM_ISO);
    g_sem_icu = sem_open(SEM_ICU, O_CREAT | O_EXCL, 0666, ICU_COUNT);
    g_sem_iso = sem_open(SEM_ISO, O_CREAT | O_EXCL, 0666, ISOLATION_COUNT);
    if (g_sem_icu == SEM_FAILED || g_sem_iso == SEM_FAILED) {
        perror("sem_open"); return 1;
    }
    printf("[SEM] ICU semaphore (cap=%d) and Isolation semaphore (cap=%d) created\n",
           ICU_COUNT, ISOLATION_COUNT);

    /* Named FIFO */
    unlink(DISCHARGE_FIFO);
    if (mkfifo(DISCHARGE_FIFO, 0666) < 0) { perror("mkfifo"); return 1; }
    printf("[FIFO] Discharge FIFO created at %s\n\n", DISCHARGE_FIFO);

    print_ward_map(g_ward);

    /* ── Start threads ── */
    pthread_t t_receptionist, t_scheduler;
    pthread_t t_nurse_icu, t_nurse_iso, t_nurse_gen;

    static NurseArg na_icu  = { "ICU",       2 };
    static NurseArg na_iso  = { "ISOLATION", 3 };
    static NurseArg na_gen  = { "GENERAL",   5 };

    pthread_create(&t_nurse_icu,    NULL, nurse_thread,       &na_icu);
    pthread_create(&t_nurse_iso,    NULL, nurse_thread,       &na_iso);
    pthread_create(&t_nurse_gen,    NULL, nurse_thread,       &na_gen);
    pthread_create(&t_scheduler,    NULL, scheduler_thread,   NULL);
    pthread_create(&t_receptionist, NULL, receptionist_thread, NULL);

    /* Wait for receptionist (blocks until user quits) */
    pthread_join(t_receptionist, NULL);

    /* Signal everything to stop */
    g_running = 0;
    pthread_cond_broadcast(&g_bed_freed);
    pthread_cond_broadcast(&g_queue_not_empty);

    pthread_join(t_scheduler,  NULL);
    pthread_join(t_nurse_icu,  NULL);
    pthread_join(t_nurse_iso,  NULL);
    pthread_join(t_nurse_gen,  NULL);

    /* Run scheduling simulation on recorded jobs */
    run_scheduling_simulation();

    /* Final stats */
    printf("\n[SUMMARY] Total patients served: %d\n",
           g_ward->total_patients_served);
    print_ward_map(g_ward);
    report_fragmentation(g_ward, g_mem_fp);

    /* Cleanup */
    shmdt(g_ward);
    shmctl(g_shmid, IPC_RMID, NULL);
    sem_close(g_sem_icu);  sem_unlink(SEM_ICU);
    sem_close(g_sem_iso);  sem_unlink(SEM_ISO);
    unlink(DISCHARGE_FIFO);

    if (g_sched_fp && g_sched_fp != stderr) fclose(g_sched_fp);
    if (g_mem_fp   && g_mem_fp   != stderr) fclose(g_mem_fp);

    printf("[ADMISSIONS] Shutdown complete.\n");
    return 0;
}
