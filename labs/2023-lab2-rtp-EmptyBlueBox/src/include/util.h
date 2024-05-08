#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

    uint32_t compute_checksum(const void *pkt, size_t n_bytes);

    void print_hex(const void *ptr, std::size_t len);

    double get_time_interval_by_second(struct timeval *test_start, struct timeval *test_end);

// Use it to display a help message
#define LOG_MSG(...)                                          \
    do                                                        \
    {                                                         \
        fprintf(stdout, "\033[32m[INFO]\033[0m" __VA_ARGS__); \
        fflush(stdout);                                       \
    } while (0)

// Use it to display debug information. Turn it on/off in CMakeLists.txt
#ifdef LDEBUG
#define LOG_DEBUG(...)                                         \
    do                                                         \
    {                                                          \
        fprintf(stderr, "\033[33m[DEBUG]\033[0m" __VA_ARGS__); \
        fflush(stderr);                                        \
    } while (0)
#else
#define LOG_DEBUG(...)
#endif

// For generic error messages, without exiting the program
#define LOG_ERROR(...)                                         \
    do                                                         \
    {                                                          \
        fprintf(stderr, "\033[31m[ERROR]\033[0m" __VA_ARGS__); \
        fflush(stderr);                                        \
    } while (0)

// Use it when an unrecoverable error happened
#define LOG_FATAL(...)                                         \
    do                                                         \
    {                                                          \
        fprintf(stderr, "\033[31m[FATAL]\033[0m" __VA_ARGS__); \
        fflush(stderr);                                        \
        exit(EXIT_FAILURE);                                    \
    } while (0)

// For system call errors, which prints the system error message and exits the program
#define LOG_SYS_FATAL(...)                                              \
    do                                                                  \
    {                                                                   \
        fprintf(stderr, "\033[31m[SYS CALL FATAL]\033[0m" __VA_ARGS__); \
        perror("\033[31m[SYS CALL FATAL]\033[0m");                      \
        fflush(stderr);                                                 \
        exit(EXIT_FAILURE);                                             \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
