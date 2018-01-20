#ifndef ERROR_H
#define ERROR_H

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#define INFO    0
#define ERROR   1
#define PANIC   2

#define LOG_LEVEL   INFO

static const char *types[] = {
    "INFO ",
    "ERROR",
    "PANIC"
};

static inline char* strupr(char* s)
{
    char* tmp = s;

    while (*tmp) {
        *tmp = toupper((unsigned char) *tmp);
        tmp++;
    }

    return s;
}

static inline void __log(FILE *out, int level, const char *prefix, const char *s, va_list ap)
{
    char prefix_buf[20];
    char time_buf[26];
    time_t timer;
    struct tm* tm_info;
    
    if (level < LOG_LEVEL) return;
    
    time(&timer);
    tm_info = localtime(&timer);

    strftime(time_buf, 26, "%H:%M:%S", tm_info); 
    strncpy(prefix_buf, prefix, 20);
    
    fprintf(out, "%s - %-6s : [%s] ", time_buf, strupr(prefix_buf), types[level]);
    vfprintf(out, s, ap);
    
    if (level > INFO)
        fprintf(out, " : %s", strerror(errno));
    
    fprintf(out, "\n");}

#define LOG_FUNCTION(prefix, type, name)                \
static inline void name(const char *s, ...)             \
{                                                       \
    va_list args;                                       \
    va_start(args, s);                                  \
    __log(stdout, type, #prefix, s, args);              \
    va_end(args);                                       \}                                                       \

#define LOG_FUNCTIONS(prefix)                           \
LOG_FUNCTION(prefix, INFO, log_##prefix)                \
LOG_FUNCTION(prefix, ERROR, log_##prefix##_error)       \
LOG_FUNCTION(prefix, PANIC, log_##prefix##_panic)       \


#define PANIC_FUNCTION(prefix, close)                   \
static inline void prefix##_panic(const char *s, ...)   \
{                                                       \
    va_list args;                                       \
    va_start(args, s);                                  \
    __log(stdout, PANIC, #prefix, s, args);             \
    va_end(args);                                       \
    close(-1);                                          \}                                                       \

#endif
