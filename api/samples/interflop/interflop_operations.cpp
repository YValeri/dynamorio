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

        case OP_addps: //SSE packed float add
        return IFP_PADDS_128;

        case OP_addpd: //SSE packed double add
        return IFP_PADDD_128;

        case OP_subps: //SSE packed float sub
        return IFP_PSUBS_128;

        case OP_subpd: //SSE packet double sub
        return IFP_PSUBD_128;

        case OP_mulps: //SSE packed float mul
        return IFP_PMULS_128;

        case OP_mulpd: //SSE packet double mul
        return IFP_PMULD_128;

        case OP_divps: //SSE packed float div
        return IFP_PDIVS_128;

        case OP_divpd: //SSE packet double div
        return IFP_PDIVD_128;

        // ###### AVX ######

        case OP_vaddpd: //AVX packed double add
        return IFP_PADDD_256;

        case OP_vaddps: //AVX packed float add
        return IFP_PADDS_256;

        case OP_vsubps: //AVX packed float sub
        return IFP_PSUBS_256;

        case OP_vsubpd: //AVX packet double sub
        return IFP_PSUBD_256;

        case OP_vmulps: //AVX packed float mul
        return IFP_PMULS_256;

        case OP_vmulpd: //AVX packet double mul
        return IFP_PMULD_256;

        case OP_vdivps: //AVX packed float div
        return IFP_PDIVS_256;

        case OP_vdivpd: //AVX packet double div
        return IFP_PDIVD_256;

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