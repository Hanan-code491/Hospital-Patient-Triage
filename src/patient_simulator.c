/*
 * ============================================================
 * Project : Hospital Patient Triage & Bed Allocator
 * File    : patient_simulator.c
 * Purpose : Child process per patient – prints lifecycle msgs,
 *           sleeps (treatment), notifies admissions via FIFO
 * Compile : gcc -Wall -o patient_simulator patient_simulator.c
 * ============================================================
 */

#include "hospital.h"

int main(int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <patient_id> <priority> <bed_id> <name>\n", argv[0]);
        return 1;
    }

    int  patient_id = atoi(argv[1]);
    int  priority   = atoi(argv[2]);
    int  bed_id     = atoi(argv[3]);
    char name[64];
    strncpy(name, argv[4], 63);
    name[63] = '\0';

    /* Determine treatment duration based on bed type */
    int min_sleep, max_sleep;
    const char *bed_type = priority_to_bed_type(priority);

    if (strcmp(bed_type, "ICU") == 0) {
        min_sleep = 5; max_sleep = 15;
    } else if (strcmp(bed_type, "ISOLATION") == 0) {
        min_sleep = 3; max_sleep = 10;
    } else {
        min_sleep = 2; max_sleep = 8;
    }

    srand((unsigned)(time(NULL) ^ getpid()));
    int treatment_time = min_sleep + rand() % (max_sleep - min_sleep + 1);

    printf("[PATIENT %d] %s ARRIVED → Bed %d (%s)\n",
           patient_id, name, bed_id, bed_type);
    fflush(stdout);

    printf("[PATIENT %d] %s TREATMENT STARTED (duration=%ds)\n",
           patient_id, name, treatment_time);
    fflush(stdout);

    sleep(treatment_time);

    printf("[PATIENT %d] %s TREATMENT COMPLETE → discharging\n",
           patient_id, name);
    fflush(stdout);

    /* Notify admissions via named FIFO */
    int fd = open(DISCHARGE_FIFO, O_WRONLY);
    if (fd < 0) {
        perror("[PATIENT] open FIFO");
        return 1;
    }
    char msg[32];
    snprintf(msg, sizeof(msg), "%d\n", patient_id);
    write(fd, msg, strlen(msg));
    close(fd);

    printf("[PATIENT %d] %s DISCHARGED\n", patient_id, name);
    fflush(stdout);

    return 0;
}
