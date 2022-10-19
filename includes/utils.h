#ifndef UTILS_H_
#define UTILS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef size_t usize;

typedef enum {
    LOG_PANIC,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_TRACE,
} LogLevel;

LogLevel get_log_level();
void set_log_level(LogLevel level);

void __logger(LogLevel lvl, const char *fmt, ...);
void __panic(const char *fmt, ...);
void __array(const char **msgs, u32 len, const char *format, ...);

/* Due to C being a weird language:
 *
 * "[]" fmt ##__VA_ARGS__
 *
 * can expand to something like:
 *
 * "[]" "%d" 100
 *
 * but since string literals concatenated in C, we end up getting:
 *
 * "[] %d", 100 */

#define trace(fmt, ...)                                           \
    __logger(                                                     \
        LOG_TRACE,                                                \
        "[%s:%d]\x1B[0m " fmt, __FILE__, __LINE__, ##__VA_ARGS__  \
    )

#define info(fmt, ...)                                            \
    __logger(                                                     \
        LOG_INFO,                                                 \
        "[%s:%d]\x1B[0m " fmt, __FILE__, __LINE__, ##__VA_ARGS__  \
    )

#define warn(fmt, ...)                                            \
    __logger(                                                     \
        LOG_WARN,                                                 \
        "[%s:%d]\x1B[0m " fmt, __FILE__, __LINE__, ##__VA_ARGS__  \
    )

#define error(fmt, ...)                                           \
    __logger(                                                     \
        LOG_ERROR,                                                \
        "[%s:%d]\x1B[0m " fmt, __FILE__, __LINE__, ##__VA_ARGS__  \
    )

#define panic(fmt, ...)                                           \
    __logger(                                                     \
        LOG_PANIC,                                                \
        "[%s:%d]\x1B[0m thread panicked with: '" fmt "'",         \
        __FILE__, __LINE__, ##__VA_ARGS__                         \
    );

#define trace_array(msgs, len, fmt, ...)                          \
    __array(                                                      \
        msgs,                                                     \
        len,                                                      \
        "[%s:%d]\x1B[0m " fmt, __FILE__, __LINE__, ##__VA_ARGS__  \
    )

void *vmalloc(usize size);
void *vrealloc(void* ptr, usize size);

struct timespec now();
struct timespec time_elapsed(struct timespec start);

char *read_binary(const char *path, u32 *bytes_read);

u32 clamp(u32 val, u32 min, u32 max);
u32 min(u32 a, u32 b);
u32 max(u32 a, u32 b);

#endif // UTILS_H_
