#include "test_instructions.h"

int main(int argc, char const *argv[])
{   
    /* Default seed get a static test case for comparison when running with dynamoRIO */
    unsigned int seed = 42;

    if(argc == 2) seed = atoi(argv[1]);

    init(seed);

    std::cout << "\n\t***** SOURCES *****\n";
    print_all_src();

    v_src0_f_avx = _mm256_load_ps(src0_f_avx);
    v_src1_f_avx = _mm256_load_ps(src1_f_avx);
    v_src2_f_avx = _mm256_load_ps(src2_f_avx);
    v_src0_d_avx = _mm256_load_pd(src0_d_avx);
    v_src1_d_avx = _mm256_load_pd(src1_d_avx);
    v_src2_d_avx = _mm256_load_pd(src2_d_avx);


    std::cout << "\n\t***** VECTORISED INSTRUCTIONS AVX only *****\n\n";

    v_res_f_avx = _mm256_add_ps(v_src0_f_avx , v_src1_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_add_pd(v_src0_d_avx , v_src1_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    print<float>(res_f_avx , 4 , "VADDPS");
    print<double>(res_d_avx , 2 , "VADDPD");
    std::cout << std::endl;

    v_res_f_avx = _mm256_sub_ps(v_src0_f_avx , v_src1_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_sub_pd(v_src0_d_avx , v_src1_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    print<float>(res_f_avx , 4 , "VSUBPS");
    print<double>(res_d_avx , 2 , "VSUBPD");
    std::cout << std::endl;


    v_res_f_avx = _mm256_mul_ps(v_src0_f_avx , v_src1_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_mul_pd(v_src0_d_avx , v_src1_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    print<float>(res_f_avx , 4 , "VMULPS");
    print<double>(res_d_avx , 2 , "VMULPD");
    std::cout << std::endl;

    v_res_f_avx = _mm256_div_ps(v_src0_f_avx , v_src1_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_div_pd(v_src0_d_avx , v_src1_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    print<float>(res_f_avx , 4 , "VDIVPS");
    print<double>(res_d_avx , 2 , "VDIVPD");
    std::cout << std::endl;
    
    return 0;
}
