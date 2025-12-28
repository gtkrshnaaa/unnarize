#include "ucore_timer.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>

// Native ucoreTimer.now()
// Returns current monotonic time in milliseconds (double)
static Value utimer_now(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    // Convert to milliseconds
    double ms = (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
    
    Value v;
    v.type = VAL_FLOAT;
    v.floatVal = ms;
    return v;
}

// Native ucoreTimer.sleep(ms)
static Value utimer_sleep(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_INT) {
        printf("Error: ucoreTimer.sleep expects 1 int argument (ms).\n");
        return (Value){VAL_INT, .intVal = 0};
    }
    
    int ms = args[0].intVal;
    usleep(ms * 1000); // usleep takes microseconds
    
    return (Value){VAL_INT, .intVal = 0};
}

void registerUCoreTimer(VM* vm) {
    registerNativeFunction(vm, "ucoreTimer.now", utimer_now);
    registerNativeFunction(vm, "ucoreTimer.sleep", utimer_sleep);
}
