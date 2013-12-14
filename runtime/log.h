#if !defined(CAUSAL_RUNTIME_LOG_H)
#define CAUSAL_RUNTIME_LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define INFO_COLOR "\033[01;34m"
#define WARNING_COLOR "\033[01;33m"
#define FATAL_COLOR "\033[01;31m"
#define SRC_COLOR "\033[34m"
#define END_COLOR "\033[0m"

#if defined(NDEBUG)
#  define INFO(fmt, ...)
#  define WARNING(fmt, ...) if(1) { fprintf(stderr, WARNING_COLOR fmt END_COLOR "\n", __VA_ARGS__); }
#  define FATAL(fmt, ...)   if(1) { fprintf(stderr, FATAL_COLOR fmt END_COLOR "\n", __VA_ARGS__); abort(); }
#else
#  define INFO(fmt, ...)    if(1) { fprintf(stderr, SRC_COLOR "[%s:%d] " INFO_COLOR fmt END_COLOR "\n", __FILE__, __LINE__, ##__VA_ARGS__); }
#  define WARNING(fmt, ...) if(1) { fprintf(stderr, SRC_COLOR "[%s:%d] " WARNING_COLOR fmt END_COLOR "\n", __FILE__, __LINE__, ##__VA_ARGS__); }
#  define FATAL(fmt, ...)   if(1) { fprintf(stderr, SRC_COLOR "[%s:%d] " FATAL_COLOR fmt END_COLOR "\n", __FILE__, __LINE__, ##__VA_ARGS__); abort(); }
#endif
#endif
