#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>

#define LOG_DEBUG(M, ...) do { fprintf(stderr, "[DEBUG] (%s:%d:%s) " M "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)
#define LOG_INFO(M, ...) do { fprintf(stderr, "[INFO] (%s:%d:%s) " M "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)
#define LOG_WARN(M, ...) do { fprintf(stderr, "[WARN] (%s:%d:%s) " M "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)
#define LOG_ERROR(M, ...) do { fprintf(stderr, "[ERROR] (%s:%d:%s) " M "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)
#define LOG_FATAL(M, ...) do { fprintf(stderr, "[FATAL] (%s:%d:%s) " M "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); exit(EXIT_FAILURE); } while(0)
#define LOG_ASSERT(A, M, ...) do { if(!(A)) { LOG_FATAL(M, ##__VA_ARGS__); } } while(0)

#endif // LOGGER_H