#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "logger.h"
#include "runtime.h"
#include <stdarg.h>

void runtime_init(runtime_t *rt) {
    srand(time(0));
    rt->error_stack = NULL;
    rt->has_error = false;
}

void runtime_push_error(runtime_t *rt, const char *file, const char *func, const int line, const char *format_error, ...) {
    error_entry_t *curr = rt->error_stack;
    error_entry_t *err = malloc(sizeof(error_entry_t));
    memset(err, 0, sizeof(error_entry_t));
    err->file = file;
    err->func = func;
    err->line = line;
    va_list args;
    va_start(args, format_error);
    vsnprintf(err->error_msg, ERROR_MSG_SIZE, format_error, args);
    va_end(args);

    // copy error msg into buffer
    err->inner_error = curr;
    rt->error_stack = err;
    rt->has_error = true;
    return;
}

void runtime_clear_error(runtime_t *rt) {
    error_entry_t *curr = rt->error_stack;
    while (curr != NULL) {
        error_entry_t *next = curr->inner_error;
        free(curr);
        curr = next;
    }
    rt->error_stack = NULL;
    rt->has_error = false;
    return;
}

void runtime_destroy(runtime_t *rt) {
    runtime_clear_error(rt);
    return;
}

static void _runtime_print_error(error_entry_t *curr) {
    if (curr == NULL) {
        return;
    }
    _runtime_print_error(curr->inner_error);
    fprintf(stderr, "[ERROR]%s:%d:%s:\n\t%s\n", curr->file, curr->line, curr->func, curr->error_msg);
}

void runtime_print_error(runtime_t *rt) {
    _runtime_print_error(rt->error_stack);
}
