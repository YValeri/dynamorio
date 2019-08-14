/**
 * \file
 * \brief  
 * 
 * \author Brasseur Dylan, Teaudors Mickaël, Valeri Yoann
 * \date 2019
 * \copyright Interflop 
 */

#include <cstdint>
#include "padloc_client.h"
#include "analyse.hpp"

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#if defined(X86)
#define INSTR_IS_ANY_SSE(instr) (instr_is_sse(instr) || instr_is_sse2(instr) || instr_is_sse3(instr) || instr_is_sse41(instr) || instr_is_sse42(instr) || instr_is_sse4A(instr))
#elif defined(AARCH64)
static reg_id_t Q_REG[] = {
    DR_REG_Q0, DR_REG_Q1, DR_REG_Q2, DR_REG_Q3, DR_REG_Q4,
    DR_REG_Q5, DR_REG_Q6, DR_REG_Q7, DR_REG_Q8, DR_REG_Q9,
    DR_REG_Q10, DR_REG_Q11, DR_REG_Q12, DR_REG_Q13, DR_REG_Q14,
    DR_REG_Q15, DR_REG_Q16, DR_REG_Q17, DR_REG_Q18, DR_REG_Q19,
    DR_REG_Q20, DR_REG_Q21, DR_REG_Q22, DR_REG_Q23, DR_REG_Q24,
    DR_REG_Q25, DR_REG_Q26, DR_REG_Q27, DR_REG_Q28, DR_REG_Q29,
    DR_REG_Q30, DR_REG_Q31
};
static reg_id_t Q_REG_REVERSE[] = {
    DR_REG_Q31, DR_REG_Q30, DR_REG_Q29, DR_REG_Q28, DR_REG_Q27,
    DR_REG_Q26, DR_REG_Q25, DR_REG_Q24, DR_REG_Q23, DR_REG_Q22,
    DR_REG_Q21, DR_REG_Q20, DR_REG_Q19, DR_REG_Q18, DR_REG_Q17,
    DR_REG_Q16, DR_REG_Q15, DR_REG_Q14, DR_REG_Q13, DR_REG_Q12,
    DR_REG_Q11, DR_REG_Q10, DR_REG_Q9, DR_REG_Q8, DR_REG_Q7,
    DR_REG_Q6, DR_REG_Q5, DR_REG_Q4, DR_REG_Q3, DR_REG_Q2,
    DR_REG_Q1, DR_REG_Q0
};
#endif

#define R(name) DR_REG_##name
#if defined(X86) && defined(X64)
static const reg_id_t GPR_ORDER[] = {R(NULL), R(XAX), R(XCX), R(XDX), R(XBX), R(XSP), R(XBP), R(XSI), R(XDI), R(R8), R(R9), R(R10), R(R11), R(R12), R(R13), R(R14), R(R15)};
#define NUM_GPR_SLOTS 17
#elif defined(AArch64)
//TODO Add GPR for Aarch64
#endif
#undef R

static int tls_gpr, tls_float, tls_result;

/**
 * \brief Returns the index of the gpr tls
 */
int get_index_tls_gpr(){
    return tls_gpr;
}

/**
 * \brief Returns the index of the floating point registers tls
 */
int get_index_tls_float(){
    return tls_float;
}

/**
 * \brief Returns the index of the result tls
 */
int get_index_tls_result(){
    return tls_result;
}

/**
 * \brief Sets the index of the gpr tls
 */
void set_index_tls_gpr(int new_tls_value){
    tls_gpr = new_tls_value;
}

/**
 * \brief Sets the index of the floating point registers tls
 */
void set_index_tls_float(int new_tls_value){
    tls_float = new_tls_value;
}

/**
 * \brief Sets the index of the result tls
 */
void set_index_tls_result(int new_tls_value){
    tls_result = new_tls_value;
}

/**
 * \brief Returns the offset, in bytes, of the \param gpr stored in the tls
 */
inline int offset_of_gpr(reg_id_t gpr){
    //Assuming the gpr parameter is a valid gpr
    return (((int)gpr - DR_REG_START_GPR) + 1) << 3;
}

/**
 * \brief Returns the offset, in bytes, of the simd register in the tls
 * 
 * \param simd 
 * \return int 
 */
inline int offset_of_simd(reg_id_t simd){
    //Assuming the simd parameter is a valid simd register
    static const int OFFSET = AVX_512_SUPPORTED ? 6 : AVX_SUPPORTED ? 5 : 4 /*128 bits = 16 bytes = 2^4 */;
    const reg_id_t START = reg_is_strictly_zmm(simd) ? DR_REG_START_ZMM : reg_is_strictly_ymm(simd) ? DR_REG_START_YMM
                                                                                                    : DR_REG_START_XMM;
    return ((uint)simd - START) << OFFSET;
}

/**
 * \brief Definition of intermediary functions between front-end and back-end for 2 operands instructions
 * \tparam  float or double depending on the precision of the instruction 
 * \tparam  Function corresponding to the overloaded operation (add, sub, fmadd ...) : implementation in Backend.hxx
 * \tparam  Marks the difference SSE and AVX instructions. Possible values are PLC_OP_SSE and PLC_OP_AVX
 * \tparam PLC_OP_SCALAR Define the length of the elements of the operation. Possible values are PLC_OP_SCALAR, PLC_OP_128, PLC_OP_256 and PLC_OP_512
 */
template<typename FTYPE, FTYPE (*Backend_function)(FTYPE, FTYPE), int INSTR_CATEGORY, int SIMD_TYPE = PLC_OP_SCALAR>
struct padloc_backend{

    static void apply(FTYPE *vect_a, FTYPE *vect_b){

        static const int operation_size =   (SIMD_TYPE == PLC_OP_128) ? 16 : (SIMD_TYPE == PLC_OP_256) ? 32 :
                                            (SIMD_TYPE == PLC_OP_512) ? 64 : sizeof(FTYPE);
        static const int nb_elem = operation_size / sizeof(FTYPE);

        static const int max_operation_size = (INSTR_CATEGORY == PLC_OP_SSE) ? 16
                                            : (INSTR_CATEGORY == PLC_OP_AVX) ? 32
                                            : sizeof(FTYPE);
        static const int max_nb_elem = max_operation_size / sizeof(FTYPE);

        FTYPE res;

        FTYPE *tls = *(FTYPE **)GET_TLS(dr_get_current_drcontext(), tls_result);

        for(int i = 0; i < nb_elem; i++){
#if defined(X86)
            res = Backend_function(vect_a[i], vect_b[i]);
#elif defined(AARCH64)
            res = Backend_function(vect_b[i], vect_a[i]);
#endif
            *(tls + i) = res;
        }


        /* If this is an AVX instruction, set the high part with 0 */
        if(INSTR_CATEGORY == PLC_OP_AVX){
            for(int i = MAX(nb_elem, 16 / sizeof(FTYPE)); i < max_nb_elem; i++){
                *(tls + i) = 0;
            }
        }
    }
};

/**
 * \brief Identical to padloc_backend for three sources instructions 
 */
template<typename FTYPE, FTYPE (*Backend_function)(FTYPE, FTYPE, FTYPE), int INSTR_CATEGORY, int SIMD_TYPE = PLC_OP_SCALAR>
struct padloc_backend_fused{

    static void apply(FTYPE *vect_a, FTYPE *vect_b, FTYPE *vect_c){

        static const int vect_size =  (SIMD_TYPE == PLC_OP_128) ? 16 : (SIMD_TYPE == PLC_OP_256) ? 32
                                    : (SIMD_TYPE == PLC_OP_512) ? 64 : sizeof(FTYPE);
        static const int nb_elem = vect_size / sizeof(FTYPE);

        static const int max_operation_size = (INSTR_CATEGORY == PLC_OP_SSE) ? 16
                                            : (INSTR_CATEGORY == PLC_OP_AVX) ? 32
                                            : sizeof(FTYPE);
        static const int max_nb_elem = max_operation_size / sizeof(FTYPE);

        FTYPE res;
        FTYPE *tls = *(FTYPE **)(GET_TLS(dr_get_current_drcontext(), tls_result));

        for(int i = 0; i < nb_elem; i++){
#if defined(X86)
            res = Backend_function(vect_a[i], vect_b[i], vect_c[i]);
#elif defined(AARCH64)
            res = Backend_function(vect_b[i], vect_a[i], vect_c[i]);
#endif
            *(tls + i) = res;
        }

        /* If this is an AVX instruction, set the high part with 0 */
        if(INSTR_CATEGORY == PLC_OP_AVX){
            for(int i = MAX(nb_elem, 16 / sizeof(FTYPE)); i < max_nb_elem; i++){
                *(tls + i) = 0;
            }
        }
    }
};

/**
 * \brief Inserts \param newinstr in \param ilist prior to \param instr and set it as an application instruction
 */
void translate_insert(instr_t *newinstr, instrlist_t *ilist, instr_t *instr){
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist, instr, newinstr);
}

/**
 * \brief Subset of insert_call for two sources instructions
 */
template<typename FTYPE, FTYPE (*Backend_function)(FTYPE, FTYPE)>
void insert_corresponding_vect_call(void *drcontext, instrlist_t *bb, instr_t *instr, OPERATION_CATEGORY oc){
    switch(oc & PLC_SIMD_TYPE_MASK){
        case PLC_OP_128:
            if(oc & PLC_OP_SSE){
                dr_insert_call(drcontext, bb, instr,
                               (void *)padloc_backend<FTYPE, Backend_function, PLC_OP_SSE, PLC_OP_128>::apply, 0);
            }else{
                dr_insert_call(drcontext, bb, instr,
                               (void *)padloc_backend<FTYPE, Backend_function, PLC_OP_AVX, PLC_OP_128>::apply, 0);
            }
            break;
        case PLC_OP_256:
            dr_insert_call(drcontext, bb, instr, (void *)padloc_backend<FTYPE, Backend_function, PLC_AVX, PLC_OP_256>::apply,
                           0);
            break;
        case PLC_OP_512:
            dr_insert_call(drcontext, bb, instr, (void *)padloc_backend<FTYPE, Backend_function, PLC_AVX, PLC_OP_512>::apply,
                           0);
            break;
        default: /*SCALAR */
            if(oc & PLC_OP_SSE){
                dr_insert_call(drcontext, bb, instr, (void *)padloc_backend<FTYPE, Backend_function, PLC_OP_SSE>::apply, 0);
            }else{
                dr_insert_call(drcontext, bb, instr, (void *)padloc_backend<FTYPE, Backend_function, PLC_OP_AVX>::apply, 0);
            }
    }
}

/**
 * \brief Subset of insert_call for three sources instructions
 */
template<typename FTYPE, FTYPE (*Backend_function)(FTYPE, FTYPE, FTYPE)>
void insert_corresponding_vect_call_fused(void *drcontext, instrlist_t *bb, instr_t *instr, OPERATION_CATEGORY oc){
    switch(oc & PLC_SIMD_TYPE_MASK){
        case PLC_OP_128:
            if(oc & PLC_OP_SSE){
                dr_insert_call(drcontext, bb, instr,
                               (void *)padloc_backend_fused<FTYPE, Backend_function, PLC_OP_SSE, PLC_OP_128>::apply, 0);
            }else{
                dr_insert_call(drcontext, bb, instr,
                               (void *)padloc_backend_fused<FTYPE, Backend_function, PLC_OP_AVX, PLC_OP_128>::apply, 0);
            }
            break;
        case PLC_OP_256:
            dr_insert_call(drcontext, bb, instr,
                           (void *)padloc_backend_fused<FTYPE, Backend_function, PLC_OP_AVX, PLC_OP_256>::apply, 0);
            break;
        case PLC_OP_512:
            dr_insert_call(drcontext, bb, instr,
                           (void *)padloc_backend_fused<FTYPE, Backend_function, PLC_OP_AVX, PLC_OP_512>::apply, 0);
            break;

        default: /*SCALAR */
            if(oc & PLC_OP_SSE){
                dr_insert_call(drcontext, bb, instr, (void *)padloc_backend_fused<FTYPE, Backend_function, PLC_OP_SSE>::apply,
                               0);
            }else{
                dr_insert_call(drcontext, bb, instr, (void *)padloc_backend_fused<FTYPE, Backend_function, PLC_OP_AVX>::apply,
                               0);
            }
    }
}


/**
 * \brief Insert the corresponding call into the application depending on the properties of the instruction overloaded.  
 * 
 * \param drcontext DynamoRIO's context
 * \param bb Current basic block
 * \param instr Instrumented instruction
 * \param oc Operation category of the instruction
 * \param is_double True if the baseline precision is double
 */
void insert_call(void *drcontext, instrlist_t *bb, instr_t *instr, OPERATION_CATEGORY oc, bool is_double){
    if(oc & PLC_OP_FUSED){
        if(oc & PLC_OP_FMA){
            if(!(oc & PLC_OP_NEG)){
                if(is_double){
                    insert_corresponding_vect_call_fused<double, Interflop::Op<double>::fmadd>(drcontext, bb, instr, oc);
                }else{
                    insert_corresponding_vect_call_fused<float, Interflop::Op<float>::fmadd>(drcontext, bb, instr, oc);
                }
            }else{
                if(is_double){
                    insert_corresponding_vect_call_fused<double, Interflop::Op<double>::nfmadd>(drcontext, bb, instr, oc);
                }else{
                    insert_corresponding_vect_call_fused<float, Interflop::Op<float>::nfmadd>(drcontext, bb, instr, oc);
                }
            }
        }else if(oc & PLC_OP_FMS){
            if(!(oc & PLC_OP_NEG)){
                if(is_double){
                    insert_corresponding_vect_call_fused<double, Interflop::Op<double>::fmsub>(drcontext, bb, instr, oc);
                }else{
                    insert_corresponding_vect_call_fused<float, Interflop::Op<float>::fmsub>(drcontext, bb, instr, oc);
                }
            }else{
                if(is_double){
                    insert_corresponding_vect_call_fused<double, Interflop::Op<double>::nfmsub>(drcontext, bb, instr, oc);
                }else{
                    insert_corresponding_vect_call_fused<float, Interflop::Op<float>::nfmsub>(drcontext, bb, instr, oc);
                }
            }
        }
    }else{
        switch(oc & PLC_OP_TYPE_MASK){
            case PLC_OP_ADD:
                if(is_double){
                    insert_corresponding_vect_call<double, Interflop::Op<double>::add>(drcontext, bb, instr, oc);
                }else{
                    insert_corresponding_vect_call<float, Interflop::Op<float>::add>(drcontext, bb, instr, oc);
                }
                break;
            case PLC_OP_SUB:
                if(is_double){
                    insert_corresponding_vect_call<double, Interflop::Op<double>::sub>(drcontext, bb, instr, oc);
                }else{
                    insert_corresponding_vect_call<float, Interflop::Op<float>::sub>(drcontext, bb, instr, oc);
                }
                break;
            case PLC_OP_MUL:
                if(is_double){
                    insert_corresponding_vect_call<double, Interflop::Op<double>::mul>(drcontext, bb, instr, oc);
                }else{
                    insert_corresponding_vect_call<float, Interflop::Op<float>::mul>(drcontext, bb, instr, oc);
                }
                break;
            case PLC_OP_DIV:
                if(is_double){
                    insert_corresponding_vect_call<double, Interflop::Op<double>::div>(drcontext, bb, instr, oc);
                }else{
                    insert_corresponding_vect_call<float, Interflop::Op<float>::div>(drcontext, bb, instr, oc);
                }
                break;
            default:
                PRINT_ERROR_MESSAGE("ERROR OPERATION NOT FOUND !");
        }
    }
}

//Macros to reduce code size
#define MINSERT(bb, where, instr) instrlist_meta_preinsert(bb, where, instr)
#define AINSERT(bb, where, instr) translate_insert(instr, bb, where)

/**
 * \brief Inserts prior to \param where meta-instructions to save the arithmetic flags and the gpr registers
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_save_gpr_and_flags(void *drcontext, instrlist_t *bb, instr_t *where){
#if defined(X86) && defined(X64)
    //save rcx to spill slot
    dr_save_reg(drcontext, bb, where, DR_REG_RCX, SPILL_SLOT_SCRATCH_REG); 
    //read tls
    INSERT_READ_TLS(drcontext, get_index_tls_gpr(), bb, where, DR_REG_RCX); 
    //store rax in second position
    MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_RCX, 8, OPSZ_8),OP_REG(DR_REG_XAX))); 
    //store arith flags to rax
    MINSERT(bb, where, INSTR_CREATE_lahf(drcontext)); 
    //store arith flags in first position
    MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_RCX, 0, OPSZ_8),OP_REG(DR_REG_XAX))); 
    //restore rcx into rax
    dr_restore_reg(drcontext, bb, where, DR_REG_XAX, SPILL_SLOT_SCRATCH_REG); 
    //store rcx in third position
    MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_RCX, 16, OPSZ_8),OP_REG(DR_REG_XAX))); 
    //save all the other GPR
    for(size_t i=3; i<NUM_GPR_SLOTS; i++)
    {
        MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_RCX, offset_of_gpr(GPR_ORDER[i]), OPSZ_8), OP_REG(GPR_ORDER[i])));
    }

#else //AArch64
    DR_ASSERT_MSG(false, "insert_save_gpr_and_flags not implemented for this architecture");
#endif
}

/**
 * \brief Inserts prior to \param where meta-instructions to restore the arithmetic flags and the gpr registers
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_restore_gpr_and_flags(void *drcontext, instrlist_t *bb, instr_t *where){
#if defined(X86) && defined(X64)
    //read tls into rcx
    INSERT_READ_TLS(drcontext, get_index_tls_gpr(), bb, where, DR_REG_RCX);
    //load saved arith flags to rax
    MINSERT(bb, where, XINST_CREATE_load(drcontext, OP_REG(DR_REG_RAX), OP_BASE_DISP(DR_REG_RCX, 0, OPSZ_8)));
    //load arith flags
    MINSERT(bb, where, INSTR_CREATE_sahf(drcontext));
    //load saved rax into rax
    MINSERT(bb, where, XINST_CREATE_load(drcontext, OP_REG(DR_REG_RAX), OP_BASE_DISP(DR_REG_RCX, 8, OPSZ_8)));
    //load back all GPR in reverse, overwrite RCX in the end
    for(size_t i=NUM_GPR_SLOTS-1; i>=2 /*RCX*/; --i)
    {
        MINSERT(bb, where, XINST_CREATE_load(drcontext, OP_REG(GPR_ORDER[i]), OP_BASE_DISP(DR_REG_RCX, offset_of_gpr(GPR_ORDER[i]), OPSZ_8)));
    }

#else //AArch64
    DR_ASSERT_MSG(false, "insert_restore_gpr_and_flags not implemented for this architecture");
#endif
}

/**
 * \brief Prepares the address in the buffer of the tls register to point to the destination register in memory
 * \details Assumes the gpr have been saved beforehand !
 * \param drcontext DynamoRIO's context
 * \param bb Current basic block
 * \param where instruction prior to whom we insert the meta-instructions 
 * \param destination SIMD registers that should have received the result
 */
void insert_set_destination_tls(void *drcontext, instrlist_t *bb, instr_t *where, reg_id_t destination){
#if defined(X86) && defined(X64)
    //Result tls adress in OP_A register
    INSERT_READ_TLS(drcontext, get_index_tls_result(), bb, where, DR_REG_OP_A_ADDR);
    //Floating registers tls adress in OP_B register
    INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, DR_REG_OP_B_ADDR);
    //Loads the adress of the destination register who is in the saved array, in OP_C register
    MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(DR_REG_OP_C_ADDR), OP_BASE_DISP(DR_REG_OP_B_ADDR, offset_of_simd(destination), OPSZ_lea)));
    //Stores the adress in the buffer of the result tls
    MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_OP_A_ADDR,0, OPSZ_8), OP_REG(DR_REG_OP_C_ADDR)));
#else //AArch64
    DR_ASSERT_MSG(false, "insert_set_destination_tls not implemented for this architecture");
#endif
}

/**
 * \brief Inserts prior to \p where meta-instructions to save the floating point registers (xmm-ymm-zmm)
 * \details Assumes the gpr have been saved beforehand !
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_save_simd_registers(void *drcontext, instrlist_t *bb, instr_t *where){
#if defined(X86) && defined(X64)
    //Loads the adress of the simd registers TLS
    INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, DR_SCRATCH_REG);
    //By default, SSE
    reg_id_t start = DR_REG_START_XMM, stop = DR_REG_XMM15;
    bool is_avx=false;
    opnd_size_t size = OPSZ_16;
    if(AVX_512_SUPPORTED)
    {
        is_avx = true;
        start = DR_REG_START_ZMM;
        stop = DR_REG_STOP_ZMM;
        size=OPSZ_64;
    }else if(AVX_SUPPORTED)
    {
        is_avx=true;
        start = DR_REG_START_YMM;
        stop = DR_REG_YMM15;
        size = OPSZ_32;
    }
    //Save all the SIMD registers
    for(reg_id_t i=start; i<=stop; i++)
    {
        MINSERT(bb, where, MOVE_FLOATING_PACKED(is_avx, drcontext, OP_BASE_DISP(DR_SCRATCH_REG, offset_of_simd(i),size), OP_REG(i)));
    }
#else //AArch64
    DR_ASSERT_MSG(false, "insert_save_simd_registers not implemented for this architecture");
#endif
}

/**
 * \brief Inserts prior to \p where meta-instructions to restore the floating point registers (xmm-ymm-zmm)
 * \details Assumes the gpr have been saved beforehand !
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_restore_simd_registers(void *drcontext, instrlist_t *bb, instr_t *where){
#if defined(X86) && defined(X64)
    //Loads the adress of the simd registers TLS
    INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, DR_SCRATCH_REG);
    //By default, SSE
    reg_id_t start = DR_REG_START_XMM, stop = DR_REG_XMM15;
    bool is_avx=false;
    opnd_size_t size = OPSZ_16;
    if(AVX_512_SUPPORTED)
    {
        is_avx = true;
        start = DR_REG_START_ZMM;
        stop = DR_REG_STOP_ZMM;
        size=OPSZ_64;
    }else if(AVX_SUPPORTED)
    {
        is_avx=true;
        start = DR_REG_START_YMM;
        stop = DR_REG_YMM15;
        size = OPSZ_32;
    }
    //Restore all the SIMD registers
    for(reg_id_t i=start; i<=stop; i++)
    {
        MINSERT(bb, where, MOVE_FLOATING_PACKED(is_avx, drcontext, OP_REG(i), OP_BASE_DISP(DR_SCRATCH_REG, offset_of_simd(i),size)));
    }
#else //AArch64
    DR_ASSERT_MSG(false, "insert_save_simd_registers not implemented for this architecture");
#endif
}

/**
 * \brief Inserts prior to \p where meta-instructions to set the calling convention registers to the right adress when all the operands are registers
 * \details Assumes the GPR have been saved !
 * \param drcontext DynamoRIO's context
 * \param bb Current Basic block
 * \param where instruction prior to whom we insert the meta-instructions 
 * \param src0 source 0 of the instruction
 * \param src1 source 1 of the instruction
 * \param src2 source 2 of the instruction
 */
static void insert_set_operands_all_registers(void *drcontext, instrlist_t *bb, instr_t *where, reg_id_t src0, reg_id_t src1,
                                              reg_id_t src2, reg_id_t out_reg[]){
#if defined(X86) && defined(X64)
    if(src2 == DR_REG_NULL)
    {
        //2 registers
        INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, out_reg[1]);
        MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[0]), OP_BASE_DISP(out_reg[1], offset_of_simd(src0), OPSZ_lea)));
        MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[1]), OP_BASE_DISP(out_reg[1], offset_of_simd(src1), OPSZ_lea)));
    }else
    {
        //3 registers
        INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, out_reg[2]);
        MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[0]), OP_BASE_DISP(out_reg[2], offset_of_simd(src0), OPSZ_lea)));
        MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[1]), OP_BASE_DISP(out_reg[2], offset_of_simd(src1), OPSZ_lea)));
        MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[2]), OP_BASE_DISP(out_reg[2], offset_of_simd(src2), OPSZ_lea)));
    }
#else //AArch64
    DR_ASSERT_MSG(false, "insert_set_operands_all_registers not implemented for this architecture");
#endif
}

/**
 * \brief Inserts prior to \p where meta-instructions to set the calling convention register to the right adress when it's an adress defined as a base-disp
 * \details Assumes the GPR have been saved !
 * \param drcontext DynamoRIO's context
 * \param bb Current Basic Block
 * \param where instruction prior to whom we insert the meta-instructions 
 * \param addr Operand of the instruction which is a base disp
 * \param destination Register that will receive the address
 * \param tempindex Register to use as a temporary holder for the index, if there is one
 */
static void insert_set_operands_base_disp(void *drcontext, instrlist_t *bb, instr_t *where, opnd_t addr, reg_id_t destination,
                                          reg_id_t tempindex){
    INSERT_READ_TLS(drcontext, get_index_tls_gpr(), bb, where, destination);
    reg_id_t base = opnd_get_base(addr), index = opnd_get_index(addr);
    if(index != DR_REG_NULL){
        MINSERT(bb, where,
                XINST_CREATE_load(drcontext, OP_REG(tempindex), OP_BASE_DISP(destination, offset_of_gpr(index), OPSZ_8)));
        index = tempindex;
    }
    if(base != DR_REG_NULL){
        MINSERT(bb, where,
                XINST_CREATE_load(drcontext, OP_REG(destination), OP_BASE_DISP(destination, offset_of_gpr(base), OPSZ_8)));
        base = destination;
    }
    MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(destination),
                                        opnd_create_base_disp(base, index, opnd_get_scale(addr), opnd_get_disp(addr),
                                        OPSZ_lea)));
}

/**
 * \brief Inserts prior to \p where meta-instructions to set the calling convention register to the right adress when it's an relative of absolute adresse
 * \details Assumes the GPR have been saved !
 * \param drcontext DynamoRIO's context
 * \param bb Current Basic Block
 * \param where instruction prior to whom we insert the meta-instructions 
 * \param addr Adress pointed at by the operand
 * \param destination Register that will receive the address
 */
static void insert_set_operands_addr(void *drcontext, instrlist_t *bb, instr_t *where, void *addr, reg_id_t destination){
    MINSERT(bb, where, XINST_CREATE_load_int(drcontext, OP_REG(destination), OPND_CREATE_INTPTR(addr)));
}

/**
 * \brief Inserts prior to \p where meta-instructions to set the calling convention registers to the right adresses when one of the operands is a memory reference
 * \details Assumes the GPR have been saved !
 * \param drcontext DynamoRIO's context
 * \param bb Current Basic Block
 * \param where instruction prior to whom we insert the meta-instructions 
 * \param instr Instrumented instruction
 * \param fused true if the instruction is a fused operator (fma, fms...)
 * \param mem_src_idx index of the operand that is a memory reference
 * \param out_regs array containing the registers (in order) that shoud be set for the calling convention
 */
static void insert_set_operands_mem_reference(void *drcontext, instrlist_t *bb, instr_t *where, instr_t *instr, bool fused,
                                              int mem_src_idx, reg_id_t out_regs[]){
    DR_ASSERT_MSG((mem_src_idx >= 0) && (instr_num_srcs(instr) > mem_src_idx), "MEMORY SOURCE INDEX IS INVALID");
    opnd_t mem_src = SRC(instr, mem_src_idx);
    if(OP_IS_BASE_DISP(mem_src)){
        insert_set_operands_base_disp(drcontext, bb, where, mem_src, out_regs[mem_src_idx], out_regs[mem_src_idx == 0 ? 1 : 0]);
    }else if(OP_IS_ADDR(mem_src)){
        void *addr = opnd_get_addr(mem_src);
        insert_set_operands_addr(drcontext, bb, where, addr, out_regs[mem_src_idx]);
    }else{
        DR_ASSERT_MSG(false, "OPERAND IS NOT A MEMORY REFERENCE");
    }
    //Index in which we'll store the tls, we want it to be the last so that way we override it only at the end
    int last_reg_idx = fused ? 2 : 1;
    //We don't want to override the address we put before
    if(mem_src_idx == last_reg_idx){ --last_reg_idx; }
    INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, out_regs[last_reg_idx]);

    for(int i = 0; i < (fused ? 3 : 2); i++){
        if(i != mem_src_idx){
            reg_id_t reg = GET_REG(SRC(instr, i));
            MINSERT(bb, where, INSTR_CREATE_lea(drcontext,
                                                OP_REG(out_regs[i]),
                                                OP_BASE_DISP(out_regs[last_reg_idx], offset_of_simd(reg), OPSZ_lea)));
        }
    }
}

/**
 * \brief Inserts prior to \p where meta-instructions to set the calling convention registers to the right adresses
 * \details Assumes the GPR have been saved !
 * \param drcontext DynamoRIO's context
 * \param bb Current Basic Block
 * \param where instruction prior to whom we insert the meta-instructions 
 * \param instr Instrumented instruction
 * \param oc Operation category of the instrumented instruction
 */
void insert_set_operands(void *drcontext, instrlist_t *bb, instr_t *where, instr_t *instr, OPERATION_CATEGORY oc){
    reg_id_t reg_op_addr[3];
    const bool fused = plc_is_fused(oc);
    if(fused){
        if(oc & PLC_OP_213){
            reg_op_addr[0] = DR_REG_OP_B_ADDR;
            reg_op_addr[1] = DR_REG_OP_C_ADDR;
            reg_op_addr[2] = DR_REG_OP_A_ADDR;
        }else if(oc & PLC_OP_231){
            reg_op_addr[0] = DR_REG_OP_B_ADDR;
            reg_op_addr[1] = DR_REG_OP_A_ADDR;
            reg_op_addr[2] = DR_REG_OP_C_ADDR;
        }else if(oc & PLC_OP_132){
            reg_op_addr[0] = DR_REG_OP_C_ADDR;
            reg_op_addr[1] = DR_REG_OP_A_ADDR;
            reg_op_addr[2] = DR_REG_OP_B_ADDR;
        }else{
            DR_ASSERT_MSG(false, "ORDER OF FUSED UNKNOWN");
        }
    }else{
        //Due to the fact that implicit sources are put at the end of the list of sources, SSE instructions have their operands reversed
        //This may change in the future, so we have a check on Linux (get_need_sse_inverse)
        if(get_need_sse_inverse() && INSTR_IS_ANY_SSE(instr)){
            reg_op_addr[0] = DR_REG_OP_B_ADDR;
            reg_op_addr[1] = DR_REG_OP_A_ADDR;
        }else{
            reg_op_addr[0] = DR_REG_OP_A_ADDR;
            reg_op_addr[1] = DR_REG_OP_B_ADDR;
        }
        reg_op_addr[2] = DR_REG_NULL;
    }

    int mem_src = -1;
    //Get the index of the memory operand, if there is one
    for(uint i = 0; i < (fused ? 3U : 2U); i++){
        opnd_t src = SRC(instr, i);
        if(OP_IS_ADDR(src) || OP_IS_BASE_DISP(src)){
            mem_src = (int)i;
            break;
        }
    }
    if(mem_src == -1){
        //No memory references, only registers
        insert_set_operands_all_registers(drcontext, bb, where, GET_REG(SRC(instr, 0)), GET_REG(SRC(instr, 1)),
                                          fused ? GET_REG(SRC(instr, 2)) : DR_REG_NULL, reg_op_addr);
    }else{
        //Memory reference
        insert_set_operands_mem_reference(drcontext, bb, where, instr, fused, mem_src, reg_op_addr);
    }
}

/**
 * \brief Inserts prior to \p where meta-instructions to restore RSP from its saved value
 * 
 * \param drcontext DynamoRIO's context
 * \param bb Current Basic Block
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_restore_rsp(void *drcontext, instrlist_t *bb, instr_t *where){
    INSERT_READ_TLS(drcontext, get_index_tls_gpr(), bb, where, DR_REG_RSP);
    MINSERT(bb, where,
            XINST_CREATE_load(drcontext, OP_REG(DR_REG_RSP), OP_BASE_DISP(DR_REG_RSP, offset_of_gpr(DR_REG_RSP), OPSZ_8)));
}