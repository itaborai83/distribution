#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <stdbool.h>
#include "runtime.h"

typedef struct {
    int alpha;
    int count;
} bin_t;

typedef struct {
    char header[4]; 
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
extern retcode_t hst_debug(histogram_t *hst, FILE *fp);
extern retcode_t hst_display(histogram_t *hst, FILE *fp);
retcode_t hst_display_percentiles(histogram_t *hst, FILE *fp, double precision);
extern retcode_t hst_save(histogram_t *hst, FILE *fp);
extern retcode_t hst_load(runtime_t *rt, histogram_t *hst, FILE *fp);
extern retcode_t hst_get_percentiles(histogram_t *hst, percentiles_t *pcts);
extern retcode_t hst_get_percentile(histogram_t *hst, percentiles_t *pcts, double pct, double *value);
#endif // HISTOGRAM_H