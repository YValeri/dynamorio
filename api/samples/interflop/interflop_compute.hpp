#ifndef INTERFLOP_COMPUTE_HEADER
#define INTERFLOP_COMPUTE_HEADER

#include "./backend/interflop/backend.hxx"
#include "interflop_operations.hpp"
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include <string.h>


template <typename T>
void get_value_from_opnd(void *drcontext , dr_mcontext_t mcontext , opnd_t src , T *res, int size_mask) {
    if(!opnd_is_null(src)) {
        if(opnd_is_reg(src)) {
           reg_get_value_ex(opnd_get_reg(src) , &mcontext , (byte*)res);
        }
        else if(opnd_is_base_disp(src)) {
            reg_id_t base = opnd_get_base(src);
            int disp = opnd_get_disp(src);
            unsigned long *addr;

            reg_get_value_ex(base , &mcontext , (byte*)&addr);
            switch(size_mask)
            {
                case IFP_OP_512:
                    memcpy((void*)res , ((byte*)addr)+disp , 64);
                break;
                case IFP_OP_256:
                    memcpy((void*)res , ((byte*)addr)+disp , 32);
                break;
                case IFP_OP_128:
                    memcpy((void*)res , ((byte*)addr)+disp , 16);
                break;
                    memcpy((void*)res , ((byte*)addr)+disp , sizeof(T));
            }
            
        }
        else if(opnd_is_abs_addr(src) || opnd_is_rel_addr(src)) {
            switch(size_mask)
            {
                case IFP_OP_512:
                    memcpy((void*)res , opnd_get_addr(src) , 64);
                break;
                case IFP_OP_256:
                    memcpy((void*)res , opnd_get_addr(src) , 32);
                break;
                case IFP_OP_128:
                    memcpy((void*)res , opnd_get_addr(src) , 16);
                break;
                    memcpy((void*)res , opnd_get_addr(src) , sizeof(T));
            }
            //memcpy((void*)res , opnd_get_addr(src) , sizeof(T));
        }
        else if(opnd_is_immed(src)) {
            // Possible only with float
            *res = opnd_get_immed_float(src);
        }

    }
}

static inline void get_instr_and_context(app_pc pc, void* drcontext, OUT instr_t* instr, OUT dr_mcontext_t *mcontext)
{
    mcontext->size = sizeof(*mcontext);
    mcontext->flags = DR_MC_ALL;
    dr_get_mcontext(drcontext , mcontext);
    instr_init(drcontext, instr);
    decode(drcontext , pc , instr);
}

static inline void push_result_and_free(instr_t * instr, byte* value, void* drcontext, dr_mcontext_t* mcontext)
{
    reg_id_t dst = opnd_get_reg(instr_get_dst(instr,0));
    reg_set_value_ex(dst , mcontext , value);
    dr_set_mcontext(drcontext , mcontext);
    instr_free(drcontext , instr);
}

template <typename T>
void interflop_operation(app_pc apppc , int operation);
template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
static inline void execute(FTYPE* src0, FTYPE* src1, FTYPE* res)
{
    constexpr unsigned int maxiter = SIMD_TYPE == 0 ? 1 : (SIMD_TYPE == IFP_OP_128 ? 4 : SIMD_TYPE == IFP_OP_256 ? 8 : 16)/((sizeof(FTYPE))/sizeof(float));
    for(unsigned int i=0; i<maxiter; i++)
    {
        res[i] = FN(src1[i], src0[i]);
    }
}

void initContext(dr_mcontext_flags_t flags, void* drcontext, OUT dr_mcontext_t* mcontext)
{
    mcontext->size = sizeof(*mcontext);
    mcontext->flags = flags;
    dr_get_mcontext(drcontext , mcontext);
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
void interflop_operation_reg(const reg_id_t reg_src0, const reg_id_t reg_src1, const reg_id_t reg_dst, const dr_mcontext_flags_t flags)
{
    dr_mcontext_t mcontext;
    void* drcontext = dr_get_current_drcontext();
    initContext(flags, drcontext, &mcontext);

    constexpr size_t size = sizeof(dr_zmm_t)/sizeof(FTYPE);
    FTYPE src0[size], src1[size], res[size];
    reg_get_value_ex(reg_src0, &mcontext, (byte*)src0);
    reg_get_value_ex(reg_src1, &mcontext, (byte*)src1);

    execute<FTYPE, FN, SIMD_TYPE>(src0, src1, res);

    reg_set_value_ex(reg_dst, &mcontext, (byte*)res);
    dr_set_mcontext(drcontext , &mcontext);
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
void interflop_operation_base_disp(reg_id_t base, long disp, reg_id_t reg_src1, reg_id_t reg_dst, const dr_mcontext_flags_t flags)
{
    dr_mcontext_t mcontext;
    void* drcontext = dr_get_current_drcontext();
    initContext(flags, drcontext, &mcontext);

    constexpr size_t size = sizeof(dr_zmm_t)/sizeof(FTYPE);
    FTYPE src0[size], src1[size], res[size];
    byte *addr;
    reg_get_value_ex(base , &mcontext , (byte*)&addr);
    constexpr size_t copysize = SIMD_TYPE == 0 ? sizeof(FTYPE) : SIMD_TYPE == IFP_OP_128 ? 16 : SIMD_TYPE == IFP_OP_256 ? 32 : 64;
    memcpy(src0, addr+disp, copysize);
    reg_get_value_ex(reg_src1, &mcontext, (byte*)src1);

    execute<FTYPE, FN, SIMD_TYPE>(src0, src1, res);

    reg_set_value_ex(reg_dst, &mcontext, (byte*)res);
    dr_set_mcontext(drcontext , &mcontext);
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
void interflop_operation_addr(void* addr, reg_id_t reg_src1, reg_id_t reg_dst, const dr_mcontext_flags_t flags)
{
    dr_mcontext_t mcontext;
    void* drcontext = dr_get_current_drcontext();
    initContext(flags, drcontext, &mcontext);

    constexpr size_t size = sizeof(dr_zmm_t)/sizeof(FTYPE);
    FTYPE src0[size], src1[size], res[size];
    constexpr size_t copysize = SIMD_TYPE == 0 ? sizeof(FTYPE) : SIMD_TYPE == IFP_OP_128 ? 16 : SIMD_TYPE == IFP_OP_256 ? 32 : 64;
    memcpy(src0, addr, copysize);
    reg_get_value_ex(reg_src1, &mcontext, (byte*)src1);

    execute<FTYPE, FN, SIMD_TYPE>(src0, src1, res);

    reg_set_value_ex(reg_dst, &mcontext, (byte*)res);
    dr_set_mcontext(drcontext , &mcontext);
}

template<>
void interflop_operation<double>(app_pc apppc , int operation)
{  
    instr_t instr;
    dr_mcontext_t mcontext;
    void *drcontext = dr_get_current_drcontext();
    get_instr_and_context(apppc, drcontext, &instr, &mcontext);
    double src0[8];
    double src1[8];
    get_value_from_opnd<double>(drcontext, mcontext, instr_get_src(&instr, 0), src0, operation & IFP_SIMD_TYPE_MASK);
    get_value_from_opnd<double>(drcontext, mcontext, instr_get_src(&instr, 1), src1, operation & IFP_SIMD_TYPE_MASK);

    double res[8];
    switch(operation & IFP_OP_TYPE_MASK) {
        case IFP_OP_ADD:
            switch(operation & IFP_SIMD_TYPE_MASK)
            {
               case IFP_OP_512:
                //dr_printf("512\n");
                res[7] = Interflop::Op<double>::add(src1[7], src0[7]);
                res[6] = Interflop::Op<double>::add(src1[6], src0[6]);
                res[5] = Interflop::Op<double>::add(src1[5], src0[5]);
                res[4] = Interflop::Op<double>::add(src1[4], src0[4]);
                case IFP_OP_256:
                //dr_printf("256\n");
                res[3] = Interflop::Op<double>::add(src1[3], src0[3]);
                res[2] = Interflop::Op<double>::add(src1[2], src0[2]);
                case IFP_OP_128:
                //dr_printf("128\n");
                res[1] = Interflop::Op<double>::add(src1[1], src0[1]);
                default:
                //dr_printf("scalar\n");
                res[0] = Interflop::Op<double>::add(src1[0], src0[0]);
            }
            
            break;

        case IFP_OP_SUB:
            switch(operation & IFP_SIMD_TYPE_MASK)
            {
                case IFP_OP_512:
                //dr_printf("512\n");
                res[7] = Interflop::Op<double>::sub(src1[7], src0[7]);
                res[6] = Interflop::Op<double>::sub(src1[6], src0[6]);
                res[5] = Interflop::Op<double>::sub(src1[5], src0[5]);
                res[4] = Interflop::Op<double>::sub(src1[4], src0[4]);
                case IFP_OP_256:
                //dr_printf("256\n");
                res[3] = Interflop::Op<double>::sub(src1[3], src0[3]);
                res[2] = Interflop::Op<double>::sub(src1[2], src0[2]);
                case IFP_OP_128:
                //dr_printf("128\n");
                res[1] = Interflop::Op<double>::sub(src1[1], src0[1]);
                default:
                //dr_printf("scalar\n");
                res[0] = Interflop::Op<double>::sub(src1[0], src0[0]);
            }
            break;

        case IFP_OP_MUL:
             switch(operation & IFP_SIMD_TYPE_MASK)
            {
                case IFP_OP_512:
                //dr_printf("512\n");
                res[7] = Interflop::Op<double>::mul(src1[7], src0[7]);
                res[6] = Interflop::Op<double>::mul(src1[6], src0[6]);
                res[5] = Interflop::Op<double>::mul(src1[5], src0[5]);
                res[4] = Interflop::Op<double>::mul(src1[4], src0[4]);
                case IFP_OP_256:
                //dr_printf("256\n");
                res[3] = Interflop::Op<double>::mul(src1[3], src0[3]);
                res[2] = Interflop::Op<double>::mul(src1[2], src0[2]);
                case IFP_OP_128:
                //dr_printf("128\n");
                res[1] = Interflop::Op<double>::mul(src1[1], src0[1]);
                default:
                //dr_printf("scalar\n");
                res[0] = Interflop::Op<double>::mul(src1[0], src0[0]);
            }
            break;

        case IFP_OP_DIV:
             switch(operation & IFP_SIMD_TYPE_MASK)
            {
                case IFP_OP_512:
                //dr_printf("512\n");
                res[7] = Interflop::Op<double>::div(src1[7], src0[7]);
                res[6] = Interflop::Op<double>::div(src1[6], src0[6]);
                res[5] = Interflop::Op<double>::div(src1[5], src0[5]);
                res[4] = Interflop::Op<double>::div(src1[4], src0[4]);
                case IFP_OP_256:
                //dr_printf("256\n");
                res[3] = Interflop::Op<double>::div(src1[3], src0[3]);
                res[2] = Interflop::Op<double>::div(src1[2], src0[2]);
                case IFP_OP_128:
                //dr_printf("128\n");
                res[1] = Interflop::Op<double>::div(src1[1], src0[1]);
                default:
                //dr_printf("scalar\n");
                res[0] = Interflop::Op<double>::div(src1[0], src0[0]);
            }
            break;
        
    }
    push_result_and_free(&instr, (byte*)&res, drcontext, &mcontext);
}

template <>
void interflop_operation<float>(app_pc apppc , int operation)
{  
    instr_t instr;
    dr_mcontext_t mcontext;
    void *drcontext = dr_get_current_drcontext();
    get_instr_and_context(apppc, drcontext, &instr, &mcontext);

    float src0[16];
    float src1[16];
    get_value_from_opnd<float>(drcontext, mcontext, instr_get_src(&instr, 0), src0, operation & IFP_SIMD_TYPE_MASK);
    get_value_from_opnd<float>(drcontext, mcontext, instr_get_src(&instr, 1), src1, operation & IFP_SIMD_TYPE_MASK);

    float res[16];
    switch(operation & IFP_OP_TYPE_MASK) {
        case IFP_OP_ADD:
            switch(operation & IFP_SIMD_TYPE_MASK)
            {
                case IFP_OP_512:
                res[15] = Interflop::Op<float>::add(src1[15], src0[15]);
                res[14] = Interflop::Op<float>::add(src1[14], src0[14]);
                res[13] = Interflop::Op<float>::add(src1[13], src0[13]);
                res[12] = Interflop::Op<float>::add(src1[12], src0[12]);
                res[11] = Interflop::Op<float>::add(src1[11], src0[11]);
                res[10] = Interflop::Op<float>::add(src1[10], src0[10]);
                res[9] = Interflop::Op<float>::add(src1[9], src0[9]);
                res[8] = Interflop::Op<float>::add(src1[8], src0[8]);
                case IFP_OP_256:
                res[7] = Interflop::Op<float>::add(src1[7], src0[7]);
                res[6] = Interflop::Op<float>::add(src1[6], src0[6]);
                res[5] = Interflop::Op<float>::add(src1[5], src0[5]);
                res[4] = Interflop::Op<float>::add(src1[4], src0[4]);
                case IFP_OP_128:
                res[3] = Interflop::Op<float>::add(src1[3], src0[3]);
                res[2] = Interflop::Op<float>::add(src1[2], src0[2]);
                res[1] = Interflop::Op<float>::add(src1[1], src0[1]);
                default:
                res[0] = Interflop::Op<float>::add(src1[0], src0[0]);
            }
            
            break;

        case IFP_OP_SUB:
             switch(operation & IFP_SIMD_TYPE_MASK)
            {
                case IFP_OP_512:
                res[15] = Interflop::Op<float>::sub(src1[15], src0[15]);
                res[14] = Interflop::Op<float>::sub(src1[14], src0[14]);
                res[13] = Interflop::Op<float>::sub(src1[13], src0[13]);
                res[12] = Interflop::Op<float>::sub(src1[12], src0[12]);
                res[11] = Interflop::Op<float>::sub(src1[11], src0[11]);
                res[10] = Interflop::Op<float>::sub(src1[10], src0[10]);
                res[9] = Interflop::Op<float>::sub(src1[9], src0[9]);
                res[8] = Interflop::Op<float>::sub(src1[8], src0[8]);
                case IFP_OP_256:
                res[7] = Interflop::Op<float>::sub(src1[7], src0[7]);
                res[6] = Interflop::Op<float>::sub(src1[6], src0[6]);
                res[5] = Interflop::Op<float>::sub(src1[5], src0[5]);
                res[4] = Interflop::Op<float>::sub(src1[4], src0[4]);
                case IFP_OP_128:
                res[3] = Interflop::Op<float>::sub(src1[3], src0[3]);
                res[2] = Interflop::Op<float>::sub(src1[2], src0[2]);
                res[1] = Interflop::Op<float>::sub(src1[1], src0[1]);
                default:
                res[0] = Interflop::Op<float>::sub(src1[0], src0[0]);
            }
            break;

        case IFP_OP_MUL:
             switch(operation & IFP_SIMD_TYPE_MASK)
            {
                case IFP_OP_512:
                res[15] = Interflop::Op<float>::mul(src1[15], src0[15]);
                res[14] = Interflop::Op<float>::mul(src1[14], src0[14]);
                res[13] = Interflop::Op<float>::mul(src1[13], src0[13]);
                res[12] = Interflop::Op<float>::mul(src1[12], src0[12]);
                res[11] = Interflop::Op<float>::mul(src1[11], src0[11]);
                res[10] = Interflop::Op<float>::mul(src1[10], src0[10]);
                res[9] = Interflop::Op<float>::mul(src1[9], src0[9]);
                res[8] = Interflop::Op<float>::mul(src1[8], src0[8]);
                case IFP_OP_256:
                res[7] = Interflop::Op<float>::mul(src1[7], src0[7]);
                res[6] = Interflop::Op<float>::mul(src1[6], src0[6]);
                res[5] = Interflop::Op<float>::mul(src1[5], src0[5]);
                res[4] = Interflop::Op<float>::mul(src1[4], src0[4]);
                case IFP_OP_128:
                res[3] = Interflop::Op<float>::mul(src1[3], src0[3]);
                res[2] = Interflop::Op<float>::mul(src1[2], src0[2]);
                res[1] = Interflop::Op<float>::mul(src1[1], src0[1]);
                default:
                res[0] = Interflop::Op<float>::mul(src1[0], src0[0]);
            }
            break;

        case IFP_OP_DIV:
             switch(operation & IFP_SIMD_TYPE_MASK)
            {
                case IFP_OP_512:
                res[15] = Interflop::Op<float>::div(src1[15], src0[15]);
                res[14] = Interflop::Op<float>::div(src1[14], src0[14]);
                res[13] = Interflop::Op<float>::div(src1[13], src0[13]);
                res[12] = Interflop::Op<float>::div(src1[12], src0[12]);
                res[11] = Interflop::Op<float>::div(src1[11], src0[11]);
                res[10] = Interflop::Op<float>::div(src1[10], src0[10]);
                res[9] = Interflop::Op<float>::div(src1[9], src0[9]);
                res[8] = Interflop::Op<float>::div(src1[8], src0[8]);
                case IFP_OP_256:
                res[7] = Interflop::Op<float>::div(src1[7], src0[7]);
                res[6] = Interflop::Op<float>::div(src1[6], src0[6]);
                res[5] = Interflop::Op<float>::div(src1[5], src0[5]);
                res[4] = Interflop::Op<float>::div(src1[4], src0[4]);
                case IFP_OP_128:
                res[3] = Interflop::Op<float>::div(src1[3], src0[3]);
                res[2] = Interflop::Op<float>::div(src1[2], src0[2]);
                res[1] = Interflop::Op<float>::div(src1[1], src0[1]);
                default:
                res[0] = Interflop::Op<float>::div(src1[0], src0[0]);
            }
            break;
        
    }
    push_result_and_free(&instr, (byte*)&res, drcontext, &mcontext);
}

#endif 