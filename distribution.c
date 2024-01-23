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
        double delta = abs(curr_count - next_count) / (double)MAX(curr_count, next_count);
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
    
    dist->bins[compaction_idx] = (dist->bins[compaction_idx] + dist->bins[compaction_idx + 1]) / 2.0;
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