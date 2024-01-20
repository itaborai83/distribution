#ifndef RUNTIME_H
#define RUNTIME_H

#define ERROR_MSG_SIZE 2048

#include <stdbool.h>

typedef struct error_entry_t{
    char const *file;
    char const *func;
    int line;
    char error_msg[ERROR_MSG_SIZE];
    struct error_entry_t *inner_error;
} error_entry_t;

typedef struct {
    error_entry_t *error_stack;
    bool has_error;
} runtime_t;

extern void runtime_init(runtime_t *rt);
extern void runtime_push_error(runtime_t *rt, const char *file, const char *func, const int line, const char *error_msg, ...);
extern void runtime_clear_error(runtime_t *rt);
extern void runtime_destroy(runtime_t *rt);
extern void runtime_print_error(runtime_t *rt);

#define RT_PUSH_ERROR(RT, MSG, ...) runtime_push_error(RT, __FILE__, __func__, __LINE__, MSG, ##__VA_ARGS__)
#define RT_CLEAR_ERROR(RT) runtime_clear_error(rt)
#define RT_ASSERT(RT, A, MSG, ...) do { if(!(A)) { runtime_push_error(RT, __FILE__, __func__, __LINE__, MSG, ##__VA_ARGS__); } } while(0)
#define RT_PANIC(RT, A, MSG, ...) do { if(!(A)) { runtime_push_error(RT, __FILE__, __func__, __LINE__, MSG, ##__VA_ARGS__); runtime_print_error(RT); exit(EXIT_FAILURE); } } while(0)
#define RT_CHECK_NO_ERROR(RT) do { if ((RT)->has_error) { runtime_print_error(RT); exit(EXIT_FAILURE); } } while(0)

#endif // RUNTIME_H