#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <stdatomic.h>
#include <stdarg.h>

#include "chair.h"

static atomic_uint LEVEL = LOG_WARN;

LogLevel get_log_level() {
    return atomic_load(&LEVEL);
}

void set_log_level(LogLevel level) {
    atomic_store(&LEVEL, level);
}

void info(const char *format, ...) {
    if (get_log_level() < LOG_INFO) return;

    va_list ap;
    va_start(ap, format);
    printf("[+] ");
    vprintf(format, ap);
    putchar('\n');
    fflush(stdout);
    va_end(ap);
}

void trace(const char *format, ...) {
    if (get_log_level() < LOG_TRACE) return;

    va_list ap;
    va_start(ap, format);
    printf("[?] ");
    vprintf(format, ap);
    putchar('\n');
    fflush(stdout);
    va_end(ap);
}


void trace_array(const char** msgs, u32 len, const char *format, ...) {
    if (get_log_level() < LOG_TRACE) return;

    va_list ap;
    va_start(ap, format);
    printf("[?] ");
    vprintf(format, ap);

    printf("[");
    for (u32 idx = 0; idx < len; idx++) {
        if (idx == len - 1)
            printf("%s]", msgs[idx]);
        else
            printf("%s, ", msgs[idx]);
    }

    putchar('\n');
    fflush(stdout);
    va_end(ap);
}

void warn(const char *format, ...) {
    if (get_log_level() < LOG_WARN) return;

    va_list ap;
    va_start(ap, format);
    printf("[!] ");
    vprintf(format, ap);
    putchar('\n');
    fflush(stdout);
    va_end(ap);
}

void error(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    printf("[-] ");
    vprintf(format, ap);
    putchar('\n');
    fflush(stdout);
    va_end(ap);
}

noreturn void panic(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "[-] thread panicked with: `");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "`\n");
    fflush(stderr);
    va_end(ap);
    exit(1);
}
