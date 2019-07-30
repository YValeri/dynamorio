#include "interflop_operations.hpp"

static unsigned int get_size_flag(instr_t* instr)
{
    int n = instr_num_srcs(instr);
    uint max_size = 0;
    for(int i = 0; i < n; i++)
    {
        uint curr_size = opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, i))); 
        if(curr_size > max_size)
        {
            max_size = curr_size;
        }
    }
    switch(max_size)
    {
#if defined(X86)
        case 16:
            return IFP_OP_128;
        case 32:
            return IFP_OP_256;
        case 64:
            return IFP_OP_512;
        default:
            return 0;
#elif defined(AARCH64)
        case 4:
            return IFP_OP_SCALAR | IFP_OP_FLOAT;
        case 8:
            return IFP_OP_SCALAR | IFP_OP_DOUBLE;
        case 16:
            return IFP_OP_PACKED | IFP_OP_128;
        default:
            return 0;
#endif
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
            return (OPERATION_CATEGORY)(IFP_PADDD | get_size_flag(instr));

        case OP_vaddps: //AVX packed float add
            return (OPERATION_CATEGORY)(IFP_PADDS | get_size_flag(instr));

        case OP_vsubps: //AVX packed float sub
            return (OPERATION_CATEGORY)(IFP_PSUBS | get_size_flag(instr));

        case OP_vsubpd: //AVX packet double sub
            return (OPERATION_CATEGORY)(IFP_PSUBD | get_size_flag(instr));

        case OP_vmulps: //AVX packed float mul
            return (OPERATION_CATEGORY)(IFP_PMULS | get_size_flag(instr));

        case OP_vmulpd: //AVX packet double mul
            return (OPERATION_CATEGORY)(IFP_PMULD | get_size_flag(instr));

        case OP_vdivps: //AVX packed float div
            return (OPERATION_CATEGORY)(IFP_PDIVS | get_size_flag(instr));

        case OP_vdivpd: //AVX packet double div
            return (OPERATION_CATEGORY)(IFP_PDIVD | get_size_flag(instr));


        // ###### FMA SCALAR ######

        case OP_vfmadd132ss:
            return (OPERATION_CATEGORY)(IFP_A132SS | get_size_flag(instr));

        case OP_vfmadd132sd:
            return (OPERATION_CATEGORY)(IFP_A132SD | get_size_flag(instr));

        case OP_vfmadd213ss:
            return (OPERATION_CATEGORY)(IFP_A213SS | get_size_flag(instr));

        case OP_vfmadd213sd:
            return (OPERATION_CATEGORY)(IFP_A213SD | get_size_flag(instr));

        case OP_vfmadd231ss:
            return (OPERATION_CATEGORY)(IFP_A231SS | get_size_flag(instr));

        case OP_vfmadd231sd:
            return (OPERATION_CATEGORY)(IFP_A231SD | get_size_flag(instr));

        // ###### FMS SCALAR ######

        case OP_vfmsub132ss:
            return (OPERATION_CATEGORY)(IFP_S132SS | get_size_flag(instr));

        case OP_vfmsub132sd:
            return (OPERATION_CATEGORY)(IFP_S132SD | get_size_flag(instr));

        case OP_vfmsub213ss:
            return (OPERATION_CATEGORY)(IFP_S213SS | get_size_flag(instr));

        case OP_vfmsub213sd:
            return (OPERATION_CATEGORY)(IFP_S213SD | get_size_flag(instr));

        case OP_vfmsub231ss:
            return (OPERATION_CATEGORY)(IFP_S231SS | get_size_flag(instr));

        case OP_vfmsub231sd:
            return (OPERATION_CATEGORY)(IFP_S231SD | get_size_flag(instr));

        // ###### FMA PACKED ######

        case OP_vfmadd132ps:
            return (OPERATION_CATEGORY)(IFP_A132PS | get_size_flag(instr));

        case OP_vfmadd132pd:
            return (OPERATION_CATEGORY)(IFP_A132PD | get_size_flag(instr));

        case OP_vfmadd213ps:
            return (OPERATION_CATEGORY)(IFP_A213PS | get_size_flag(instr));

        case OP_vfmadd213pd:
            return (OPERATION_CATEGORY)(IFP_A213PD | get_size_flag(instr));

        case OP_vfmadd231ps:
            return (OPERATION_CATEGORY)(IFP_A231PS | get_size_flag(instr));

        case OP_vfmadd231pd:
            return (OPERATION_CATEGORY)(IFP_A231PD | get_size_flag(instr));

        // ###### FMS PACKED ######

        case OP_vfmsub132ps:
            return (OPERATION_CATEGORY)(IFP_S132PS | get_size_flag(instr));

        case OP_vfmsub132pd:
            return (OPERATION_CATEGORY)(IFP_S132PD | get_size_flag(instr));

        case OP_vfmsub213ps:
            return (OPERATION_CATEGORY)(IFP_S213PS | get_size_flag(instr));

        case OP_vfmsub213pd:
            return (OPERATION_CATEGORY)(IFP_S213PD | get_size_flag(instr));

        case OP_vfmsub231ps:
            return (OPERATION_CATEGORY)(IFP_S231PS | get_size_flag(instr));

        case OP_vfmsub231pd:
            return (OPERATION_CATEGORY)(IFP_S231PD | get_size_flag(instr));

        // ###### Negated FMA SCALAR ######

        case OP_vfnmadd132ss:
            return (OPERATION_CATEGORY)(IFP_NA132SS | get_size_flag(instr));

        case OP_vfnmadd132sd:
            return (OPERATION_CATEGORY)(IFP_NA132SD | get_size_flag(instr));

        case OP_vfnmadd213ss:
            return (OPERATION_CATEGORY)(IFP_NA213SS | get_size_flag(instr));

        case OP_vfnmadd213sd:
            return (OPERATION_CATEGORY)(IFP_NA213SD | get_size_flag(instr));

        case OP_vfnmadd231ss:
            return (OPERATION_CATEGORY)(IFP_NA231SS | get_size_flag(instr));

        case OP_vfnmadd231sd:
            return (OPERATION_CATEGORY)(IFP_NA231SD | get_size_flag(instr));

        // ###### Negated FMS SCALAR ######

        case OP_vfnmsub132ss:
            return (OPERATION_CATEGORY)(IFP_NS132SS | get_size_flag(instr));

        case OP_vfnmsub132sd:
            return (OPERATION_CATEGORY)(IFP_NS132SD | get_size_flag(instr));

        case OP_vfnmsub213ss:
            return (OPERATION_CATEGORY)(IFP_NS213SS | get_size_flag(instr));

        case OP_vfnmsub213sd:
            return (OPERATION_CATEGORY)(IFP_NS213SD | get_size_flag(instr));

        case OP_vfnmsub231ss:
            return (OPERATION_CATEGORY)(IFP_NS231SS | get_size_flag(instr));

        case OP_vfnmsub231sd:
            return (OPERATION_CATEGORY)(IFP_NS231SD | get_size_flag(instr));

        // ###### Negated FMA PACKED ######

        case OP_vfnmadd132ps:
            return (OPERATION_CATEGORY)(IFP_NA132PS | get_size_flag(instr));

        case OP_vfnmadd132pd:
            return (OPERATION_CATEGORY)(IFP_NA132PD | get_size_flag(instr));

        case OP_vfnmadd213ps:
            return (OPERATION_CATEGORY)(IFP_NA213PS | get_size_flag(instr));

        case OP_vfnmadd213pd:
            return (OPERATION_CATEGORY)(IFP_NA213PD | get_size_flag(instr));

        case OP_vfnmadd231ps:
            return (OPERATION_CATEGORY)(IFP_NA231PS | get_size_flag(instr));

        case OP_vfnmadd231pd:
            return (OPERATION_CATEGORY)(IFP_NA231PD | get_size_flag(instr));

        // ###### Negated FMS PACKED ######

        case OP_vfnmsub132ps:
            return (OPERATION_CATEGORY)(IFP_NS132PS | get_size_flag(instr));

        case OP_vfnmsub132pd:
            return (OPERATION_CATEGORY)(IFP_NS132PD | get_size_flag(instr));

        case OP_vfnmsub213ps:
            return (OPERATION_CATEGORY)(IFP_NS213PS | get_size_flag(instr));

        case OP_vfnmsub213pd:
            return (OPERATION_CATEGORY)(IFP_NS213PD | get_size_flag(instr));

        case OP_vfnmsub231ps:
            return (OPERATION_CATEGORY)(IFP_NS231PS | get_size_flag(instr));

        case OP_vfnmsub231pd:
            return (OPERATION_CATEGORY)(IFP_NS231PD | get_size_flag(instr));

        // ###### UNSUPPORTED ######

        case OP_vaddsd: //AVX-512  scalar double add with special rounding
        case OP_vaddss: //AVX-512  scalar float add with special rounding
            return IFP_UNSUPPORTED;

#elif defined(AARCH64)
        case OP_fadd:
	    return (OPERATION_CATEGORY)(IFP_OP_ADD | get_size_flag(instr));

        case OP_fsub:
            return (OPERATION_CATEGORY)(IFP_OP_SUB | get_size_flag(instr));

        case OP_fmul:
            return (OPERATION_CATEGORY)(IFP_OP_MUL | get_size_flag(instr));

        case OP_fdiv:
            return (OPERATION_CATEGORY)(IFP_OP_DIV | get_size_flag(instr));

        case OP_fmadd:
            return (OPERATION_CATEGORY)(IFP_OP_FMA | get_size_flag(instr));

        case OP_fmsub:
            return (OPERATION_CATEGORY)(IFP_OP_FMS | get_size_flag(instr));
#endif

        default:
            return IFP_OTHER;
    }
    
}
