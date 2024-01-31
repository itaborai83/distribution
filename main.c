#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <time.h>

#include "logger.h"
#include "distribution.h"
#include "runtime.h"


#define DEFAULT_BASE 2
#define DEFAULT_EXPONENT -3

typedef struct {
    char *filename;
    int base;
    int exponent;
    bool percentiles;
    bool quiet;
    bool help;
} options_t;

void usage(char *progname) {
    fprintf(stderr, "Usage: %s [OPTIONS] FILE\n", progname);
    fprintf(stderr, "Compare the distribution of samples in FILE to the distribution of samples in standard input\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -b BASE     Set the base of the histogram. Defaults to " "\n");
    fprintf(stderr, "  -e EXPONENT Set the exponent of the histogram\n");
    fprintf(stderr, "  -p          Show percentiles\n");
    fprintf(stderr, "  -q          Quiet mode\n");
    fprintf(stderr, "  -h          Print this message and exit\n");
}

retcode_t parse_options(runtime_t *rt, int argc, char *argv[], options_t *options) {
    int opt;
    options->filename = NULL;
    options->base = DEFAULT_BASE;
    options->exponent = DEFAULT_EXPONENT;
    options->percentiles = false;
    options->quiet = false;
    options->help = false;

    while ((opt = getopt(argc, argv, "b:e:pqh")) != -1) {
        switch (opt) {
            case 'b':
                options->base = atoi(optarg);
                RT_ASSERT(rt, options->base > 0, "Error: Base must be greater than 0");
                if (rt->has_error) {
                    return EXIT_FAILURE;
                }
                break;

            case 'e':
                options->exponent = atoi(optarg);
                break;

            case 'p':
                options->percentiles = true;
                break;	

            case 'q':
                options->quiet = true;
                break;

            case 'h':
                options->help = true;
                break;

            default:
                return EXIT_FAILURE;
        }
    }
    if (optind < argc) {
        options->filename = argv[optind];
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    retcode_t rc;
    runtime_t rt;
    options_t options;
    histogram_t hst;
    
    runtime_init(&rt);
    
    rc = parse_options(&rt, argc, argv, &options);
    if (rc != EXIT_SUCCESS) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    if (options.help) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    rc = hst_init(&rt, &hst, options.base, options.exponent);
    if (rt.has_error) {
        runtime_print_error(&rt);
        return EXIT_FAILURE;
    }

    if (options.filename != NULL) {
        // does file exist?
        FILE *fp = fopen(options.filename, "rb");
        if (fp != NULL) {
            rc = hst_load(&rt, &hst, fp);
            if (rc != EXIT_SUCCESS) {
                runtime_print_error(&rt);
                return EXIT_FAILURE;
            }
            fclose(fp);
        }
    }

    double value;
    while (1) {
        int read = scanf("%lf", &value);
        if (read == EOF) {
            break;
        }
        if (read == 0) {
            // consume the bad input and continue
            scanf("%*[^0-9+-.]");
            continue;
        }
        rc = hst_update(&hst, value);
        if (rc != EXIT_SUCCESS) {
            runtime_print_error(&rt);
            return EXIT_FAILURE;
        }
    }
    
    if (!options.quiet) {
        if (options.percentiles) {
            rc = hst_display_percentiles(&hst, stdout);
            if (rc != EXIT_SUCCESS) {
                runtime_print_error(&rt);
                return EXIT_FAILURE;
            }
            
        } else {
            rc = hst_display(&hst, stdout);
            if (rc != EXIT_SUCCESS) {
                runtime_print_error(&rt);
                return EXIT_FAILURE;
            }
        }
    }

    if (options.filename != NULL) {
        FILE *fp = fopen(options.filename, "wb");
        if (fp == NULL) {
            runtime_push_error(&rt, __FILE__, __func__, __LINE__, "Error: Failed to open file %s", options.filename);
            runtime_print_error(&rt);
            return EXIT_FAILURE;
        }
        
        rc = hst_save(&hst, fp);
        if (rc != EXIT_SUCCESS) {
            runtime_print_error(&rt);
            return EXIT_FAILURE;
        }
        fclose(fp);
    }

    return EXIT_SUCCESS;
}

/*
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
*/