// ThetaZ CRT integration test: argc/argv, atexit, math, locale.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <locale.h>

static void on_exit_a(void) { printf("atexit-a\n"); }
static void on_exit_b(void) { printf("atexit-b\n"); }

int main(int argc, char** argv) {
    printf("argc=%d\n", argc);
    if (argc > 1) printf("arg1=%s\n", argv[1]);

    printf("sqrt(16)=%d\n", (int)sqrt(16.0));
    printf("sqrt2=%d\n", (int)(sqrt(2.0) * 1000.0));

    char* set = setlocale(LC_ALL, "C");
    printf("locale=%s\n", set ? set : "null");

    atexit(on_exit_a);
    atexit(on_exit_b);
    return 0;
}
