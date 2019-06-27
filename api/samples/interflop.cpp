/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "interflop/interflop_operations.hpp"
#include "interflop/interflop_compute.hpp"

#ifndef MAX_INSTR_OPND_COUNT

// Generally floating operations needs three operands (2src + 1 dst) and FMA needs 4 (3src + 1dst)
#define MAX_INSTR_OPND_COUNT 4
#endif

#ifndef MAX_OPND_SIZE_BYTES

// operand size is maximum 512 bits (AVX512 instructions) = 64 bytes 
#define MAX_OPND_SIZE_BYTES 64 
#endif

#define INTERFLOP_BUFFER_SIZE (MAX_INSTR_OPND_COUNT*MAX_OPND_SIZE_BYTES)


static void event_exit(void);

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,         //TODO
                                            bool translating);      //TODO

// Main function to setup the dynamoRIO client
DR_EXPORT void dr_client_main(  client_id_t id, // client ID
                                int argc,   
                                const char *argv[])
{
    // Init DynamoRIO MGR extension ()
    drmgr_init();
    
    // Define the functions to be called before exiting this client program
    dr_register_exit_event(event_exit);

    // Define the function to executed to treat each instructions block
    drmgr_register_bb_app2app_event(event_basic_block, NULL);

}

static void event_exit(void)
{
    drmgr_exit();
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline bool insert_corresponding_parameters(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(instr_num_srcs(instr) == 2)
    {
        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);
        opnd_t dst = instr_get_dst(instr, 0);
        if(opnd_is_reg(src1) && opnd_is_reg(dst))
        {
            reg_id_t reg_src1 = opnd_get_reg(src1), reg_dst = opnd_get_reg(dst);
            if(opnd_is_reg(src0))
            {
                //reg reg -> reg
                reg_id_t reg_src0 = opnd_get_reg(src0);
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_reg<FTYPE, FN, SIMD_TYPE>, false, 4,OPND_CREATE_INT32(reg_src0),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(DR_MC_MULTIMEDIA));
                return true;
            }else if(opnd_is_base_disp(src0))
            {
                reg_id_t base = opnd_get_base(src0);
                int flags = DR_MC_MULTIMEDIA;
                if(base == DR_REG_XSP) //Cannot find XIP
                {
                    flags |= DR_MC_CONTROL;
                }else if(reg_is_gpr(base))
                {
                    flags |= DR_MC_INTEGER;
                }
                long disp = opnd_get_disp(src0);
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_base_disp<FTYPE, FN, SIMD_TYPE>, false, 5, OPND_CREATE_INT32(base),OPND_CREATE_INT64(disp),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(flags));
                return true;

            }else if(opnd_is_rel_addr(src0) || opnd_is_abs_addr(src0))
            {
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_addr<FTYPE, FN, SIMD_TYPE>, false, 4, OPND_CREATE_INTPTR(opnd_get_addr(src0)),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(DR_MC_MULTIMEDIA));
                return true;
            }
        }
    }
    return false;
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE)>
inline bool insert_corresponding_packed(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    switch(oc & IFP_SIMD_TYPE_MASK)
    {
        case IFP_OP_128:
            return insert_corresponding_parameters<FTYPE, FN, IFP_OP_128>(drcontext, bb, instr, oc);
        case IFP_OP_256:
            return insert_corresponding_parameters<FTYPE, FN, IFP_OP_256>(drcontext, bb, instr, oc);
        case IFP_OP_512:
            return insert_corresponding_parameters<FTYPE, FN, IFP_OP_512>(drcontext, bb, instr, oc);
        default:
            return false;
    }
}

template<typename FTYPE, bool packed>
inline bool insert_corresponding_operation(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if (packed)
    {
        switch (oc & IFP_OP_TYPE_MASK)
        {
            case IFP_OP_ADD:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::add>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_SUB:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::sub>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_MUL:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::mul>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_DIV:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::div>(drcontext, bb, instr, oc);
            break;
            default:
                return false;
            break;
        }
    }else{
        switch (oc & IFP_OP_TYPE_MASK)
        {
            case IFP_OP_ADD:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::add, 0>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_SUB:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::sub, 0>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_MUL:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::mul, 0>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_DIV:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::div, 0>(drcontext, bb, instr, oc);
            break;
            default:
                return false;
            break;
        }
    }
}

inline bool insert_corresponding_call(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(ifp_is_double(oc))
    {
        if(ifp_is_packed(oc))
        {
            return insert_corresponding_operation<double,true>(drcontext, bb, instr, oc);
        }else if(ifp_is_scalar(oc))
        {
            return insert_corresponding_operation<double,false>(drcontext, bb, instr, oc);
        }
    }else if(ifp_is_float(oc))
    {
        if(ifp_is_packed(oc))
        {
            return insert_corresponding_operation<float,true>(drcontext, bb, instr, oc);
        }else if(ifp_is_scalar(oc))
        {
            return insert_corresponding_operation<float,false>(drcontext, bb, instr, oc);
        }
    }
    
    return false;
}


static dr_emit_flags_t event_basic_block(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    OPERATION_CATEGORY oc;

    for(instr = instrlist_first(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next(instr);
        oc = ifp_get_operation_category(instr);
        if(oc)
        {
            dr_print_instr(drcontext, STDERR, instr, "Found : ");

            if(insert_corresponding_call(drcontext, bb, instr, oc))
            {
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
            }else
            {
                dr_printf("Bad oc\n");
            }
            
            //dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_operation<double> : (void*)interflop_operation<float>, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)) , opnd_create_immed_int(oc & IFP_OP_MASK , OPSZ_4));
            
        }

    }
    return DR_EMIT_DEFAULT;
}
