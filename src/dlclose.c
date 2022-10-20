#include <stdio.h>
#include <utils.h>

/// Overriding the unloading of libraries to doing nothing.
i32 dlclose(void* p) {
    return 0; 
}
