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
int find_compaction_index(distribution_t *dist, int *idx);
int compact(distribution_t *dist);
double interpolate(distribution_t *dist, int start_idx, int end_idx);

void init_distribution(runtime_t *rt, distribution_t *dist) {
    dist->count = 0;
    dist->bin_count = 0;
    dist->curr_gap = BIN_COUNT / 2;
    dist->last_insert_idx = 0;
    for(int i = 0; i < BIN_COUNT; i++) {
        dist->bins[i] = 0.0;
        dist->merges[i] = 0.0;
    }
    dist->rt = rt;
}

void copy_distribution(distribution_t *src, distribution_t *dst) {
    dst->rt = src->rt;
    dst->count = src->count;
    dst->bin_count = src->bin_count;
    dst->curr_gap = src->curr_gap;
    dst->last_insert_idx = src->last_insert_idx;
    memcpy(dst->bins, src->bins, sizeof(src->bins));
    memcpy(dst->merges, src->merges, sizeof(src->merges));
}

bool is_full(distribution_t *dist) {
    return  dist->bin_count == BIN_COUNT;
}

int insert_value(distribution_t *dist, double value) {
    RT_ASSERT(dist->rt, dist->bin_count < BIN_COUNT, "Error: Bin count is greater than default bin count");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    // find the position to insert the new element
    int i, j;
    for (i = 0; i < dist->bin_count; i++) {
        if (dist->bins[i] > value) {
            break;
        }
    }
    
    // shift the elements to make room for the new element
    for (j = dist->bin_count; j > i; j--) {
        dist->bins[j] = dist->bins[j - 1];
        //dist->merges[j] = dist->merges[j - 1];
    }
    dist->bins[i] = value;
    dist->bin_count++;
    if (dist->bin_count == BIN_COUNT) {
        dist->merges[i] += 1; // bump up merge count because it helps with compaction
    }
    dist->last_insert_idx = i;
    return EXIT_SUCCESS;
}

int find_compaction_index(distribution_t *dist, int *idx) {
    int candidate_count = BIN_COUNT / dist->curr_gap;
    double delta = DBL_MIN;
    double min_delta = DBL_MAX;
    *idx = INT_MAX;

    // compute candidate compaction points
    for (int i = 0; i < candidate_count; i++) {
        int compaction_idx = (dist->count + i * dist->curr_gap) % (dist->bin_count - 2);
        RT_PANIC(dist->rt, dist->bins[compaction_idx] <= dist->bins[compaction_idx + 1], "Error: Bins are not sorted");
        int curr_merges = dist->merges[compaction_idx];
        int next_merges = dist->merges[compaction_idx + 1];
        double sum_merges = (double)(curr_merges + next_merges);
        delta = sum_merges / (sum_merges + 1.0);
        if (delta < min_delta) {
            min_delta = delta;
            *idx = compaction_idx;
        }
    }
    bool valid_idx = *idx >= 0 && *idx < dist->bin_count - 1;
    if (!valid_idx) {
        RT_ASSERT(dist->rt, valid_idx, "Error: Compaction index is not in the range [0, bin_count - 1]: %d", *idx);
        LOG_DEBUG("last delta = %0.2lf, min_delta = %0.2lf, idx = %d, bin_count = %d", delta, min_delta, *idx, dist->bin_count);
        if (dist->rt->has_error) {
            return EXIT_FAILURE;
        }
    }
    dist->curr_gap = dist->curr_gap / 2;
    if (dist->curr_gap <= 1) {
        dist->curr_gap = BIN_COUNT / 2;
    }
    return EXIT_SUCCESS;
}

double interpolate(distribution_t *dist, int start_idx, int end_idx) {
    // perform linear interpolation between the current bin and the next bin using the bin index as the x-axis
    double x1 = start_idx;
    double x2 = end_idx;
    double y1 = dist->bins[start_idx];
    double y2 = dist->bins[end_idx];
    double x = (x1 + x2) / 2.0;
    double m = (y2 - y1) / (x2 - x1);
    double y = m * (x - x1) + y1;
    return y;
}

int compact(distribution_t *dist) {
    int retcode;
    RT_ASSERT(dist->rt, dist->bin_count == BIN_COUNT, "Error: Bin count is not equal to default bin count");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    // find the compaction point
    int compaction_idx;
    retcode = find_compaction_index(dist, &compaction_idx);
    RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to find compaction point");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    double new_value = interpolate(dist, compaction_idx, compaction_idx + 1);
    double delta = new_value - dist->bins[compaction_idx];
    // number of bins after the two compacted bins excluding the last one
    int bins_after_compaction_point = dist->bin_count - compaction_idx - 2;
    double step = delta / (double)(bins_after_compaction_point);
    // distribute the compaction error to the bins after the compaction point
    for (int i = 0; i < bins_after_compaction_point; i++) {
        int bin_idx = compaction_idx + 2 + i;
        if (bin_idx == dist->bin_count - 1) {
            break;
        }
        dist->bins[compaction_idx + 2 + i] += step;
    }
    
    dist->bins[compaction_idx] = new_value;
    dist->merges[compaction_idx]++;
    dist->merges[compaction_idx + 1]++;

    // Shift elements to remove the next bin
    for (int i = compaction_idx + 1; i < dist->bin_count - 1; i++) {
        dist->bins[i] = dist->bins[i + 1];
    }

    // Update the bin count and clear the last bin
    dist->bin_count--;
    dist->bins[dist->bin_count] = 0.0;
    dist->merges[dist->bin_count] = 0;
    return EXIT_SUCCESS;
}

int update_distribution(distribution_t *dist, double value) {
    int retcode;

    dist->count++;

    if (dist->count == 1) {
        dist->bins[0] = value;
        dist->merges[0] = 0;
        dist->bin_count = 1;
        return EXIT_SUCCESS;
    }

    if (is_full(dist)) {
        retcode = compact(dist);
        RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to compact distribution");
        if (dist->rt->has_error) {
            return EXIT_FAILURE;
        }
    }
    
    RT_ASSERT(dist->rt, dist->bin_count < BIN_COUNT, "Error: Bin count is greater than default bin count");    
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    retcode = insert_value(dist, value);
    RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to insert value into distribution");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void display_distribution(distribution_t *dist, FILE *fp) {
    fprintf(fp, "Distribution: Count = %d, Bin Count = %d\n", dist->count, dist->bin_count);
    fprintf(fp, "    Bins: \n");
    for (int i = 0; i < dist->bin_count; i++) {
        if (i % 20 == 0) {
            fprintf(fp, "    ") ;
        }
        fprintf(fp, "%0.2lf", dist->bins[i]);
        if (i % 20 == 19) { 
            fprintf(fp, "\n");
        } else {
            fprintf(fp, " ");
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "    Merges: \n");
    for (int i = 0; i < dist->bin_count; i++) {
        if (i % 20 == 0) {
            fprintf(fp, "    ") ;
        }
        fprintf(fp, "%d", dist->merges[i]);
        if (i % 20 == 19) { 
            fprintf(fp, "\n");
        } else {
            fprintf(fp, " ");
        }
    }
    fprintf(fp, "\n");
}

int get_percentile(distribution_t *dist, double pct, double *value) {
    RT_ASSERT(dist->rt, pct >= 0.0 && pct < 1.0, "Error: Percentile is out of range");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    if (dist->count == 0) {
        *value = 0.0;
        return EXIT_SUCCESS;
    }
    if (dist->count == 1) {
        *value = dist->bins[0];
        return EXIT_SUCCESS;
    }
    double double_idx = pct * (dist->bin_count);
    int idx = (int)double_idx;
    if (idx >= dist->bin_count) {
        *value = dist->bins[dist->bin_count - 1];
        return EXIT_SUCCESS;
    }
    
    *value = dist->bins[idx];
    if (idx == dist->bin_count - 1) {
        return EXIT_SUCCESS;
    }    
    double curr_pct = (idx) / (double)dist->bin_count;
    double next_pct = (idx + 1) / (double)dist->bin_count;
    double pct_range = next_pct - curr_pct;
    double bin_range = dist->bins[idx + 1] - dist->bins[idx];
    double error_pct = (pct - curr_pct) / pct_range;
    double correction_term = error_pct * bin_range;
    *value += correction_term;
    RT_ASSERT(dist->rt, correction_term >= 0.0, "Error: Correction term is negative");
    RT_ASSERT(dist->rt, correction_term <= bin_range, "Error: Correction term is greater than bin range");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

