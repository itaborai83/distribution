#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <limits.h>

#include "histogram.h"
#include "logger.h"
#include "runtime.h"

retcode_t hst_init(runtime_t *rt, histogram_t *hst, int base, int exponent) {
    memcpy(hst->header, "HST", 4);
    hst->rt = rt;
    hst->count = 0;
    hst->bin_count = 0;
    hst->exponent = exponent;
    hst->base = base;
    memset(hst->bins, 0, sizeof(hst->bins));
    return EXIT_SUCCESS;
}

retcode_t hst_destroy(histogram_t *hst) {
    hst->rt = NULL;
    hst->count = -1;
    hst->bin_count = -1;
    hst->exponent = -1;
    hst->base = -1;
    memset(hst->bins, 0, sizeof(hst->bins));
    return EXIT_SUCCESS;
}

retcode_t hst_compact(histogram_t *hst) {
    bin_t new_bins[BIN_COUNT];
    int new_bin_count;

    RT_ASSERT(hst->rt, hst->bin_count == BIN_COUNT, "Error: Histogram is not full");
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }

    for (int i = 0; i < hst->bin_count; i++) {
        RT_ASSERT(hst->rt, hst->bins[i].count > 0, "Error: found an uncompacted bin with an empty count at index %d", i);
        if (hst->rt->has_error) {
            return EXIT_FAILURE;
        }
    }

recompact:
    new_bin_count = 0;
    memset(new_bins, 0, sizeof(new_bins));
    // compact the old bins into the new bins
    for (int i = 0; i < BIN_COUNT; i++) {
        
        double new_alpha = floor(hst->bins[i].alpha / hst->base);
        
        if (new_bin_count == 0) {
            new_bins[new_bin_count].alpha = new_alpha;
            new_bins[new_bin_count].count = hst->bins[i].count;
            new_bin_count++;
            continue;
        } 

        if (new_bins[new_bin_count - 1].alpha == new_alpha) {
            new_bins[new_bin_count - 1].count += hst->bins[i].count;
            continue;
        }
        // allocate a new bin in the new bins array for the current bin
        new_bins[new_bin_count].alpha = new_alpha;
        new_bins[new_bin_count].count = hst->bins[i].count;
        new_bin_count++;
    }

    // clear old bins
    for (int i = 0; i < BIN_COUNT; i++) {
        if (i < new_bin_count) {
            hst->bins[i].alpha = new_bins[i].alpha;
            hst->bins[i].count = new_bins[i].count;
        } else {
            hst->bins[i].alpha = 0;
            hst->bins[i].count = 0;
        }
    }

    hst->bin_count = new_bin_count;
    hst->exponent++;
    if (hst->bin_count == BIN_COUNT) {
        goto recompact;
    }
    return EXIT_SUCCESS;
}

retcode_t hst_find_insertion_point(histogram_t *hst, double value, int *idx, bool *match) {
    int i = 0;
    *match = false;
    
    int alpha = floor(value / pow(hst->base, hst->exponent));
    
    for (i = 0; i < hst->bin_count; i++) {
        RT_ASSERT(hst->rt, hst->bins[i].count > 0, "Error: Bin count is zero");
        if (hst->rt->has_error) {
            return EXIT_FAILURE;
        }

        if (hst->bins[i].alpha == alpha) {
            *match = true;
            *idx = i;
            return EXIT_SUCCESS;
        }

        if (alpha < hst->bins[i].alpha) {
            break;
        }
    }
    RT_ASSERT(hst->rt, 0 <= i && i <= BIN_COUNT, "Error: Failed to find insertion point: %d", i);
    
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }
    *idx = i;
    return EXIT_SUCCESS;
}

retcode_t hst_update(histogram_t *hst, double value) {
    retcode_t rc;

    int idx;
    bool match;
    rc = hst_find_insertion_point(hst, value, &idx, &match);
    RT_ASSERT(hst->rt, rc == EXIT_SUCCESS, "Error: Failed to find insertion point");
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }

    if (match) {
        hst->bins[idx].count++;
        hst->count++;
        return EXIT_SUCCESS;
    } 
    
    // compact the histogram if it is full
    if (hst->bin_count == BIN_COUNT) {
        rc = hst_compact(hst);
        RT_ASSERT(hst->rt, rc == EXIT_SUCCESS, "Error: Failed to compact histogram");
        if (hst->rt->has_error) {
            return EXIT_FAILURE;
        }
        RT_ASSERT(hst->rt, hst->bin_count < BIN_COUNT, "Error: Histogram is still full after compaction");
        if (hst->rt->has_error) {
            return EXIT_FAILURE;
        }
    }

    // having compacted the histogram, find the insertion point again
    rc = hst_find_insertion_point(hst, value, &idx, &match);
    RT_ASSERT(hst->rt, rc == EXIT_SUCCESS, "Error: Failed to find insertion point");
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    // if the value is already in the histogram, increment the count
    if (match) {
        hst->bins[idx].count++;
        hst->count++;
        return EXIT_SUCCESS;
    } 

    // shift the elements to make room for the new element
    for (int i = hst->bin_count; i >= idx; i--) {
        hst->bins[i].alpha = hst->bins[i - 1].alpha;
        hst->bins[i].count = hst->bins[i - 1].count;
    }
    hst->bins[idx].alpha = floor(value / pow(hst->base, hst->exponent));
    hst->bins[idx].count = 1;
    hst->count++;
    hst->bin_count++;
    
    RT_ASSERT(hst->rt, hst->bin_count <= BIN_COUNT, "Error: Bin count is greater than default bin count");
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS; 
}

retcode_t hst_get_percentiles(histogram_t *hst, percentiles_t *pcts) {
    double curr_count = 0.0;
    pcts->bin_count = hst->bin_count;
    for (int i = 0; i < hst->bin_count; i++) {
        double bin_pct = curr_count / (double)hst->count;
        pcts->pcts[i] = bin_pct;
        pcts->values[i] = hst->bins[i].alpha * pow(hst->base, hst->exponent);
        curr_count += hst->bins[i].count;
    }
    return EXIT_SUCCESS;
}

retcode_t hst_display(histogram_t *hst, FILE *fp) {
    fprintf(fp, "Histogram: Count = %d, Bin Count = %d, Base = %d, Exponent = %d\n", 
        hst->count,
        hst->bin_count,
        hst->base,
        hst->exponent
    );
    fprintf(fp, "    Bins: \n");
    for (int i = 0; i < hst->bin_count; i++) {
        double value = hst->bins[i].alpha * pow(hst->base, hst->exponent);
        int count = hst->bins[i].count;
        if (i % 5 == 0) {
            fprintf(fp, "    ") ;
        }
        fprintf(fp, "(%0.2lf, %d)", value, count);
        if (i % 5 == 4) { 
            fprintf(fp, "\n");
        } else {
            fprintf(fp, " ");
        }
    }
    fprintf(fp, "\n");
    return EXIT_SUCCESS;
}

retcode_t hst_display_percentiles(histogram_t *hst, FILE *fp, double precision) {
    retcode_t rc;
    percentiles_t pcts[BIN_COUNT];
    
    RT_ASSERT(hst->rt, precision > 0.0, "Error: Precision is less than zero");
    RT_ASSERT(hst->rt, precision < 1.0, "Error: Precision is greater than one");
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }
    
    rc = hst_get_percentiles(hst, pcts);
    RT_ASSERT(hst->rt, rc == EXIT_SUCCESS, "Error: Failed to get percentiles");
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }
    fprintf(fp, "PCT\tVALUE\n");
    for (double pct = 0; pct < 1.0; pct += precision) {
        double value;
        rc = hst_get_percentile(hst, pcts, pct, &value);
        RT_ASSERT(hst->rt, rc == EXIT_SUCCESS, "Error: Failed to get percentile");
        if (hst->rt->has_error) {
            return EXIT_FAILURE;
        }
        fprintf(fp, "%lf\t%lf\n", pct, value);
    }
    return EXIT_SUCCESS;
}

retcode_t hst_save(histogram_t *hst, FILE *fp) {
    runtime_t *rt = hst->rt;

    RT_ASSERT(hst->rt, fp != NULL, "Error: file is not open");
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }
    hst->rt = NULL;
    int written = fwrite(hst, sizeof(histogram_t), 1, fp);
    RT_ASSERT(rt, written == 1, "Error: Failed to write histogram to file");
    if (rt->has_error) {
        return EXIT_FAILURE;
    }
    hst->rt = rt;
    return EXIT_SUCCESS;
}

retcode_t hst_load(runtime_t *rt, histogram_t *hst, FILE *fp) {
    RT_ASSERT(rt, fp != NULL, "Error: file is not open");
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }
    int read = fread(hst, sizeof(histogram_t), 1, fp);
    RT_ASSERT(rt, read == 1, "Error: Failed to read histogram from file");
    if (rt->has_error) {
        return EXIT_FAILURE;
    }
    hst->rt = rt;
    return EXIT_SUCCESS;
}

retcode_t hst_get_percentile(histogram_t *hst, percentiles_t *pcts, double pct, double *value) {
    RT_ASSERT(hst->rt, pct >= 0.0, "Error: Percentile is less than zero");
    RT_ASSERT(hst->rt, pct < 1.0, "Error: Percentile is greater than one");
    RT_ASSERT(hst->rt, pcts->bin_count > 0, "Error: Percentiles are empty");
    if (hst->rt->has_error) {
        return EXIT_FAILURE;
    }
    *value = pcts->values[pcts->bin_count - 1];
    for (int i = 0; i < pcts->bin_count; i++) {
        
        *value = pcts->values[i];
        
        // if the percentile is greater than the last percentile, return the last value.
        // No interpolation is necessary/possible.
        if (i == pcts->bin_count - 1) {
            return EXIT_SUCCESS;
        }

        // if the percentile is between the current and next percentile, interpolate the value
        if (pcts->pcts[i] <= pct && pct <= pcts->pcts[i + 1]) {
            double bin_pct = pcts->pcts[i];
            double bin_value = pcts->values[i];
            double next_bin_pct = pcts->pcts[i + 1];
            double next_bin_value = pcts->values[i + 1];
            // interpolate the value
            double pct_range = next_bin_pct - bin_pct;
            double bin_range = next_bin_value - bin_value;
            double error_pct = (pct - bin_pct) / pct_range;
            double correction_term = error_pct * bin_range;
            // set the value
            *value += correction_term;
            
            RT_ASSERT(hst->rt, correction_term >= 0.0, "Error: Correction term is negative");
            RT_ASSERT(hst->rt, correction_term <= bin_range, "Error: Correction term is greater than bin range");
            if (hst->rt->has_error) {
                return EXIT_FAILURE;
            }
            
            return EXIT_SUCCESS;
        }
    }
    return EXIT_SUCCESS;
}