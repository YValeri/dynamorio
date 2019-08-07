#include "test_instructions.h"


int main(int argc, char const *argv[])
{
    /* Default seed get a static test case for comparison when running with dynamoRIO */
    unsigned int seed = 42;

    if(argc == 2) seed = atoi(argv[1]);

    init(seed);

    std::cout << "\n\t***** SOURCES *****\n";
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

    std::cout << std::endl;

     /* SCALAR FMADD */
    v_res_f_sse = _mm_fmadd_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fmadd_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);

    print<float>(res_f_sse , 4 , "FMADDSS");
    print<double>(res_d_sse , 2 , "FMADDSD");
    std::cout << std::endl;

    /* SCALAR FMSUB */
    v_res_f_sse = _mm_fmsub_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fmsub_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);

    print<float>(res_f_sse , 4 , "FMSUBSS");
    print<double>(res_d_sse , 2 , "FMSUBSD");
    std::cout << std::endl;

    /* SCALAR NEG FMADD */
    v_res_f_sse = _mm_fnmadd_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fnmadd_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);

    print<float>(res_f_sse , 4 , "FNMADDSS");
    print<double>(res_d_sse , 2 , "FNMADDSD");
    std::cout << std::endl;
    

    /* SCALAR NEG FMSUB */
    v_res_f_sse = _mm_fnmsub_ss(v_src0_f_sse , v_src1_f_sse , v_src2_f_sse); _mm_store_ss(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_fnmsub_sd(v_src0_d_sse , v_src1_d_sse , v_src2_d_sse); _mm_store_sd(res_d_sse , v_res_d_sse);

    print<float>(res_f_sse , 4 , "FNMSUBSS");
    print<double>(res_d_sse , 2 , "FNMSUBSD");
    std::cout << std::endl;


    v_res_f_avx = _mm256_fmadd_ps(v_src0_f_avx , v_src1_f_avx, v_src2_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_fmadd_pd(v_src0_d_avx , v_src1_d_avx, v_src2_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    print<float>(res_f_avx , 4 , "VFMADDPS");
    print<double>(res_d_avx , 2 , "VFMADDPD");
    std::cout << std::endl;

    v_res_f_avx = _mm256_fmsub_ps(v_src0_f_avx , v_src1_f_avx, v_src2_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_fmsub_pd(v_src0_d_avx , v_src1_d_avx, v_src2_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    print<float>(res_f_avx , 4 , "VFMSUBPS");
    print<double>(res_d_avx , 2 , "VFMSUBPD");
    std::cout << std::endl;

    v_res_f_avx = _mm256_fnmadd_ps(v_src0_f_avx , v_src1_f_avx, v_src2_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_fnmadd_pd(v_src0_d_avx , v_src1_d_avx, v_src2_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    print<float>(res_f_avx , 4 , "VFNMADDPS");
    print<double>(res_d_avx , 2 , "VFNMADDPD");
    std::cout << std::endl;

    v_res_f_avx = _mm256_fnmsub_ps(v_src0_f_avx , v_src1_f_avx, v_src2_f_avx); _mm256_store_ps(res_f_avx , v_res_f_avx);
    v_res_d_avx = _mm256_fnmsub_pd(v_src0_d_avx , v_src1_d_avx, v_src2_d_avx); _mm256_store_pd(res_d_avx , v_res_d_avx);
    print<float>(res_f_avx , 4 , "VFNMSUBPS");
    print<double>(res_d_avx , 2 , "VFNMSUBPD");
    std::cout << std::endl;




    return 0;
}
