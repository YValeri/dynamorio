

#include "test_instructions.h"

int main(int argc, char const *argv[])
{
    /* Default seed get a static test case for comparison when running with dynamoRIO */
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

    /* *************************** SCALAR *************************** */
    std::cout << "\n\t***** SCALAR INSTRUCTIONS SSE *****\n\n";
    
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
    


    std::cout << "\n\n\t***** VECTORISED INSTRUCTIONS SSE *****\n\n";
    
    /* ADD */
    v_res_f_sse = _mm_add_ps(v_src0_f_sse , v_src1_f_sse);   _mm_store_ps(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_add_pd(v_src0_d_sse , v_src1_d_sse);   _mm_store_pd(res_d_sse , v_res_d_sse);
    print<float>(res_f_sse , 4 , "ADDPS");
    print<double>(res_d_sse , 2 , "ADDPD");
    std::cout << std::endl;

    
    /* SUB */
    v_res_f_sse = _mm_sub_ps(v_src0_f_sse , v_src1_f_sse); _mm_store_ps(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_sub_pd(v_src0_d_sse , v_src1_d_sse); _mm_store_pd(res_d_sse , v_res_d_sse);
    print<float>(res_f_sse , 4 , "SUBPS");
    print<double>(res_d_sse , 2 , "SUBPD");
    std::cout << std::endl;
    

    /* MUL */
    v_res_f_sse = _mm_mul_ps(v_src0_f_sse , v_src1_f_sse); _mm_store_ps(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_mul_pd(v_src0_d_sse , v_src1_d_sse); _mm_store_pd(res_d_sse , v_res_d_sse);
    print<float>(res_f_sse , 4 , "MULPS");
    print<double>(res_d_sse , 2 , "MULPD");
    std::cout << std::endl;
    
    /* DIV */
    v_res_f_sse = _mm_div_ps(v_src0_f_sse , v_src1_f_sse); _mm_store_ps(res_f_sse , v_res_f_sse);
    v_res_d_sse = _mm_div_pd(v_src0_d_sse , v_src1_d_sse); _mm_store_pd(res_d_sse , v_res_d_sse);
    print<float>(res_f_sse , 4 , "DIVPS");
    print<double>(res_d_sse , 2 , "DIVPD");
    std::cout << std::endl;


    return 0;
}
