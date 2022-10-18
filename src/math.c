#include "utils.h"

/// Force `a` to be between `b` and `c`.
u32 clamp(u32 val, u32 min, u32 max) {
    if (val < min) {
        return min;
    }

    if (val > max) {
        return max;
    }

    return val;
}

u32 max(u32 a, u32 b) {
    if (a > b) return a;
    return b;
}

u32 min(u32 a, u32 b) {
    if (a < b) return a;
    return b;
}
