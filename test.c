
extern int test();

static int fib(int i) {
    if (i == 0)
        return 1;
    return fib(i-1) + i;
}

int main() {
    return fib(0) + test();
}