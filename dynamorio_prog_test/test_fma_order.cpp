#include <iostream>

int main(int argc , char *argv[]) {

    double a[] __attribute__ ((aligned(32))) = {1.15, 7.32, 4.63, 6.24};
    double b[] __attribute__ ((aligned(32))) = {2.03, 8.32, 1.06, 9.32};
    double c[] __attribute__ ((aligned(32))) = {8.31, 11.1, 5.26, 8.94};

    float aa[] __attribute__ ((aligned(32))) = {1.15, 7.32, 4.63, 6.24, 2.03, 8.32, 1.06, 9.32};
    float bb[] __attribute__ ((aligned(32))) = {2.03, 8.32, 1.06, 9.32, 1.15, 7.32, 4.63, 6.24,};
    float cc[] __attribute__ ((aligned(32))) = {8.31, 11.1, 5.26, 8.94, 8.32, 1.06, 9.32, 1.15};

    printf("\t\t*********** SCALAR DOUBLE ***********\n");

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd231sd %%xmm0, %%xmm1, %%xmm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(c)
                    : "m"(a), "m"(b), "m"(c));
    
    printf("FMA 231 : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd213sd %%xmm0, %%xmm1, %%xmm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(c)
                    : "m"(a), "m"(b), "m"(c));
    
    printf("FMA 213 : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd132sd %%xmm0, %%xmm1, %%xmm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(c)
                    : "m"(a), "m"(b), "m"(c));
    
    printf("FMA 132 : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);

    printf("\t\t*********** SCALAR FLOAT ***********\n");

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd231ss %%xmm0, %%xmm1, %%xmm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(cc)
                    : "m"(aa), "m"(bb), "m"(cc));
    
    printf("FMA 231 : %lf %lf %lf %lf %lf %lf %lf %lf\n", cc[0], cc[1], cc[2], cc[3], cc[4], cc[5], cc[6], cc[7]);

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd213ss %%xmm0, %%xmm1, %%xmm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(cc)
                    : "m"(aa), "m"(bb), "m"(cc));
    
    printf("FMA 231 : %lf %lf %lf %lf %lf %lf %lf %lf\n", cc[0], cc[1], cc[2], cc[3], cc[4], cc[5], cc[6], cc[7]);

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"                
                    "\tvfmadd132ss %%xmm0, %%xmm1, %%xmm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(cc)
                    : "m"(aa), "m"(bb), "m"(cc));
    
    printf("FMA 231 : %lf %lf %lf %lf %lf %lf %lf %lf\n", cc[0], cc[1], cc[2], cc[3], cc[4], cc[5], cc[6], cc[7]);


    printf("\t\t*********** PACKED DOUBLE ***********\n");

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd231pd %%ymm0, %%ymm1, %%ymm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(c)
                    : "m"(a), "m"(b), "m"(c));
    
    printf("FMA 231 : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd213pd %%ymm0, %%ymm1, %%ymm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(c)
                    : "m"(a), "m"(b), "m"(c));
    
    printf("FMA 213 : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd132pd %%ymm0, %%ymm1, %%ymm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(c)
                    : "m"(a), "m"(b), "m"(c));
    
    printf("FMA 132 : %lf %lf %lf %lf\n", c[0], c[1], c[2], c[3]);

    printf("\t\t*********** PACKED FLOAT ***********\n");

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd231ps %%ymm0, %%ymm1, %%ymm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(cc)
                    : "m"(aa), "m"(bb), "m"(cc));
    
    printf("FMA 231 : %lf %lf %lf %lf %lf %lf %lf %lf\n", cc[0], cc[1], cc[2], cc[3], cc[4], cc[5], cc[6], cc[7]);

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"
                    "\tvfmadd213ps %%ymm0, %%ymm1, %%ymm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(cc)
                    : "m"(aa), "m"(bb), "m"(cc));
    
    printf("FMA 231 : %lf %lf %lf %lf %lf %lf %lf %lf\n", cc[0], cc[1], cc[2], cc[3], cc[4], cc[5], cc[6], cc[7]);

    asm volatile(   "\tvmovaps %%ymm0, %1\n"
                    "\tvmovaps %%ymm1, %2\n"
                    "\tvmovaps %%ymm2, %3\n"                
                    "\tvfmadd132ps %%ymm0, %%ymm1, %%ymm2\n"
                    "\tvmovaps %0, %%ymm0\n" 
                    : "=m"(cc)
                    : "m"(aa), "m"(bb), "m"(cc));
    
    printf("FMA 231 : %lf %lf %lf %lf %lf %lf %lf %lf\n", cc[0], cc[1], cc[2], cc[3], cc[4], cc[5], cc[6], cc[7]);
        


    
    
    
    return 0;
}