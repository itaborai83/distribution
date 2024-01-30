#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include <stdbool.h>
#include "runtime.h"

typedef struct {
    int alpha;
    int count;
} bin_t;

typedef struct {
    runtime_t *rt;
    int base;
    int exponent;
    int count;
    int bin_count;
    bin_t bins[BIN_COUNT];
} histogram_t;

typedef struct {
    int bin_count;
    double pcts[BIN_COUNT];
    double values[BIN_COUNT];
} percentiles_t;

typedef int retcode_t;

retcode_t hst_init(runtime_t *rt, histogram_t *hst, int base, int exponent);
extern retcode_t hst_destroy(histogram_t *hst);
extern retcode_t hst_update(histogram_t *hst, double value);
extern retcode_t hst_display(histogram_t *hst, FILE *fp);
extern retcode_t hst_get_percentiles(histogram_t *hst, percentiles_t *pcts);

/*
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
*/
#endif // DISTRIBUTION_H