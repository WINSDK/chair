#include <stdio.h>

/// Overriding the unloading of libraries to doing nothing.
int dlclose(void* p) { 
    return 0; 
}
