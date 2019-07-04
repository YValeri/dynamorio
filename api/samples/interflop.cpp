/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */
#include <map>
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

static int tls_index;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,         //TODO
                                            bool translating);      //TODO

static void thread_init_handler(void* drcontext)
{
    unsigned long long int randomValue = dr_get_random_value(0x80000000);
    drmgr_set_tls_field(drcontext, tls_index, (void*)randomValue);
}

// Main function to setup the dynamoRIO client
DR_EXPORT void dr_client_main(  client_id_t id, // client ID
                                int argc,   
                                const char *argv[])
{
    // Init DynamoRIO MGR extension ()
    drmgr_init();
    tls_index = drmgr_register_tls_field();
    
    // Define the functions to be called before exiting this client program
    dr_register_exit_event(event_exit);

    // Define the function to executed to treat each instructions block
    drmgr_register_bb_app2app_event(event_basic_block, NULL);

    drmgr_register_thread_init_event(thread_init_handler);

}

static void event_exit(void)
{
    drmgr_unregister_tls_field(tls_index);
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

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

static void test()
{
    void* dc = dr_get_current_drcontext();
    void* tls = drmgr_get_tls_field(dc, tls_index);
    dr_printf("%p\n", tls);
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
            //dr_print_instr(drcontext, STDERR, instr, "Found : ");
            //########### Adding randomized rounding before ##########
            /*
                pushf
                push        rax                                     ;save rax
                push        rdx                                     ;save rdx

                ;### Begin Park–Miller random number generator ###

                (meta)      drmgr_insert_read_tls_field into rdx    ;Get the last generated random value
                push        rcx                                     ;save rcx
                mov         rcx     48271
                mul         rdx     rcx
                mov         eax     edx
                shr         rdx     32
                mov         rcx     0x7FFFFFFF
                div         ecx                                     ;Modulo 0x7FFFFFFF
                pop         rcx                                     ;restore rcx
                mov         rax     rdx
                (meta)      drmgr_insert_write_tls_field from rax
                
                ;### End Park–Miller random number generator ###
                
                ;### Preparing the flag value ###
                mov         rdx     0x2000                          ;Low interesting bit mask
                and         rax     rdx                             ;Keeping the interesting bit (0 or 1)
                add         rax     rdx                             ;increment the register (01 (downward) or 10 (upward))
                
                ;### Modifying the flags
                sub         rsp     4
                stmxcsr     (rsp)                                   ;store the current flags
                mov         rdx     0xFFFF9FFF
                and         (rsp)   edx                             ;reset the rounding mode bits
                or          (rsp)   eax                             ;put our rounding mode
                ldmxcsr     (rsp)                                   ;load the current flags
                add         rsp     4                               ;get the stack back to normal


                pop         rdx                                     ;restore rdx
                pop         rax                                     ;restore rax
                popf
            */
            opnd_t rax = opnd_create_reg(DR_REG_RAX);
            opnd_t rdx = opnd_create_reg(DR_REG_RDX);
            opnd_t rcx = opnd_create_reg(DR_REG_RCX);
            opnd_t eax = opnd_create_reg(DR_REG_EAX);
            opnd_t ecx = opnd_create_reg(DR_REG_ECX);
            opnd_t edx = opnd_create_reg(DR_REG_EDX);
            opnd_t rsp_base_32 = opnd_create_base_disp(DR_REG_RSP, DR_REG_NULL, 0, 0, OPSZ_4);

            translate_insert(INSTR_CREATE_pushf(drcontext), bb, instr);
            translate_insert(INSTR_CREATE_push(drcontext, rax),bb, instr);
            translate_insert(INSTR_CREATE_push(drcontext, rdx), bb, instr);

            drmgr_insert_read_tls_field(drcontext, tls_index, bb, instr, DR_REG_RDX);
            translate_insert(INSTR_CREATE_push(drcontext, rcx), bb, instr);

            translate_insert(INSTR_CREATE_mov_imm(drcontext, rcx,opnd_create_immed_uint(48271, reg_get_size(DR_REG_RCX))),bb, instr);
            translate_insert(INSTR_CREATE_imul(drcontext, rdx, rcx), bb, instr);
            translate_insert(INSTR_CREATE_mov_ld(drcontext, eax, edx), bb, instr);
            translate_insert(INSTR_CREATE_shr(drcontext, rdx, opnd_create_immed_uint(32, OPSZ_1)), bb, instr);
            translate_insert(INSTR_CREATE_mov_imm(drcontext, rcx, opnd_create_immed_uint(0x7FFFFFFF, reg_get_size(DR_REG_RCX))),bb, instr);
            translate_insert(INSTR_CREATE_div_4(drcontext, ecx), bb, instr);
            translate_insert(INSTR_CREATE_pop(drcontext, rcx), bb, instr);
            translate_insert(INSTR_CREATE_mov_ld(drcontext, rax, rdx), bb, instr);

            drmgr_insert_write_tls_field(drcontext, tls_index, bb, instr, DR_REG_RAX, DR_REG_RDX);

            translate_insert(INSTR_CREATE_mov_imm(drcontext, rdx,opnd_create_immed_int(0X2000, OPSZ_4)), bb, instr);
            translate_insert(INSTR_CREATE_and(drcontext, rax, rdx),bb, instr);
            translate_insert(INSTR_CREATE_add(drcontext, rax, rdx), bb, instr);

            translate_insert(INSTR_CREATE_sub(drcontext, opnd_create_reg(DR_REG_RSP), opnd_create_immed_int(4, OPSZ_4)), bb, instr);
            translate_insert(INSTR_CREATE_stmxcsr(drcontext, rsp_base_32), bb, instr);
            translate_insert(INSTR_CREATE_mov_imm(drcontext, rdx, opnd_create_immed_uint(0xFFFF9FFF, reg_get_size(DR_REG_RDX))), bb, instr);
            translate_insert(INSTR_CREATE_and(drcontext, rsp_base_32, edx), bb, instr);
            translate_insert(INSTR_CREATE_or(drcontext, rsp_base_32, eax), bb, instr);
            translate_insert(INSTR_CREATE_ldmxcsr(drcontext, rsp_base_32), bb, instr);
            translate_insert(INSTR_CREATE_add(drcontext, opnd_create_reg(DR_REG_RSP), opnd_create_immed_int(4, OPSZ_4)), bb, instr);


            translate_insert(INSTR_CREATE_pop(drcontext, rdx),bb, instr);
            translate_insert(INSTR_CREATE_pop(drcontext, rax),bb, instr);
            translate_insert(INSTR_CREATE_popf(drcontext), bb, instr);
            
           

           //########## Resetting the rounding mode after #########
           /*
                pushf
                push        rax
                sub         rsp     4               ;get the stack 4 bytes ahead
                stmxcsr     (rsp)                   ;store the current control flags
                mov         rax     0xFFFF9FFF
                and         (rsp)   rax             ;reset the rounding bits to 00 (nearest)
                ldmxcsr     (rsp)                   ;load the modified control flags
                add         rsp     4               ;get the stack back to normal
                pop         rax
                popf
            */
           translate_insert(INSTR_CREATE_pushf(drcontext), bb, next_instr);
           translate_insert(INSTR_CREATE_push(drcontext, rax),bb, next_instr);
           translate_insert(INSTR_CREATE_sub(drcontext, opnd_create_reg(DR_REG_RSP), opnd_create_immed_int(4, OPSZ_4)), bb, next_instr);
           translate_insert(INSTR_CREATE_stmxcsr(drcontext, rsp_base_32), bb, next_instr);
           translate_insert(INSTR_CREATE_mov_imm(drcontext, rax, opnd_create_immed_uint(0xFFFF9FFF, reg_get_size(DR_REG_RAX))), bb, next_instr);
           translate_insert(INSTR_CREATE_and(drcontext, rsp_base_32, eax), bb, next_instr);
           translate_insert(INSTR_CREATE_ldmxcsr(drcontext, rsp_base_32), bb, next_instr);
           translate_insert(INSTR_CREATE_add(drcontext, opnd_create_reg(DR_REG_RSP), opnd_create_immed_int(4, OPSZ_4)), bb, next_instr);
           translate_insert(INSTR_CREATE_pop(drcontext, rax),bb, next_instr);
           translate_insert(INSTR_CREATE_popf(drcontext), bb, next_instr);
           




            /*
            if(insert_corresponding_call(drcontext, bb, instr, oc))
            {
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
            }else
            {
                dr_printf("Bad oc\n");
            }*/
            
            //dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_operation<double> : (void*)interflop_operation<float>, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)) , opnd_create_immed_int(oc & IFP_OP_MASK , OPSZ_4));
            
        }

    }
    return DR_EMIT_DEFAULT;
}
