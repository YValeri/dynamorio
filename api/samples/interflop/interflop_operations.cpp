#include "interflop_operations.hpp"

enum OPERATION_CATEGORY ifp_get_operation_category(instr_t* instr)
{
    //TODO Need to complete
    switch(instr_get_opcode(instr))
    {
#ifdef X86
        // ####### SCALAR #######
        case OP_addss: //SSE scalar float add
        return IFP_ADDS;

        case OP_addsd: //SSE scalar double add
        return IFP_ADDD;

        case OP_subsd: //SSE scalar double sub
        return IFP_SUBD;

        case OP_subss: //SSE scalar float sub
        return IFP_SUBS;

        case OP_mulsd: //SSE scalar double mul
        return IFP_MULD;

        case OP_mulss: //SSE scalar float mul
        return IFP_MULS;

        case OP_divsd: //SSE scalar double div
        return IFP_DIVD;

        case OP_divss: //SSE scalar float div
        return IFP_DIVS;

        // ###### SIMD ######
        // ###### SSE ######

        case OP_addpd: //SSE packed double add
        return IFP_PADDD_128;

        case OP_addps: //SSE packed float add
        return IFP_PADDS_128;

        // ###### AVX ######

        case OP_vaddpd: //AVX packed double add
        return IFP_PADDD_256;

        case OP_vaddps: //AVX packed float add
        return IFP_PADDS_256;

        // ###### UNSUPPORTED ######

        case OP_vaddsd: //AVX-512  scalar double add with special rounding
        case OP_vaddss: //AVX-512  scalar float add with special rounding
        return IFP_UNSUPPORTED;

#elif defined(AARCH64)

#endif

        default:
        return IFP_OTHER;
    }
    
}