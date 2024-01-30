#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <time.h>

#include "logger.h"
#include "distribution.h"
#include "runtime.h"


int compare(char *filename, histogram_t *hst, double *samples, int sample_count) {
    retcode_t rc;
   
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
    percentiles_t pcts;
    rc = hst_get_percentiles(hst, &pcts);
    RT_CHECK_NO_ERROR(hst->rt);

    FILE *fp = fopen(filename, "w");
    LOG_ASSERT(fp != NULL, "Error: Failed to open file %s", filename);

    fprintf(fp, "PCT\tSAMPLE\tDIST\tDIFF\n");
    for (int i = 0; i < pcts.bin_count; i++) {
        double pct = pcts.pcts[i];
        double value = pcts.values[i];
        double double_sample_idx = pct * sample_count;
        int sample_idx = (int)double_sample_idx;
        double sample_value = samples[sample_idx];

        fprintf(fp, "%lf\t%lf\t%lf\t%lf\n", 
            pct,
            sample_value,
            value,
            sample_value - value
        );
    }
    rc = fclose(fp);
    LOG_ASSERT(rc == 0, "Error: Failed to close file %s", filename);
    return EXIT_SUCCESS;
}

int main() {
    retcode_t rc;
    double *samples = NULL;
    int capacity = 999999999;
    int curr_sample_idx = -1; 
    samples = malloc(sizeof(double) * 999999999);
    LOG_ASSERT(samples != NULL, "Error: Failed to allocate memory for samples");

    runtime_t rt;
    histogram_t hst;
    runtime_init(&rt);
    rc = hst_init(&rt, &hst, 2, -3);
    RT_PANIC(&rt, rc == EXIT_SUCCESS, "Error: Failed to initialize histogram");
    double value;
    while (scanf("%lf", &value) != EOF) {
        samples[++curr_sample_idx] = value;
        curr_sample_idx = curr_sample_idx % capacity;
        rc = hst_update(&hst, value);
        RT_PANIC(&rt, rc == EXIT_SUCCESS, "Error: Failed to update histogram");        
    }
    rc = hst_display(&hst, stderr);
    RT_PANIC(&rt, rc == EXIT_SUCCESS, "Error: failed to display histogram");

    rc = compare("histog.out", &hst, samples, curr_sample_idx);
    RT_PANIC(&rt, rc == EXIT_SUCCESS, "Error: Failed to compare distributions");

    return 0;
}