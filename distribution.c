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

#define NUM_AVG_DELTAS 100
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void init_distribution(runtime_t *rt, distribution_t *dist) {
    dist->count = 0;
    dist->bin_count = 0;
    dist->left_outlier_count = 0;
    dist->right_outlier_count = 0;
    memset(dist->left_outliers, 0, sizeof(dist->left_outliers));
    memset(dist->bins, 0, sizeof(dist->bins));
    memset(dist->counts, 0, sizeof(dist->counts));
    memset(dist->right_outliers, 0, sizeof(dist->right_outliers));
    dist->rt = rt;
    runtime_init(dist->rt);
}

void copy_distribution(distribution_t *src, distribution_t *dst) {
    dst->rt = src->rt;
    dst->count = src->count;
    dst->bin_count = src->bin_count;
    dst->left_outlier_count = src->left_outlier_count;
    dst->right_outlier_count = src->right_outlier_count;
    memcpy(dst->left_outliers, src->left_outliers, sizeof(src->left_outliers));
    memcpy(dst->bins, src->bins, sizeof(src->bins));
    memcpy(dst->counts, src->counts, sizeof(src->counts));
    memcpy(dst->right_outliers, src->right_outliers, sizeof(src->right_outliers));
}

int update_left_outliers(distribution_t *dist, double *value, bool *keep_going) {
    // If we haven't filled up the left outliers, just add the value keeping the array sorted
    if (dist->left_outlier_count < OUTLIER_COUNT) {
        // find the position to insert the new element
        int i, j;
        for (i = 0; i < dist->left_outlier_count; i++) {
            if (dist->left_outliers[i] > *value) {
                break;
            }
        }
        // shift the elements to make room for the new element
        for (j = dist->left_outlier_count; j > i; j--) {
            dist->left_outliers[j] = dist->left_outliers[j - 1];
        }
        // insert the new element
        dist->left_outliers[i] = *value;        
        // update the size of the array and return indicating that no further processing is necessary
        dist->left_outlier_count++;
        *keep_going = false;
        return EXIT_SUCCESS;
    }
    
    // in the majority of cases the new value will be less than the largest left outlier, so we can just return
    if (dist->left_outliers[OUTLIER_COUNT-1] <= *value) {
        *keep_going = true;
        return EXIT_SUCCESS;
    }

    // If we have filled up the left outliers, then we need to replace the largest left outlier
    int insert_idx = OUTLIER_COUNT - 1;
    int curr_idx = OUTLIER_COUNT - 2;
    while(1) {
        double curr = dist->left_outliers[curr_idx];
        if (curr < *value) {
            break;
        }
        insert_idx--;
        curr_idx--; 
        if (curr_idx < 0) {
            break;
        }
    }
    bool is_valid_idx = insert_idx >= 0 && insert_idx <= OUTLIER_COUNT-1;
    RT_ASSERT(dist->rt, is_valid_idx, "Error: Insert index is out of bounds (%d)", insert_idx);
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }

    double new_value;
    // replace the largest left outlier with the new value
    new_value = dist->left_outliers[dist->left_outlier_count - 1];
    int idx = dist->left_outlier_count - 1;
    while (idx > insert_idx) {
        dist->left_outliers[idx] = dist->left_outliers[idx - 1];
        idx--;
    }
    dist->left_outliers[insert_idx] = *value;
    *value = new_value;
    *keep_going = true;
    return EXIT_SUCCESS;
}

int update_right_outliers(distribution_t *dist, double *value, bool *keep_going) {
    // If we haven't filled up the right outliers, just add the value keeping the array sorted
    if (dist->right_outlier_count < OUTLIER_COUNT) {
        // find the position to insert the new element
        int i, j;
        for (i = 0; i < dist->right_outlier_count; i++) {
            if (dist->right_outliers[i] > *value) {
                break;
            }
        }
        // shift the elements to make room for the new element
        for (j = dist->right_outlier_count; j > i; j--) {
            dist->right_outliers[j] = dist->right_outliers[j - 1];
        }
        // insert the new element
        dist->right_outliers[i] = *value;        
        // update the size of the array and return indicating that no further processing is necessary
        dist->right_outlier_count++;
        *keep_going = false;
        return EXIT_SUCCESS;
    }
    
    // in the majority of cases the new value will be greater than the largest right outlier, so we can just return
    if (*value <= dist->right_outliers[0]) {
        *keep_going = true;
        return EXIT_SUCCESS;
    }

    // If we have filled up the right outliers, then we need to replace the smallest right outlier
    int insert_idx = -1;
    int curr_idx = 0;
    while(1) {
        double curr = dist->right_outliers[curr_idx];
        if (curr > *value) {
            break;
        }
        insert_idx++;
        curr_idx++; 
        if (curr_idx >= OUTLIER_COUNT) {
            break;
        }
    }
    bool is_valid_idx = insert_idx >= -1 && insert_idx <= OUTLIER_COUNT-1;
    RT_ASSERT(dist->rt, is_valid_idx, "Error: Insert index is out of bounds (%d)", insert_idx);
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    // if the new value is the same as the largest right outlier, then we don't need to do anything
    if (insert_idx == -1) {
        *keep_going = true;
        return EXIT_SUCCESS;
    }
    // replace the largest right outlier with the new value
    int idx = OUTLIER_COUNT - 1;
    double new_value = dist->right_outliers[idx];
    while (idx > insert_idx) {
        dist->right_outliers[idx] = dist->right_outliers[idx - 1];
        idx--;
    }
    dist->right_outliers[insert_idx] = *value;
    *value = new_value;
    *keep_going = true;
    return EXIT_SUCCESS;
}

bool is_full(distribution_t *dist) {
    return  dist->left_outlier_count    == OUTLIER_COUNT && 
            dist->right_outlier_count   == OUTLIER_COUNT &&
            dist->bin_count             == BIN_COUNT;
}

int find_compaction_point(distribution_t *dist, int *idx) {
    double max_delta = DBL_MIN;
    // if all the counts are equal, pick a random bin as the compaction point
    *idx = rand() % (dist->bin_count - 1);
    int curr_count;
    int next_count;
    for (int i = 0; i < dist->bin_count - 1; i++) {
        RT_PANIC(dist->rt, dist->bins[i] <= dist->bins[i + 1], "Error: Bins are not sorted");
        curr_count = dist->counts[i];
        next_count = dist->counts[i + 1];
        double delta = abs(curr_count - next_count);
        if (delta > max_delta) {
            max_delta = delta;
            *idx = i;
        }
    }
    return EXIT_SUCCESS;
}

int compact(distribution_t *dist) {
    int retcode;    
    RT_ASSERT(dist->rt, dist->bin_count == BIN_COUNT, "Error: Bin count is not equal to default bin count");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    // find the compaction point
    int compaction_idx;
    retcode = find_compaction_point(dist, &compaction_idx);
    RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to find compaction point");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    dist->bins[compaction_idx] = rand() % 2
        ? dist->bins[compaction_idx]
        : dist->bins[compaction_idx + 1];
    dist->counts[compaction_idx] = dist->counts[compaction_idx] + dist->counts[compaction_idx + 1];
    // Shift elements to remove the next bin
    for (int i = compaction_idx + 1; i < dist->bin_count - 1; i++) {
        dist->bins[i] = dist->bins[i + 1];
        dist->counts[i] = dist->counts[i + 1];
    }

    // Update the bin count and clear the last bin
    dist->bin_count--;
    dist->bins[dist->bin_count] = DBL_MAX;
    dist->counts[dist->bin_count] = 0;
    return EXIT_SUCCESS;
}

int update_distribution(distribution_t *dist, double value) {
    bool keep_going;
    int retcode;
    double original_value;

    dist->count++;
restart:
    original_value = value;
    retcode = update_left_outliers(dist, &value, &keep_going);
    RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to update left outliers");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }

    if (!keep_going) {
        return EXIT_SUCCESS;
    } 
    
    if (original_value != value) {
        goto restart;
    }

    original_value = value;
    retcode = update_right_outliers(dist, &value, &keep_going);
    RT_ASSERT(dist->rt, retcode == EXIT_SUCCESS, "Error: Failed to update right outliers");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }

    if (!keep_going) {
        return EXIT_SUCCESS;
    } 

    if (original_value != value) {
        goto restart;
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
    
    // Add the new value to the bins maintaining sorted order
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
        dist->counts[j] = dist->counts[j - 1];
    }
    dist->bins[i] = value;
    dist->counts[i]++;
    dist->bin_count++;
    return EXIT_SUCCESS;
}

void display_distribution(distribution_t *dist) {
    fprintf(stderr, "Distribution:\n");
    fprintf(stderr, "  Count: %d\n", dist->count);
    fprintf(stderr, "  Bin Count: %d\n", dist->bin_count);
    fprintf(stderr, "  Left Outlier Count: %d\n", dist->left_outlier_count);
    fprintf(stderr, "  Right Outlier Count: %d\n", dist->right_outlier_count);
    fprintf(stderr, "  Left Outliers: ");
    for (int i = 0; i < dist->left_outlier_count; i++) {
        fprintf(stderr, "%0.2lf ", dist->left_outliers[i]);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "  Right Outliers: ");
    for (int i = 0; i < dist->right_outlier_count; i++) {
        fprintf(stderr, "%0.2lf ", dist->right_outliers[i]);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "  Bins:\n");
    for (int i = 0; i < dist->bin_count; i++) {
        if (i % 10 == 0) {
            fprintf(stderr, "    ") ;
        }
        fprintf(stderr, "%0.2lf", dist->bins[i]);
        if (i % 10 == 9) { 
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, " ");
        }
    }
    fprintf(stderr, "\n");
}

/*
int init_histogram(Histogram **hist, int bin_count) {
    *hist = NULL;
    LOG_ASSERT(bin_count > 0, "Error: invalid bin count for histogram");
    Histogram *result = malloc(sizeof(Histogram));
    LOG_ASSERT(result != NULL, "Error: could not allocate memory for an histogram");
    result->bins = malloc(sizeof(histogram_bin_t) * bin_count);
    result->bin_count = bin_count;
    // initialize bins with all zeros
    memset(result->bins, 0, sizeof(histogram_bin_t) * bin_count);
    LOG_ASSERT(result->bins != NULL, "Error: could not allocate memory for the bins of an histogram");
    *hist = result;
    return EXIT_SUCCESS;
}

int destroy_histogram(Histogram **hist) {
    LOG_ASSERT(hist != NULL && *hist != NULL, "Error: destroy called with an invalid histogram");
    free((*hist)->bins);
    free(*hist);
    *hist = NULL;
    return EXIT_SUCCESS;
}

double compute_delta(distribution_t *dist, double distribution_range, int curr_idx) {
    double curr_value = dist->bins[curr_idx].value;
    double next_value = dist->bins[curr_idx + 1].value;
    double delta = (next_value - curr_value) / distribution_range;
    return delta;
}

double compute_connectedness_map(distribution_t *dist, bool **forward_connected, bool **backward_connected) {
    RT_ASSERT(dist->rt, dist != NULL, "Error: null distribution");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    RT_ASSERT(dist->rt, dist->bin_count > 3, "Error: insuficient values in distribution");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    int delta_count = (dist->bin_count - 1);
    double min_value = dist->bins[0].value;
    double max_value = dist->bins[delta_count].value;
    double distribution_range = max_value - min_value;
    RT_ASSERT(dist->rt, distribution_range > 0, "Error: distribution has a zero or negative range");
    if (dist->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    double sum_of_deltas = 0.0;
    for (int i = 0; i < delta_count; i++) {
        sum_of_deltas += compute_delta(dist, distribution_range, i);
    }
    double average_delta = sum_of_deltas / delta_count;
    double threshold =  NUM_AVG_DELTAS * average_delta;

    *forward_connected = malloc(sizeof(bool) * dist->bin_count);
    RT_PANIC(dist->rt, *forward_connected != NULL, "Error: allocation failed");
    *backward_connected = malloc(sizeof(bool) * dist->bin_count);
    RT_PANIC(dist->rt, *backward_connected != NULL, "Error: allocation failed");

    for (int i=0; i < dist->bin_count; i++) {
        (*forward_connected)[i] = false;
        (*backward_connected)[i] = false;

        if (i != 0) {
            (*forward_connected)[i] = compute_delta(dist, distribution_range, i) <= threshold;
        } 
        if (i != delta_count) {
            (*backward_connected)[i] = compute_delta(dist, distribution_range, i - 1) <= threshold;
        }
    }
    return EXIT_SUCCESS;
}  

int create_histogram(distribution_t *dist, Histogram *hist) {
    LOG_ASSERT(dist != NULL, "Error: create_histogram called with an unintialised distribution");
    LOG_ASSERT(dist->bin_count > 3, "Error: invalid number of bins");
    LOG_ASSERT(hist != NULL, "Error: create_histogram called with an unitialized histogram");
    LOG_ASSERT(dist->bin_count > 0, "Error: invalid number of bins");

    double min_value = dist->bins[0].value;
    double max_value = dist->bins[dist->bin_count-1].value;
    double distribution_range = max_value - min_value;
    LOG_ASSERT(distribution_range > 0.0, "Error: distribution range is zero or negative");
    double bin_size = distribution_range / hist->bin_count;
    LOG_ASSERT(bin_size > 0.0, "Error: bin size is zero or negative");
    
    int retcode;
    bool *forward_connected;
    bool *backward_connected;
    retcode = compute_connectedness_map(dist, &forward_connected, &backward_connected);
    LOG_ASSERT(retcode == EXIT_SUCCESS, "Error: failed to compute connected map successfully");
    
    for (int i = 0; i < dist->bin_count - 1; i++) {
        
        // is the value an isolated one? Ignore if so
        if (!backward_connected[i] && !forward_connected[i]) {
            continue;
        }
        // is the value a local minimum? Ignore if so
        // dist->bins[dist->bin_count - 1].value is the last value in the distribution
        // and is always a local minimum
        if (backward_connected[i] && !forward_connected[i]) {
            continue;
        }

        double curr_value = dist->bins[i].value;
        double next_value = dist->bins[i + 1].value;
        double bin_range = next_value - curr_value;
        double bin_start = curr_value;
        double step = bin_range / BIN_RESOLUTION;

        for (int j = 0; j < BIN_RESOLUTION; j++) {
            double bin_value = bin_start + (j * step);
            double bin_value_pct = (bin_value - min_value) / distribution_range;
            int histogram_idx = (int)(bin_value_pct * hist->bin_count);
            bool invalid_index = histogram_idx < 0 || histogram_idx >= hist->bin_count;
            if (invalid_index) {
                LOG_ERROR("Error: invalid histogram index (%d)", histogram_idx);
                LOG_ERROR("Error: bin_value_pct = %lf", bin_value_pct);
                LOG_ERROR("Error: bin_value = %lf", bin_value);
                LOG_ERROR("Error: min_value = %lf", min_value);
                LOG_ERROR("Error: max_value = %lf", max_value);
                LOG_ERROR("Error: distribution_range = %lf", distribution_range);
                LOG_ERROR("Error: bin_size = %lf", bin_size);
                LOG_ERROR("Error: bin_start = %lf", bin_start);
                LOG_ERROR("Error: step = %lf", step);
                LOG_ERROR("Error: curr_value = %lf", curr_value);
                LOG_ERROR("Error: next_value = %lf", next_value);
                LOG_ERROR("Error: bin_range = %lf", bin_range);
                LOG_ERROR("Error: i = %d", i);
                LOG_ERROR("Error: j = %d", j);
                LOG_ERROR("Error: BIN_RESOLUTION = %d", BIN_RESOLUTION);
                LOG_ERROR("Error: hist->bin_count = %d", hist->bin_count);
                LOG_FATAL("Fatal error: exiting");
            }
            if (hist->bins[histogram_idx].count == 0) {
                hist->bins[histogram_idx].value = bin_value;
            }
            hist->bins[histogram_idx].count++;
        }
    }
    free(forward_connected);
    free(backward_connected);
    return EXIT_SUCCESS;
}
*/