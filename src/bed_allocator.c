/*
 * ============================================================
 * Project : Hospital Patient Triage & Bed Allocator
 * File    : bed_allocator.c
 * Purpose : Best-Fit, First-Fit, Worst-Fit allocators,
 *           coalescing, fragmentation, paging simulation
 * Compile : (included by admissions.c via Makefile)
 * ============================================================
 */

#include "bed_allocator.h"

AllocStrategy g_strategy = BEST_FIT;

/* ─────────────────────────────────────────────
 * allocate_bed()
 * ───────────────────────────────────────────── */
int allocate_bed(SharedWard *ward, PatientRecord *p)
{
    const char *required_type = priority_to_bed_type(p->priority);
    int best_idx  = -1;
    int best_size = -1;

    for (int i = 0; i < ward->bed_count; i++) {
        BedPartition *b = &ward->beds[i];
        if (!b->is_free) continue;
        if (strcmp(b->bed_type, required_type) != 0) continue;
        if (b->size < p->care_units) continue;

        switch (g_strategy) {
        case BEST_FIT:
            /* smallest partition that fits */
            if (best_idx == -1 || b->size < best_size) {
                best_idx  = i;
                best_size = b->size;
            }
            break;
        case FIRST_FIT:
            /* first partition that fits */
            if (best_idx == -1) {
                best_idx  = i;
                best_size = b->size;
            }
            break;
        case WORST_FIT:
            /* largest partition that fits */
            if (best_idx == -1 || b->size > best_size) {
                best_idx  = i;
                best_size = b->size;
            }
            break;
        }
    }

    if (best_idx == -1) return -1;

    /* Mark as occupied */
    BedPartition *chosen = &ward->beds[best_idx];
    chosen->is_free    = 0;
    chosen->patient_id = p->patient_id;

    /* Update page table */
    for (int u = chosen->start_unit;
         u < chosen->start_unit + chosen->size && u / PAGE_SIZE < (int)(TOTAL_CARE_UNITS / PAGE_SIZE + 1);
         u++) {
        ward->page_table[u / PAGE_SIZE] = p->patient_id;
    }

    return best_idx;
}

/* ─────────────────────────────────────────────
 * free_bed()
 * ───────────────────────────────────────────── */
void free_bed(SharedWard *ward, int patient_id)
{
    for (int i = 0; i < ward->bed_count; i++) {
        if (ward->beds[i].patient_id == patient_id) {
            BedPartition *b = &ward->beds[i];

            /* Clear page table entries */
            for (int u = b->start_unit;
                 u < b->start_unit + b->size && u / PAGE_SIZE < (int)(TOTAL_CARE_UNITS / PAGE_SIZE + 1);
                 u++) {
                ward->page_table[u / PAGE_SIZE] = -1;
            }

            printf("[WARD] Freeing bed (partition %d, type %s) for patient %d\n",
                   b->partition_id, b->bed_type, patient_id);

            b->is_free    = 1;
            b->patient_id = -1;
            return;
        }
    }
    fprintf(stderr, "[WARN] free_bed: patient %d not found\n", patient_id);
}

/* ─────────────────────────────────────────────
 * coalesce_free()
 * Merge adjacent free partitions of the same type.
 * ───────────────────────────────────────────── */
void coalesce_free(SharedWard *ward)
{
    int merged = 1;
    while (merged) {
        merged = 0;
        for (int i = 0; i < ward->bed_count - 1; i++) {
            BedPartition *a = &ward->beds[i];
            BedPartition *b = &ward->beds[i + 1];

            if (a->is_free && b->is_free &&
                strcmp(a->bed_type, b->bed_type) == 0 &&
                a->start_unit + a->size == b->start_unit)
            {
                printf("[COALESCE] Merging partition %d (%d units) + partition %d (%d units) → %d units [%s]\n",
                       a->partition_id, a->size,
                       b->partition_id, b->size,
                       a->size + b->size, a->bed_type);

                a->size += b->size;

                /* Shift remaining partitions left */
                for (int j = i + 1; j < ward->bed_count - 1; j++)
                    ward->beds[j] = ward->beds[j + 1];
                ward->bed_count--;
                merged = 1;
                break;
            }
        }
    }
}

/* ─────────────────────────────────────────────
 * report_fragmentation()
 * ───────────────────────────────────────────── */
void report_fragmentation(SharedWard *ward, FILE *log_fp)
{
    int total_free = 0;
    int largest    = 0;

    for (int i = 0; i < ward->bed_count; i++) {
        if (ward->beds[i].is_free) {
            total_free += ward->beds[i].size;
            if (ward->beds[i].size > largest)
                largest = ward->beds[i].size;
        }
    }

    double frag = 0.0;
    if (total_free > 0)
        frag = (1.0 - (double)largest / total_free) * 100.0;

    time_t now = time(NULL);
    char   ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    printf("[FRAG]  Total free=%d  Largest block=%d  Ext-frag=%.1f%%\n",
           total_free, largest, frag);

    if (log_fp) {
        fprintf(log_fp, "[%s] total_free=%d largest=%d ext_frag=%.1f%%\n",
                ts, total_free, largest, frag);
        fflush(log_fp);
    }
}

/* ─────────────────────────────────────────────
 * report_paging()
 * ───────────────────────────────────────────── */
void report_paging(SharedWard *ward, PatientRecord *p, int partition_id, FILE *log_fp)
{
    (void)ward;
    int pages_needed    = (p->care_units + PAGE_SIZE - 1) / PAGE_SIZE;
    int allocated_units = pages_needed * PAGE_SIZE;
    int internal_frag   = allocated_units - p->care_units;

    printf("[PAGING] Patient %d needs %d care-unit(s), pages=%d (size=%d), internal_frag=%d unit(s)\n",
           p->patient_id, p->care_units, pages_needed, PAGE_SIZE, internal_frag);

    if (log_fp) {
        fprintf(log_fp, "[PAGING] patient=%d partition=%d care_units=%d pages=%d int_frag=%d\n",
                p->patient_id, partition_id, p->care_units, pages_needed, internal_frag);
        fflush(log_fp);
    }
}

/* ─────────────────────────────────────────────
 * print_ward_map()
 * ───────────────────────────────────────────── */
void print_ward_map(SharedWard *ward)
{
    printf("\n┌─────────────────────────────────────────┐\n");
    printf("│              WARD MAP                   │\n");
    printf("├────┬──────────┬──────┬──────┬──────────┤\n");
    printf("│ ID │ Type     │ Size │ Free │ Patient  │\n");
    printf("├────┼──────────┼──────┼──────┼──────────┤\n");
    for (int i = 0; i < ward->bed_count; i++) {
        BedPartition *b = &ward->beds[i];
        printf("│ %2d │ %-8s │  %2d  │  %s  │  %6d  │\n",
               b->partition_id, b->bed_type, b->size,
               b->is_free ? "YES" : " NO",
               b->patient_id);
    }
    printf("└────┴──────────┴──────┴──────┴──────────┘\n\n");
}
