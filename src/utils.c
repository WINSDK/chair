#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <sys/stat.h>

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

#if _POSIX_C_SOURCE >= 199309L
#error no support for clocks
#endif

struct timespec now() {
    struct timespec time;

#if _POSIX_THREAD_CPUTIME != 0
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);
#else
    clock_gettime(CLOCK_REALTIME, &time);
#endif

    return time;
}

struct timespec time_elapsed(struct timespec start) {
    struct timespec temp, time;

#if _POSIX_THREAD_CPUTIME != 0
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);
#else
    clock_gettime(CLOCK_REALTIME, &time);
#endif

    if ((time.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = time.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + time.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = time.tv_sec - start.tv_sec;
        temp.tv_nsec = time.tv_nsec - start.tv_nsec;
    }

    return temp;
}

/// Reads file and returns NULL if it failed
char* read_binary(const char *path, u32 *bytes_read) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;

    struct stat info;
    if (stat(path, &info) == -1) return NULL;
    size_t size = (size_t)info.st_size;

    char* bytes = malloc(size);
    *bytes_read = fread(bytes, 1, size, file);

    if (bytes == NULL) return NULL;
    if (fclose(file) != 0 || *bytes_read != size) {
        free(bytes);
        return NULL;
    }

    return bytes;
}
