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

        // ###### FMA SCALAR ######

        case OP_vfmadd132ss:
        return IFP_A132SS;

        case OP_vfmadd132sd:
        return IFP_A132SD;

        case OP_vfmadd213ss:
        return IFP_A213SS;

        case OP_vfmadd213sd:
        return IFP_A213SD;

        case OP_vfmadd231ss:
        return IFP_A231SS;

        case OP_vfmadd231sd:
        return IFP_A231SD;

        // ###### FMS SCALAR ######

        case OP_vfmsub132ss:
        return IFP_S132SS;

        case OP_vfmsub132sd:
        return IFP_S132SD;

        case OP_vfmsub213ss:
        return IFP_S213SS;

        case OP_vfmsub213sd:
        return IFP_S213SD;

        case OP_vfmsub231ss:
        return IFP_S231SS;

        case OP_vfmsub231sd:
        return IFP_S231SD;

        // ###### FMA PACKED ######

        case OP_vfmadd132ps:
        return IFP_A132PS;

        case OP_vfmadd132pd:
        return IFP_A132PD;

        case OP_vfmadd213ps:
        return IFP_A213PS;

        case OP_vfmadd213pd:
        return IFP_A213PD;

        case OP_vfmadd231ps:
        return IFP_A231PS;

        case OP_vfmadd231pd:
        return IFP_A231PD;

        // ###### FMS PACKED ######

        case OP_vfmsub132ps:
        return IFP_S132PS;

        case OP_vfmsub132pd:
        return IFP_S132PD;

        case OP_vfmsub213ps:
        return IFP_S213PS;

        case OP_vfmsub213pd:
        return IFP_S213PD;

        case OP_vfmsub231ps:
        return IFP_S231PS;

        case OP_vfmsub231pd:
        return IFP_S231PD;



        // ###### Negated FMA SCALAR ######

        case OP_vfnmadd132ss:
        return IFP_NA132SS;

        case OP_vfnmadd132sd:
        return IFP_NA132SD;

        case OP_vfnmadd213ss:
        return IFP_NA213SS;

        case OP_vfnmadd213sd:
        return IFP_NA213SD;

        case OP_vfnmadd231ss:
        return IFP_NA231SS;

        case OP_vfnmadd231sd:
        return IFP_NA231SD;

        // ###### Negated FMS SCALAR ######

        case OP_vfnmsub132ss:
        return IFP_NS132SS;

        case OP_vfnmsub132sd:
        return IFP_NS132SD;

        case OP_vfnmsub213ss:
        return IFP_NS213SS;

        case OP_vfnmsub213sd:
        return IFP_NS213SD;

        case OP_vfnmsub231ss:
        return IFP_NS231SS;

        case OP_vfnmsub231sd:
        return IFP_NS231SD;

        // ###### Negated FMA PACKED ######

        case OP_vfnmadd132ps:
        return IFP_NA132PS;

        case OP_vfnmadd132pd:
        return IFP_NA132PD;

        case OP_vfnmadd213ps:
        return IFP_NA213PS;

        case OP_vfnmadd213pd:
        return IFP_NA213PD;

        case OP_vfnmadd231ps:
        return IFP_NA231PS;

        case OP_vfnmadd231pd:
        return IFP_NA231PD;

        // ###### Negated FMS PACKED ######

        case OP_vfnmsub132ps:
        return IFP_NS132PS;

        case OP_vfnmsub132pd:
        return IFP_NS132PD;

        case OP_vfnmsub213ps:
        return IFP_NS213PS;

        case OP_vfnmsub213pd:
        return IFP_NS213PD;

        case OP_vfnmsub231ps:
        return IFP_NS231PS;

        case OP_vfnmsub231pd:
        return IFP_NS231PD;

        // ###### UNSUPPORTED ######

        case OP_vaddsd: //AVX-512  scalar double add with special rounding
        case OP_vaddss: //AVX-512  scalar float add with special rounding
        return IFP_UNSUPPORTED;

#elif defined(AARCH64)

        case OP_fadd: //SSE scalar float add
        return IFP_ADDS;
#endif

        default:
        return IFP_OTHER;
    }
    
}