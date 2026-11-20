/* primos.c — Criba de Eratóstenes en C, compilada para RV32I (sin extensión M:
 * solo se usan sumas, restas y comparaciones). Lee N por consola y muestra los
 * primos <= N usando las syscalls SPIM. */
#define MAX_N 2000

static inline void print_int(int x) {
    register int a0 __asm__("a0") = x;
    register int a7 __asm__("a7") = 1;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}
static inline void print_str(const char* s) {
    register const char* a0 __asm__("a0") = s;
    register int a7 __asm__("a7") = 4;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}
static inline int read_int(void) {
    register int a0 __asm__("a0");
    register int a7 __asm__("a7") = 5;
    __asm__ volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return a0;
}

static unsigned char compuesto[MAX_N + 1];   /* .bss: el cargador la deja en 0 */
int primos[MAX_N];                           /* resultado, visible con 'words primos' */
int total;

__attribute__((noinline)) void criba(int n) {
    for (int i = 2; i <= n; i++) {
        if (compuesto[i]) continue;
        primos[total++] = i;
        for (int j = i + i; j <= n; j += i)  /* múltiplos por suma: sin mul */
            compuesto[j] = 1;
    }
}

int main(void) {
    print_str("Criba de Eratostenes (RV32I, programa en C cargado como ELF)\n");
    print_str("N = ");
    int n = read_int();
    if (n < 2 || n > MAX_N) {
        print_str("N debe estar entre 2 y 2000\n");
        return 1;
    }
    criba(n);
    for (int k = 0; k < total; k++) {
        print_int(primos[k]);
        print_str(k + 1 < total ? " " : "\n");
    }
    print_str("Total de primos: ");
    print_int(total);
    print_str("\n");
    return 0;
}
