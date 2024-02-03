#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <time.h>

#include "logger.h"
#include "histogram.h"
#include "runtime.h"


#define DEFAULT_BASE 2
#define DEFAULT_EXPONENT -3
#define DEFAULT_PERCENTILES_PRECISION 0.01

typedef struct {
    char *filename;
    int base;
    int exponent;
    bool percentiles;
    double percentiles_precision;
    bool quiet;
    bool help;
} options_t;

void usage(char *progname) {
    fprintf(stderr, "Usage: %s [OPTIONS] FILE\n", progname);
    fprintf(stderr, "Compare the distribution of samples in FILE to the distribution of samples in standard input\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -b BASE     Set the base of the histogram. Defaults to %d\n", DEFAULT_BASE);
    fprintf(stderr, "  -e EXPONENT Set the exponent of the histogram. Defaults to %d\n", DEFAULT_EXPONENT);
    fprintf(stderr, "  -p          Show percentiles\n");
    fprintf(stderr, "  -P          Set the precision of the percentiles. Defaults to %0.02lf\n", DEFAULT_PERCENTILES_PRECISION);
    fprintf(stderr, "  -q          Quiet mode\n");
    fprintf(stderr, "  -h          Print this message and exit\n");
}

retcode_t parse_options(runtime_t *rt, int argc, char *argv[], options_t *options) {
    int opt;
    options->filename = NULL;
    options->base = DEFAULT_BASE;
    options->exponent = DEFAULT_EXPONENT;
    options->percentiles_precision = DEFAULT_PERCENTILES_PRECISION;
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

            case 'P':
                options->percentiles_precision = atof(optarg);
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
    while (true) {
        
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
            rc = hst_display_percentiles(&hst, stdout, options.percentiles_precision);
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
