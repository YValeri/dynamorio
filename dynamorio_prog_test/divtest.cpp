#include <iostream>
int main(int argc, char const *argv[])
{
    double a[] __attribute__ ((aligned(32))) = {3.765*argc, 6.875*argc, 65, 32};
    double b[] __attribute__ ((aligned(32))) = {3.1*argc, 6.1*argc, 32, 12};
    double c[] __attribute__ ((aligned(32))) = {3, 4, 5, 6};
    asm volatile("\tvmovaps %%xmm0, %1\n"
            "\tvmovaps %%xmm1, %2\n"
            "\tdivpd %%xmm0, %%xmm1\n"
            "\tvmovaps %0, %%xmm0\n" 
            : "=m"(c)
            : "m"(a), "m"(b));
    printf("Div SSE : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);
    asm volatile("\tvmovapd %%ymm0, %1\n"
            "\tvmovapd %%ymm1, %2\n"
            "\tvdivpd %%xmm2, %%xmm0, %%xmm1\n"
            "\tvmovapd %0, %%ymm2\n" 
            : "=m"(c)
            : "m"(a), "m"(b));
    printf("Div AVX : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);

    asm volatile("\tvmovaps %%xmm0, %1\n"
            "\tvmovaps %%xmm1, %2\n"
            "\tsubpd %%xmm0, %%xmm1\n"
            "\tvmovaps %0, %%xmm0\n" 
            : "=m"(c)
            : "m"(a), "m"(b));
    printf("Sub SSE : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);
    asm volatile("\tvmovapd %%ymm0, %1\n"
            "\tvmovapd %%ymm1, %2\n"
            "\tvsubpd %%xmm2, %%xmm0, %%xmm1\n"
            "\tvmovapd %0, %%ymm2\n" 
            : "=m"(c)
            : "m"(a), "m"(b));
    printf("Sub AVX : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);
    return 0;
}