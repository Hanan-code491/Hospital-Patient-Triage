/*
 * ============================================================
 * Project : Hospital Patient Triage & Bed Allocator
 * File    : bed_allocator.h
 * Purpose : Best-Fit / First-Fit / Worst-Fit allocator,
 *           coalescing, fragmentation reporting
 * ============================================================
 */

#ifndef BED_ALLOCATOR_H
#define BED_ALLOCATOR_H

#include "hospital.h"

/* Allocation strategy selected at runtime */
typedef enum { BEST_FIT, FIRST_FIT, WORST_FIT } AllocStrategy;
extern AllocStrategy g_strategy;

/* ── Allocator ── */

/*
 * allocate_bed()
 * Finds a free BedPartition whose bed_type matches the patient's
 * required type and whose size >= care_units, using the chosen strategy.
 * Returns partition index on success, -1 if no suitable bed found.
 */
int allocate_bed(SharedWard *ward, PatientRecord *p);

/*
 * free_bed()
 * Marks the partition holding patient_id as FREE, then coalesces
 * adjacent free partitions of the same bed_type.
 */
void free_bed(SharedWard *ward, int patient_id);

/* ── Coalescing ── */
void coalesce_free(SharedWard *ward);

/* ── Fragmentation reporting ── */
void report_fragmentation(SharedWard *ward, FILE *log_fp);

/* ── Paging simulation ── */
void report_paging(SharedWard *ward, PatientRecord *p, int partition_id, FILE *log_fp);

/* ── Ward display ── */
void print_ward_map(SharedWard *ward);

#endif /* BED_ALLOCATOR_H */
