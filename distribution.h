#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include <stdbool.h>
#include "runtime.h"

#define BIN_COUNT 100
#define OUTLIER_COUNT 10
#define BIN_RESOLUTION 1000

// typedefs
typedef struct {
    runtime_t *rt;    
    int count;
    int bin_count;
    int left_outlier_count;
    int right_outlier_count;
    double left_outliers[OUTLIER_COUNT];
    double bins[BIN_COUNT];
    double counts[BIN_COUNT];
    double right_outliers[OUTLIER_COUNT];
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
extern int update_left_outliers(distribution_t *dist, double *value, bool *keep_going);
extern int update_right_outliers(distribution_t *dist, double *value, bool *keep_going);
extern bool is_full(distribution_t *dist);
extern int find_closest_bins(distribution_t *dist, double value, bool *exact_match, int *min_idx, int *next_idx);
int find_compaction_point(distribution_t *dist, int *idx);
extern int compact(distribution_t *dist);
extern int update_distribution(distribution_t *dist, double value);
extern void display_distribution(distribution_t *dist);
extern int write_distribution(distribution_t *dist, char *filename);
/*
extern int init_histogram(Histogram **hist, int bin_count);
extern int destroy_histogram(Histogram **hist);
extern int create_histogram(distribution_t *dist, Histogram *hist);
*/
#endif // DISTRIBUTION_H