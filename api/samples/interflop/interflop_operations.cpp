#include "interflop_operations.hpp"

static unsigned int getPackedSizeFlag(instr_t* instr)
{
    int n = instr_num_srcs(instr);
    uint maxSize=0;
    for(int i=0; i<n; i++)
    {
        uint curr_size = opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, i))); 
        if(curr_size > maxSize)
        {
            maxSize = curr_size;
        }
    }
    switch(maxSize)
    {
        case 16:
        return IFP_OP_128;
        case 32:
        return IFP_OP_256;
        case 64:
        return IFP_OP_512;
        default:
        return 0;
    }
}

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
        return (OPERATION_CATEGORY)(IFP_PADDD | getPackedSizeFlag(instr));

        case OP_vaddps: //AVX packed float add
        return (OPERATION_CATEGORY)(IFP_PADDS | getPackedSizeFlag(instr));

        case OP_vsubps: //AVX packed float sub
        return (OPERATION_CATEGORY)(IFP_PSUBS | getPackedSizeFlag(instr));

        case OP_vsubpd: //AVX packet double sub
        return (OPERATION_CATEGORY)(IFP_PSUBD | getPackedSizeFlag(instr));

        case OP_vmulps: //AVX packed float mul
        return (OPERATION_CATEGORY)(IFP_PMULS | getPackedSizeFlag(instr));

        case OP_vmulpd: //AVX packet double mul
        return (OPERATION_CATEGORY)(IFP_PMULD | getPackedSizeFlag(instr));

        case OP_vdivps: //AVX packed float div
        return (OPERATION_CATEGORY)(IFP_PDIVS | getPackedSizeFlag(instr));

        case OP_vdivpd: //AVX packet double div
        return (OPERATION_CATEGORY)(IFP_PDIVD | getPackedSizeFlag(instr));

        // ###### UNSUPPORTED ######

        case OP_vaddsd: //AVX-512  scalar double add with special rounding
        case OP_vaddss: //AVX-512  scalar float add with special rounding
        return IFP_UNSUPPORTED;

#elif defined(AARCH64)
        case OP_fadd:
        return IFP_ADDD;
#endif

        default:
        return IFP_OTHER;
    }
    
}