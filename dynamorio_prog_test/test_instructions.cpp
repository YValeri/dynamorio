
#include <random>
#include <iostream>
#include "immintrin.h"
#include "mmintrin.h"

#define aligned __attribute__((aligned(32)))

    /* SOURCES */

float   src0_f_sse[4] aligned, 
        src1_f_sse[4] aligned, 
        src2_f_sse[4] aligned,
        src0_f_avx[8] aligned,
        src1_f_avx[8] aligned,
        src2_f_avx[8] aligned;

double  src0_d_sse[2] aligned, 
        src1_d_sse[2] aligned, 
        src2_d_sse[2] aligned,
        src0_d_avx[4] aligned,
        src1_d_avx[4] aligned,
        src2_d_avx[4] aligned;

__m128   v_src0_f_sse, v_src1_f_sse, v_src2_f_sse;
__m128d  v_src0_d_sse, v_src1_d_sse, v_src2_d_sse;
__m256   v_src0_f_avx, v_src1_f_avx, v_src2_f_avx;
__m256d  v_src0_d_avx, v_src1_d_avx, v_src2_d_avx;

    /* RES */

float   res_f_sse[4] aligned,
        res_f_avx[8] aligned;

double  res_d_sse[2] aligned,
        res_d_avx[4] aligned;
    
__m128  v_res_f_sse;
__m128d v_res_d_sse;
__m256  v_res_f_avx;
__m256d v_res_d_avx;


template <typename FTYPE>
void print(FTYPE *values , unsigned int nb_values, const char *name) {
    std::cout << name << " : ";
    for(int i = 0 ; i < nb_values ; i++) {
        std::cout << values[i] << "\t\t";
    }
    std::cout << std::endl;
}



void init(unsigned int seed) {
    std::normal_distribution<double> dist_double;
    std::normal_distribution<float> dist_float;
    std::default_random_engine generator(seed);

    for(int i = 0 ; i < 8 ; i++) {
        if(i < 4) {
            src0_f_sse[i] = dist_float(generator);
            src1_f_sse[i] = dist_float(generator);
            src2_f_sse[i] = dist_float(generator);
        }
        src0_f_avx[i] = dist_float(generator);
        src1_f_avx[i] = dist_float(generator);
        src2_f_avx[i] = dist_float(generator);
    }

    for(int i = 0 ; i < 4 ; i++) {
        if(i < 2) {
            src0_d_sse[i] = dist_double(generator);
            src1_d_sse[i] = dist_double(generator);
            src2_d_sse[i] = dist_double(generator);
        }
        src0_d_avx[i] = dist_double(generator);
        src1_d_avx[i] = dist_double(generator);
        src2_d_avx[i] = dist_double(generator);
    }
}




void print_all_src() {
    print<float>(src0_f_sse , 4 , "src0_f_sse");
    print<float>(src1_f_sse , 4 , "src1_f_sse");
    print<float>(src2_f_sse , 4 , "src2_f_sse");
    print<float>(src0_f_avx , 8 , "src0_f_avx");
    print<float>(src1_f_avx , 8 , "src1_f_avx");
    print<float>(src2_f_avx , 8 , "src2_f_avx");

    print<double>(src0_d_sse , 2 , "src0_d_sse");
    print<double>(src1_d_sse , 2 , "src1_d_sse");
    print<double>(src2_d_sse , 2 , "src2_d_sse");
    print<double>(src0_d_avx , 4 , "src0_d_avx");
    print<double>(src1_d_avx , 4 , "src1_d_avx");
    print<double>(src2_d_avx , 4 , "src2_d_avx");
}

int main(int argc, char const *argv[])
{
    unsigned int seed = 42;

    if(argc == 2) seed = atoi(argv[1]);

    init(seed);

     std::cout << "\n\n\t***** SOURCES *****\n";
    print_all_src();

    v_src0_f_sse = _mm_load_ps(src0_f_sse);
    v_src1_f_sse = _mm_load_ps(src1_f_sse);
    v_src2_f_sse = _mm_load_ps(src2_f_sse);
    v_src0_d_sse = _mm_load_pd(src0_d_sse);
    v_src1_d_sse = _mm_load_pd(src1_d_sse);
    v_src2_d_sse = _mm_load_pd(src2_d_sse);
    v_src0_f_avx = _mm256_load_ps(src0_f_avx);
    v_src1_f_avx = _mm256_load_ps(src1_f_avx);
    v_src2_f_avx = _mm256_load_ps(src2_f_avx);
    v_src0_d_avx = _mm256_load_pd(src0_d_avx);
    v_src1_d_avx = _mm256_load_pd(src1_d_avx);
    v_src2_d_avx = _mm256_load_pd(src2_d_avx);

    /* *************************** SCALAR *************************** */
    std::cout << "\n\t***** SCALAR INSTRUCTIONS *****\n";
    
    /* ADD */
    v_res_f_sse = _mm_add_ss(v_src0_f_sse , v_src1_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_add_sd(v_src0_d_sse , v_src1_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);
    
    print<float>(res_f_sse , 4 , "ADDSS");
    print<double>(res_d_sse , 2 , "ADDSD");
    std::cout << std::endl;
    
    /* SUB */
    v_res_f_sse = _mm_sub_ss(v_src0_f_sse , v_src1_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_sub_sd(v_src0_d_sse , v_src1_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);
    
    print<float>(res_f_sse , 4 , "SUBSS");
    print<double>(res_d_sse , 2 , "SUBSD");
    std::cout << std::endl;
    

    /* MUL */
    v_res_f_sse = _mm_mul_ss(v_src0_f_sse , v_src1_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_mul_sd(v_src0_d_sse , v_src1_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);
    
    print<float>(res_f_sse , 4 , "MULSS");
    print<double>(res_d_sse , 2 , "MULSD");
    std::cout << std::endl;
    
    /* DIV */
    v_res_f_sse = _mm_div_ss(v_src0_f_sse , v_src1_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_div_sd(v_src0_d_sse , v_src1_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);
    
    print<float>(res_f_sse , 4 , "DIVSS");
    print<double>(res_d_sse , 2 , "DIVSD");
    std::cout << std::endl;

    /* FMADD */
    v_res_f_sse = _mm_fmadd_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fmadd_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);

    print<float>(res_f_sse , 4 , "FMADDSS");
    print<double>(res_d_sse , 2 , "FMADDSD");
    std::cout << std::endl;

    /* FMSUB */
    v_res_f_sse = _mm_fmsub_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fmsub_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);

    print<float>(res_f_sse , 4 , "FMSUBSS");
    print<double>(res_d_sse , 2 , "FMSUBSD");
    std::cout << std::endl;

    /* NEG FMADD */
    v_res_f_sse = _mm_fnmadd_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fnmadd_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);

    print<float>(res_f_sse , 4 , "FNMADDSS");
    print<double>(res_d_sse , 2 , "FNMADDSD");
    std::cout << std::endl;
    

    /* NEG FMSUB */
    v_res_f_sse = _mm_fnmsub_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fnmsub_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);

    print<float>(res_f_sse , 4 , "FNMSUBSS");
    print<double>(res_d_sse , 2 , "FNMSUBSD");
    std::cout << std::endl;
    

     /* *************************** VECT *************************** */
     std::cout << "\n\n\t***** VECTORISED INSTRUCTIONS *****\n";
    
    /* ADD */
    v_res_f_sse = _mm_add_ps(v_src0_f_sse , v_src1_f_sse);   _mm_store_ps(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_add_pd(v_src0_d_sse , v_src1_d_sse);   _mm_store_pd(res_d_sse , v_res_d_sse);
    v_res_f_avx = _mm256_add_ps(v_src0_f_avx , v_src1_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_add_pd(v_src0_d_avx , v_src1_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    
    print<float>(res_f_sse , 4 , "ADDPS");
    print<double>(res_d_sse , 2 , "ADDPD");
    print<float>(res_f_avx , 4 , "VADDPS");
    print<double>(res_d_avx , 2 , "VADDPD");
    std::cout << std::endl;

    
    /* SUB */
    v_res_f_sse = _mm_sub_ps(v_src0_f_sse , v_src1_f_sse); _mm_store_ps(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_sub_pd(v_src0_d_sse , v_src1_d_sse); _mm_store_pd(res_d_sse , v_res_d_sse);
    v_res_f_avx = _mm256_sub_ps(v_src0_f_avx , v_src1_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_sub_pd(v_src0_d_avx , v_src1_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    
    print<float>(res_f_sse , 4 , "SUBPS");
    print<double>(res_d_sse , 2 , "SUBPD");
    print<float>(res_f_avx , 4 , "VADDPS");
    print<double>(res_d_avx , 2 , "VADDPD");
    std::cout << std::endl;
    

    /* MUL */
    v_res_f_sse = _mm_mul_ps(v_src0_f_sse , v_src1_f_sse); _mm_store_ps(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_mul_pd(v_src0_d_sse , v_src1_d_sse); _mm_store_pd(res_d_sse , v_res_d_sse);
    v_res_f_avx = _mm256_mul_ps(v_src0_f_avx , v_src1_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_mul_pd(v_src0_d_avx , v_src1_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    
    print<float>(res_f_sse , 4 , "MULPS");
    print<double>(res_d_sse , 2 , "MULPD");
    print<float>(res_f_avx , 4 , "VADDPS");
    print<double>(res_d_avx , 2 , "VADDPD");
    std::cout << std::endl;
    
    /* DIV */
    v_res_f_sse = _mm_div_ps(v_src0_f_sse , v_src1_f_sse); _mm_store_ps(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_div_pd(v_src0_d_sse , v_src1_d_sse); _mm_store_pd(res_d_sse , v_res_d_sse);
    v_res_f_avx = _mm256_div_ps(v_src0_f_avx , v_src1_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_div_pd(v_src0_d_avx , v_src1_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    
    print<float>(res_f_sse , 4 , "DIVPS");
    print<double>(res_d_sse , 2 , "DIVPD");
    print<float>(res_f_avx , 4 , "VADDPS");
    print<double>(res_d_avx , 2 , "VADDPD");
    std::cout << std::endl;

    /* FMADD */
    v_res_f_sse = _mm_fmadd_ps(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fmadd_pd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);
    v_res_f_avx = _mm256_fmadd_ps(v_src0_f_avx , v_src1_f_avx, v_src2_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_fmadd_pd(v_src0_d_avx , v_src1_d_avx, v_src2_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);

    print<float>(res_f_sse , 4 , "FMADDPS");
    print<double>(res_d_sse , 2 , "FMADDPD");
    print<float>(res_f_avx , 4 , "VFMADDPS");
    print<double>(res_d_avx , 2 , "VFMADDPD");
    std::cout << std::endl;

    /* FMSUB */
    v_res_f_sse = _mm_fmsub_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fmsub_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);
    v_res_f_avx = _mm256_fmsub_ps(v_src0_f_avx , v_src1_f_avx, v_src2_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_fmsub_pd(v_src0_d_avx , v_src1_d_avx, v_src2_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);


    print<float>(res_f_sse , 4 , "FMSUBPS");
    print<double>(res_d_sse , 2 , "FMSUBPD");
    print<float>(res_f_avx , 4 , "VFMSUBPS");
    print<double>(res_d_avx , 2 , "VFMSUBPD");
    std::cout << std::endl;

    /* NEG FMADD */
    v_res_f_sse = _mm_fnmadd_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fnmadd_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);
    v_res_f_avx = _mm256_fnmadd_ps(v_src0_f_avx , v_src1_f_avx, v_src2_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_fnmadd_pd(v_src0_d_avx , v_src1_d_avx, v_src2_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);


    print<float>(res_f_sse , 4 , "FNMADDPS");
    print<double>(res_d_sse , 2 , "FNMADDPD");
    print<float>(res_f_avx , 4 , "VFNMADDPS");
    print<double>(res_d_avx , 2 , "VFNMADDPD");
    std::cout << std::endl;
    

    /* NEG FMSUB */
    v_res_f_sse = _mm_fnmsub_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fnmsub_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);
    v_res_f_avx = _mm256_fnmsub_ps(v_src0_f_avx , v_src1_f_avx, v_src2_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_fnmsub_pd(v_src0_d_avx , v_src1_d_avx, v_src2_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);

    print<float>(res_f_sse , 4 , "FNMSUBPS");
    print<double>(res_d_sse , 2 , "FNMSUBPD");
    print<float>(res_f_avx , 4 , "VFNMSUBPS");
    print<double>(res_d_avx , 2 , "VFNMSUBPD");
    std::cout << std::endl;


    return 0;
}
