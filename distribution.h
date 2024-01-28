#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include <stdbool.h>
#include "runtime.h"

// typedefs
typedef struct {
    runtime_t *rt;    
    int count;
    int bin_count_a;
    int bin_count_b;
    int generation;
    double bins_a[BIN_COUNT];
    double bins_b[BIN_COUNT];
} distribution_t;

extern void init_distribution(runtime_t *rt, distribution_t *dist);
extern void copy_distribution(distribution_t *src, distribution_t *dst);
extern int update_distribution(distribution_t *dist, double value);
extern void display_distribution(distribution_t *dist, FILE *fp);
extern int get_percentile(distribution_t *dist, double pct, double *value);

#endif // DISTRIBUTION_H