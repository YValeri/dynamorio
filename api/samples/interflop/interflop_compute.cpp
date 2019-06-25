#include "interflop_compute.hpp"
#include "interflop_operations.hpp"
#include "./backend/interflop/backend.hxx"
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include <string.h>

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