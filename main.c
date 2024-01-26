#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <time.h>

#include "logger.h"
#include "distribution.h"
#include "runtime.h"

int compare(char *filename, distribution_t *dist, double *samples, int sample_count) {
    int retcode;
    
    // sort samples using shell sort
    int gap = sample_count / 2;
    while (gap > 0) {
        for (int i = gap; i < sample_count; i++) {
            double value = samples[i];
            int j = i;
            while (j >= gap && samples[j - gap] > value) {
                samples[j] = samples[j - gap];
                j -= gap;
            }
            samples[j] = value;
        }
        gap /= 2;
    }
    FILE *fp = fopen(filename, "w");
    LOG_ASSERT(fp != NULL, "Error: Failed to open file %s", filename);

    fprintf(fp, "PCT\tSAMPLE\tDIST\tDIFF\n");
    for (double pct = 0.0; pct <= 0.999; pct += 0.001) {
        double value;
        retcode = get_percentile(dist, pct, &value);
        RT_CHECK_NO_ERROR(dist->rt);
        double double_sample_idx = pct * sample_count;
        int sample_idx = (int)double_sample_idx;
        double sample_value = samples[sample_idx];

        if (sample_idx < sample_count - 1) {
            double next_sample_pct = (sample_idx + 1) / (double)sample_count;
            double delta_sample_pct = pct - next_sample_pct;
            double next_sample_value = samples[sample_idx + 1];
            double delta_sample_value = next_sample_value - sample_value;
            sample_value += delta_sample_value * delta_sample_pct;
        }
        fprintf(fp, "%lf\t%lf\t%lf\t%lf\n", 
            pct,
            sample_value,
            value,
            sample_value - value
        );
    }
    retcode = fclose(fp);
    LOG_ASSERT(retcode == 0, "Error: Failed to close file %s", filename);
    return EXIT_SUCCESS;
}

int write_data(distribution_t *dist, char *filename, bool open, bool close) {
    int retcode = 0;
    static FILE *fp;

    if (open) {
        fp = fopen(filename, "wb");
        LOG_ASSERT(fp != NULL, "Error: Failed to open file %s", filename);
        return EXIT_SUCCESS;
    }
    if (close) {
        retcode = fclose(fp);
        LOG_ASSERT(retcode == 0, "Error: Failed to close file %s", filename);
        return EXIT_SUCCESS;
    }    
    double merge_count = 0.0;
    for (int i = 0; i < BIN_COUNT; i++) {
        merge_count += dist->merges[i];
    }
    for (int i = 0; i < BIN_COUNT; i++) {
        double row[3];
        row[0] = i;
        row[1] = dist->bins[i];
        row[2] = merge_count == 0.0 ? 0.0 : ((double)dist->merges[i]) / merge_count;
        int written = fwrite(row, sizeof(double), 3, fp);
        LOG_ASSERT(written == 3, "Error: Failed to write data to file %s", filename);
    }
    // add a blank line between files to indicate the end of a dataset
    return EXIT_SUCCESS;
}

int main() {
    int retcode;
    double *samples = NULL;
    int capacity = 9999999;
    int curr_sample_idx = -1; 
    samples = malloc(sizeof(double) * 9999999);
    LOG_ASSERT(samples != NULL, "Error: Failed to allocate memory for samples");

    runtime_t rt;
    distribution_t dist;
    runtime_init(&rt);
    init_distribution(&rt, &dist);
    srand(0);

    double value;
    retcode = write_data(&dist, "./data/animation/distribution.bin", true, false);
    RT_PANIC(dist.rt, retcode == EXIT_SUCCESS, "Error: Failed to open distribution data file");
    while (scanf("%lf", &value) != EOF) {
        samples[++curr_sample_idx] = value;
        curr_sample_idx = curr_sample_idx % capacity;
        //LOG_DEBUG("%lf", value);
        retcode = update_distribution(&dist, value);
        //display_distribution(&dist);
        RT_PANIC(dist.rt, retcode == EXIT_SUCCESS, "Error: Failed to update distribution");        
        retcode = write_data(&dist, "./data/animation/distribution.bin", false, false);
        RT_PANIC(dist.rt, retcode == EXIT_SUCCESS, "Error: Failed to write distribution data");
    }
    retcode = write_data(&dist, "./data/animation/distribution.bin", false, true);
    RT_PANIC(dist.rt, retcode == EXIT_SUCCESS, "Error: Failed to close distribution data file");
    
    display_distribution(&dist, stderr);
    
    double sample_count = dist.count < capacity ? dist.count : capacity;
    retcode = compare("histog.out", &dist, samples, sample_count);
    LOG_ASSERT(retcode == EXIT_SUCCESS, "Error: Failed to compare distributions");
    return 0;
}