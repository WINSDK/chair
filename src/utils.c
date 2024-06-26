#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <unistd.h>

#include "utils.h"

void breek() {
#if defined(__x86_64__) || defined(_M_X64)
            __asm__("int3");
#elif defined(__aarch64__) || defined(_M_ARM64)
            __asm__("brk #1");
#endif
}

static atomic_uint LEVEL = LOG_INFO;

LogLevel get_log_level() {
    return atomic_load(&LEVEL);
}

void set_log_level(LogLevel level) {
    atomic_store(&LEVEL, level);
}

void __logger(LogLevel lvl, const char *fmt, ...) {
    FILE *output = stdout;
    va_list ap;

    if (get_log_level() < lvl)
        return;

    va_start(ap, fmt);

    if (lvl <= LOG_ERROR)
        output = stderr;

    switch(lvl) {
    case LOG_TRACE:
        fprintf(output, "\x1b[1;38;5;4m");
        break;
    case LOG_INFO:
        fprintf(output, "\x1b[1;38;5;2m");
        break;
    case LOG_WARN:
        fprintf(output, "\x1b[1;38;5;3m");
        break;
    case LOG_ERROR:
        fprintf(output, "\x1b[1;38;5;1m");
        break;
    case LOG_PANIC:
        fprintf(output, "\x1b[1;38;5;1m");
        break;
    }

    vfprintf(output, fmt, ap);
    fputc('\n', output);
    va_end(ap);

    if (lvl == LOG_PANIC) {
        if (get_log_level() == LOG_TRACE)
            breek();

        exit(1);
    }
}

void __array(const char **msgs, u32 len, const char *format, ...) {
    if (get_log_level() < LOG_TRACE)
        return;

    va_list ap;
    va_start(ap, format);
    printf("\x1b[1;38;5;4m");
    vprintf(format, ap);

    printf("[");

    if (len == 0) {
        printf("]\n");
        fflush(stdout);
        va_end(ap);
        return;
    }

    for (u32 idx = 0; idx < len; idx++) {
        if (idx == len - 1)
            printf("%s]", msgs[idx]);
        else
            printf("%s, ", msgs[idx]);
    }

    fputc('\n', stdout);
    va_end(ap);
}

void *vmalloc(usize size) {
    void *data = malloc(size);

    if (data == NULL)
        panic("failed to allocate 0x%x bytes", size);

    return data;
}

void *vrealloc(void *ptr, usize size) {
    void *data = realloc(ptr, size);

    if (data == NULL)
        panic("failed to reallocate 0x%x bytes", size);

    return data;
}

// Force `val` to be between `min` and `max`.
u32 clamp(u32 val, u32 min, u32 max) {
    return val < min ? min : max;
}

void now(struct timespec *time) {

#ifdef __linux__
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, time);
#else
    timespec_get(time, TIME_UTC);
#endif

}

/* Calculates the numbers of seconds elapseds since some starting point */
f64 time_elapsed(struct timespec *start) {
    struct timespec diff, time;

    now(&time);

    // calculate the difference between `start` and now
    if ((time.tv_nsec - start->tv_nsec) < 0) {
        diff.tv_sec = time.tv_sec - start->tv_sec - 1;
        diff.tv_nsec = 1000000000 + time.tv_nsec - start->tv_nsec;
    } else {
        diff.tv_sec = time.tv_sec - start->tv_sec;
        diff.tv_nsec = time.tv_nsec - start->tv_nsec;
    }

    return (f64)(diff.tv_sec * 1000000000 + diff.tv_nsec) * 1.0e-9;
}

/// Reads file and returns NULL if it failed
char *read_binary(const char *path, u32 *bytes_read) {
    FILE *fh = fopen(path, "rb");

    if (fh == NULL)
        return NULL;

    // read file length
    fseek(fh, 0, SEEK_END);
    usize size = ftell(fh);
    rewind(fh);

    char *bytes = vmalloc(size);
    *bytes_read = fread(bytes, 1, size, fh);

    fclose(fh);

    if (*bytes_read != size) {
        free(bytes);
        return NULL;
    }

    return bytes;
}
