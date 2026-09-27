#pragma once

#include <cstdio>

/* Minimal printf-style logging shim. Swap the body for the platform logger
 * (e.g. csio STRLOG) when integrating; the call sites never change. */

#define LOG_ERROR(...)                                                         \
    do                                                                         \
    {                                                                          \
        std::fprintf(stderr, "[E] " __VA_ARGS__);                              \
        std::fprintf(stderr, "\n");                                            \
    } while (0)

#define LOG_WARNING(...)                                                       \
    do                                                                         \
    {                                                                          \
        std::fprintf(stderr, "[W] " __VA_ARGS__);                              \
        std::fprintf(stderr, "\n");                                            \
    } while (0)

#define LOG_INFO(...)                                                          \
    do                                                                         \
    {                                                                          \
        std::fprintf(stdout, "[I] " __VA_ARGS__);                              \
        std::fprintf(stdout, "\n");                                            \
    } while (0)

#define LOG_DEBUG(...)                                                         \
    do                                                                         \
    {                                                                          \
        std::fprintf(stdout, "[D] " __VA_ARGS__);                              \
        std::fprintf(stdout, "\n");                                            \
    } while (0)
