#ifndef ERROR_H
#define ERROR_H

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#define OUT_STD     0
#define OUT_FILE    1

#define INFO    0
#define ERROR   1
#define PANIC   2

#ifndef LOG_LEVEL
#define LOG_LEVEL   INFO
#endif

#ifndef LOG_OUT
#define LOG_OUT OUT_STD
#endif

static FILE *log_file = NULL;

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

static inline void __log(int level, const char *prefix, const char *s, va_list ap)
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
    
    FILE *out;
    if (LOG_OUT == OUT_STD) {        if (level > INFO) {            out = stderr;
        } else {            out = stdout;
        }
    } else if (LOG_OUT == OUT_FILE) {        if (log_file == NULL) {            return;
        } else {            out = log_file;
        }
    }
    
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
    __log(type, #prefix, s, args);                      \
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
    __log(PANIC, #prefix, s, args);                     \
    va_end(args);                                       \
    close(-1);                                          \}                                                       \

#endif
