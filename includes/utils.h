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


/* --------------------- hashing macro --------------------- */

/* Source: https://lolengine.net/blog/2011/12/20/cpp-constant-string-hash */

#define H1(s,i,x)   (x*65599u+(u8)s[(i)<strlen(s)?strlen(s)-1-(i):strlen(s)])
#define H4(s,i,x)   H1(s,i,H1(s,i+1,H1(s,i+2,H1(s,i+3,x))))
#define H16(s,i,x)  H4(s,i,H4(s,i+4,H4(s,i+8,H4(s,i+12,x))))
#define H64(s,i,x)  H16(s,i,H16(s,i+16,H16(s,i+32,H16(s,i+48,x))))
#define H256(s,i,x) H64(s,i,H64(s,i+64,H64(s,i+128,H64(s,i+192,x))))

#define HASH(s)    ((u32)(H256(s,0,0)^(H256(s,0,0)>>16)))

/* --------------------------------------------------------- */

void *vmalloc(usize size);
void *vcalloc(usize size);
void *vrealloc(void* ptr, usize size);

void now(struct timespec *time);
f64 time_elapsed(struct timespec *start);

char *read_binary(const char *path, u32 *bytes_read);

u32 clamp(u32 val, u32 min, u32 max);

#endif // UTILS_H_
