#ifndef CHAIR_H_
#define CHAIR_H_ 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_TRACE,
} LogLevel;

LogLevel get_log_level();
void set_log_level(LogLevel level);

void trace(const char *format, ...);
void info(const char *format, ...);
void warn(const char *format, ...);
void error(const char *format, ...);
void panic(const char *format, ...);

void trace_array(const char** msgs, u32 len, const char *format, ...);

u32 clamp(u32 val, u32 min, u32 max);
u32 min(u32 a, u32 b);
u32 max(u32 a, u32 b);

#endif // CHAIR_H_
