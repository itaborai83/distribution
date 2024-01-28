#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <limits.h>

#include "distribution.h"
#include "logger.h"
#include "runtime.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

bool is_full(distribution_t *dist);
int compact(distribution_t *dist);
double interpolate(double *bins, int start_idx, int end_idx);

void init_distribution(runtime_t *rt, distribution_t *dist) {
    dist->count = 0;
    dist->bin_count_a = 0;
    dist->bin_count_b = 0;
    dist->generation = 0;
    memset(dist->bins_a, 0, sizeof(dist->bins_a));
    memset(dist->bins_b, 0, sizeof(dist->bins_b));
    dist->rt = rt;
}

void copy_distribution(distribution_t *src, distribution_t *dst) {
    dst->rt = src->rt;
    dst->count = src->count;
    dst->bin_count_a = src->bin_count_a;
    dst->bin_count_b = src->bin_count_b;
    dst->generation = src->generation;
    memcpy(dst->bins_a, src->bins_a, sizeof(src->bins_a));
    memcpy(dst->bins_b, src->bins_b, sizeof(src->bins_b));
}

int get_current_bins(distribution_t *dist, int **bin_count, double **bins) {
    if (dist->generation % 2 == 0) {
        *bin_count = &dist->bin_count_a;
        *bins = dist->bins_a;
    } else {
        *bin_count = &dist->bin_count_b;
        *bins = dist->bins_b;
    }
    return EXIT_SUCCESS;
}

bool is_full(distribution_t *dist) {    
    int *bin_count;
    double *bins;
    int retcode = get_current_bins(dist, &bin_count, &bins);
    RT_PANIC(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to get current bins");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    return *bin_count == BIN_COUNT;
}

int insert_value(distribution_t *dist, double value) {    
    int *bin_count;
    double *bins;
    
    int retcode = get_current_bins(dist, &bin_count, &bins);
    RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to get current bins");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    RT_ASSERT(dist->rt, (*bin_count) < BIN_COUNT, "Error: Bin count is greater than default bin count");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }


    // find the position to insert the new element
    int i, j;
    for (i = 0; i < *bin_count; i++) {
        if (bins[i] > value) {
            break;
        }
    }
    
    // shift the elements to make room for the new element
    for (j = *bin_count; j > i; j--) {
        bins[j] = bins[j - 1];
    }
    bins[i] = value;
    (*bin_count)++;
    dist->count++;
    return EXIT_SUCCESS;
}

double interpolate(double *bins, int start_idx, int end_idx) {
    // perform linear interpolation between the current bin and the next bin using the bin index as the x-axis
    double x1 = start_idx;
    double x2 = end_idx;
    double y1 = bins[start_idx];
    double y2 = bins[end_idx];
    double x = (x1 + x2) / 2.0;
    double m = (y2 - y1) / (x2 - x1);
    double y = m * (x - x1) + y1;
    return y;
}

int compact(distribution_t *dist) {
    double new_bins[BIN_COUNT / 2];
    
    int *bin_count;
    double *bins;
    int retcode = get_current_bins(dist, &bin_count, &bins);
    RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to get current bins");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }

    RT_ASSERT(dist->rt, (*bin_count) == BIN_COUNT, "Error: Bin count is not equal to default bin count");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }

    for (int i = 0; i < (*bin_count) / 2; i++) {
        int start_idx = i * 2;
        int end_idx = i * 2 + 1;
        new_bins[i] = interpolate(bins, start_idx, end_idx);
    }
    for (int i = 0; i < (*bin_count); i++) {
        if (i < (*bin_count) / 2) {
            bins[i] = new_bins[i];
        } else {
            bins[i] = 0.0;
        }
    }
    (*bin_count) /= 2;
    return EXIT_SUCCESS;
}

int update_distribution(distribution_t *dist, double value) {
    int retcode;
    if (is_full(dist)) {
        dist->generation++;
        if (dist->generation > 1) { // do not compact the first generation
            // compact the old bins
            retcode = compact(dist);
            RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to compact distribution");
            if (dist->rt->has_error) {
                return EXIT_FAILURE;
            }
        }
    }
        
    retcode = insert_value(dist, value);
    RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to insert value into distribution");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void display_distribution(distribution_t *dist, FILE *fp) {
    fprintf(fp, "Distribution: Count = %d, Bin Count A = %d, Bin Count B = %d, Generation = %d\n", 
        dist->count,
        dist->bin_count_a,
        dist->bin_count_b,
        dist->generation
    );

    fprintf(fp, "    Bins A: \n");
    for (int i = 0; i < dist->bin_count_a; i++) {
        if (i % 20 == 0) {
            fprintf(fp, "    ") ;
        }
        fprintf(fp, "%0.2lf", dist->bins_a[i]);
        if (i % 20 == 19) { 
            fprintf(fp, "\n");
        } else {
            fprintf(fp, " ");
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "    Bins B: \n");
    for (int i = 0; i < dist->bin_count_b; i++) {
        if (i % 20 == 0) {
            fprintf(fp, "    ") ;
        }
        fprintf(fp, "%0.2lf", dist->bins_b[i]);
        if (i % 20 == 19) { 
            fprintf(fp, "\n");
        } else {
            fprintf(fp, " ");
        }
    }
    fprintf(fp, "\n");
}

int get_percentile_aux(runtime_t *rt, double *bins, int bin_count, double pct, double *value) {
    double double_idx = pct * bin_count;
    int idx = (int)double_idx;
    if (idx >= bin_count) {
        *value = bins[bin_count - 1];
        return EXIT_SUCCESS;
    }
    
    *value = bins[idx];
    if (idx == bin_count - 1) {
        return EXIT_SUCCESS;
    }    
    double curr_pct = (idx) / (double)bin_count;
    double next_pct = (idx + 1) / (double)bin_count;
    double pct_range = next_pct - curr_pct;
    double bin_range = bins[idx + 1] - bins[idx];
    double error_pct = (pct - curr_pct) / pct_range;
    double correction_term = error_pct * bin_range;
    *value += correction_term;
    RT_ASSERT(rt, correction_term >= 0.0, "Error: Correction term is negative");
    RT_ASSERT(rt, correction_term <= bin_range, "Error: Correction term is greater than bin range");
    if (rt->has_error) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int get_percentile(distribution_t *dist, double pct, double *value) {
    RT_ASSERT(dist->rt, pct >= 0.0 && pct <= 1.0, "Error: Percentile is out of range");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    if (dist->count == 0) {
        *value = 0.0;
        return EXIT_SUCCESS;
    }
    if (dist->count == 1) {
        *value = dist->bins_a[0];
        return EXIT_SUCCESS;
    }
    
    double result_a;
    int retcode_a = get_percentile_aux(dist->rt, dist->bins_a, dist->bin_count_a, pct, &result_a);
    RT_ASSERT(dist->rt, retcode_a == EXIT_SUCCESS, "Error: Failed to get percentile from bins A");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    double result_b;
    int retcode_b = get_percentile_aux(dist->rt, dist->bins_b, dist->bin_count_b, pct, &result_b);
    RT_ASSERT(dist->rt, retcode_b == EXIT_SUCCESS, "Error: Failed to get percentile from bins B");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    *value = (result_a * dist->bin_count_a + result_b * dist->bin_count_b) / ((double) dist->bin_count_a + dist->bin_count_b);
    return EXIT_SUCCESS;
}