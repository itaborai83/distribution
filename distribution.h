#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include <stdbool.h>
#include "runtime.h"

// typedefs
typedef struct {
    runtime_t *rt;    
    int count;
    int bin_count;
    double bins[BIN_COUNT];
    int merges[BIN_COUNT];
    int curr_gap;
    int last_insert_idx;
} distribution_t;

typedef struct {
    int count;
    double value;
} histogram_bin_t;

typedef struct {
    int bin_count;    
    histogram_bin_t *bins;
} Histogram;

extern void init_distribution(runtime_t *rt, distribution_t *dist);
extern void copy_distribution(distribution_t *src, distribution_t *dst);
extern int update_distribution(distribution_t *dist, double value);
extern void display_distribution(distribution_t *dist, FILE *fp);
extern int get_percentile(distribution_t *dist, double pct, double *value);

#endif // DISTRIBUTION_H