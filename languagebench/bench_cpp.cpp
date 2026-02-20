#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>

using namespace std;
using namespace std::chrono;

struct Obj {
    double val;
};

static void printResult(const string& name, double ops, double sec) {
    cout << "  " << left << setw(15) << name << " | "
         << right << setw(15) << fixed << setprecision(2) << ops << " OPS/sec | "
         << fixed << setprecision(4) << sec << "s\n";
}

static void printHeader() {
    cout << "  -------------------------------------------------------------\n";
    cout << "  Benchmark       |     Performance     | Time\n";
    cout << "  -------------------------------------------------------------\n";
}

static double get_time() {
    return duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count() / 1e9;
}

static void benchInt() {
    long long limit = 1000000000LL;
    double start = get_time();

    volatile long long i = 0;
    while (i < limit) {
        i = i + 1;
    }

    double sec = get_time() - start;
    if (sec == 0.0) sec = 1e-9;
    printResult("Integer Add", (double)limit / sec, sec);
}

static void benchDouble() {
    long long limit = 100000000LL;
    double start = get_time();

    volatile double v = 0.0;
    for (long long i = 0; i < limit; i++) {
        v = v + 1.1;
    }

    double sec = get_time() - start;
    if (sec == 0.0) sec = 1e-9;
    printResult("Double Arith", (double)limit / sec, sec);
}

static void benchString() {
    long long limit = 50000;
    string s;
    s.reserve(64);

    double start = get_time();

    for (long long i = 0; i < limit; i++) {
        s += 'a';
    }

    double sec = get_time() - start;
    if (sec == 0.0) sec = 1e-9;
    printResult("String Concat", (double)limit / sec, sec);
}

static void benchArray() {
    long long limit = 1000000LL;
    vector<double> arr;

    double start = get_time();

    for (long long i = 0; i < limit; i++) {
        arr.push_back((double)i);
    }

    double sec = get_time() - start;
    if (sec == 0.0) sec = 1e-9;
    printResult("Array Push", (double)limit / sec, sec);
}

static void benchStruct() {
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

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    cout << ">>> C++ (GCC -O3) Benchmark Suite <<<\n";
    printHeader();
    benchInt();
    benchDouble();
    benchString();
    benchArray();
    benchStruct();
    cout << "  -------------------------------------------------------------\n";
    cout << "\n";
    return 0;
}
