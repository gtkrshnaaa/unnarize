#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double val;
} Obj;

static void printResult(const char* name, double ops, double sec) {
    printf("  %-15s | %15.2f OPS/sec | %.4fs\n", name, ops, sec);
}

static void printHeader() {
    printf("  -------------------------------------------------------------\n");
    printf("  Benchmark       |     Performance     | Time\n");
    printf("  -------------------------------------------------------------\n");
}

static double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void benchInt(void) {
    long long limit = 1000000000LL;
    double start = get_time();

    volatile long long i = 0;
    while (i < limit) {
        i++;
    }

    double sec = get_time() - start;
    if (sec == 0.0) sec = 1e-9;
    printResult("Integer Add", (double)limit / sec, sec);
}

static void benchDouble(void) {
    long long limit = 100000000LL;
    double start = get_time();

    volatile double val = 0.0;
    for (long long i = 0; i < limit; i++) {
        val += 1.1;
    }

    double sec = get_time() - start;
    if (sec == 0.0) sec = 1e-9;
    printResult("Double Arith", (double)limit / sec, sec);
}

static void benchString(void) {
    long long limit = 50000;

    // Pre-allocate a reasonably sized buffer to avoid excessive realloc calls.
    size_t cap = 64;
    char* s = (char*)malloc(cap);
    if (!s) return;
    s[0] = '\0';
    size_t len = 0;

    double start = get_time();

    for (long long i = 0; i < limit; i++) {
        if (len + 2 > cap) {
            cap *= 2;
            char* tmp = (char*)realloc(s, cap);
            if (!tmp) { free(s); return; }
            s = tmp;
        }
        s[len]     = 'a';
        s[len + 1] = '\0';
        len++;
    }

    double sec = get_time() - start;
    if (sec == 0.0) sec = 1e-9;
    printResult("String Concat", (double)limit / sec, sec);
    free(s);
}

static void benchArray(void) {
    long long limit = 1000000LL;

    size_t cap = 16;
    double* arr = (double*)malloc(cap * sizeof(double));
    if (!arr) return;
    size_t size = 0;

    double start = get_time();

    for (long long i = 0; i < limit; i++) {
        if (size >= cap) {
            cap *= 2;
            double* tmp = (double*)realloc(arr, cap * sizeof(double));
            if (!tmp) { free(arr); return; }
            arr = tmp;
        }
        arr[size++] = (double)i;
    }

    double sec = get_time() - start;
    if (sec == 0.0) sec = 1e-9;
    printResult("Array Push", (double)limit / sec, sec);
    free(arr);
}

static void benchStruct(void) {
    long long limit = 50000000LL;
    Obj o;
    o.val = 0.0;

    double start = get_time();

    volatile double x = 0.0;
    for (long long i = 0; i < limit; i++) {
        o.val = (double)i;
        x = o.val;
    }
    (void)x;

    double sec = get_time() - start;
    if (sec == 0.0) sec = 1e-9;
    printResult("Struct Access", (double)limit / sec, sec);
}

int main(void) {
    printf(">>> C (GCC -O3) Benchmark Suite <<<\n");
    printHeader();
    benchInt();
    benchDouble();
    benchString();
    benchArray();
    benchStruct();
    printf("  -------------------------------------------------------------\n");
    printf("\n");
    return 0;
}
