/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "stdint.h"
#include "interflop/interflop_operations.hpp"
#include "interflop/interflop_compute.hpp"
#include "interflop/backend/interflop.h"

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

static void *testcontext;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,        
                                            bool translating);      

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

}

static void event_exit(void)
{
    drmgr_unregister_tls_field(tls_index);

    drmgr_exit();
    drreg_exit();
}

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

/*static void push_result_to_register(void* drcontext,instrlist_t *ilist, instr_t* instr, 
        bool removeInstr, bool is_double)
{
    if(instr && ilist && drcontext)
    {
        if(!removeInstr)
        {
            instr_t* copy = instr_clone(drcontext, instr);
            instr_set_translation(copy, instr_get_app_pc(instr));
            instr_set_app(copy);
            instrlist_preinsert(ilist,instr, copy);
        }
        

        int num_dst = instr_num_dsts(instr);
        if(num_dst > 0)
        {
            opnd_t op, opDoF, op64, opST0;
            reg_t reg;

            op = instr_get_dst(instr, 0);

            opDoF = opnd_create_rel_addr(*resultBuffer_ind, is_double? OPSZ_8: OPSZ_4);

            opST0 = opnd_create_reg(DR_REG_ST0);
            //op64 = opnd_create_reg(DR_REG_START_64);

//#ifdef SHOW_RESULTS
            dr_print_opnd(drcontext, STDERR, op, "DST : ");
//#endif
            if(opnd_is_reg(op))
            {
                reg = opnd_get_reg(op);
                if(reg_is_simd(reg)){ 
                    //SIMD scalar
                    translate_insert(INSTR_CREATE_movsd(drcontext, op, opDoF), ilist, instr);
                }else if(reg_is_mmx(reg)){ 
                    //Intel MMX
                    translate_insert(INSTR_CREATE_movq(drcontext, op, opDoF), ilist, instr);
                }else{ 
                    //General purpose register
                    translate_insert(INSTR_CREATE_mov_ld(drcontext, op, opDoF), ilist, instr);
                }
                //TODO complete if necessary
            }else if(opnd_is_immed(op)){ // Immediate value
            
                dr_printf("immed\n");
                translate_insert(INSTR_CREATE_mov_imm(drcontext, op, opDoF), ilist, instr);
            }else if(opnd_is_memory_reference(op)){
                reg_id_t reserved_reg;
                dr_printf("memref\n");
                drreg_reserve_register(drcontext, ilist, instr, NULL, &reserved_reg);
                op64 = opnd_create_reg(reserved_reg);
                
                if(opnd_is_rel_addr(op))
                    dr_printf("reladdr\n");
                if(opnd_is_base_disp(op))
                    dr_printf("Basdisp\n");
                if(opnd_is_abs_addr(op))
                    dr_printf("absaddr\n");
                if(opnd_is_pc(op))
                    dr_printf("pc\n");
                
                translate_insert(INSTR_CREATE_movq(drcontext, op64, opDoF), ilist, instr);
                translate_insert(INSTR_CREATE_movq(drcontext, op, op64), ilist, instr);

                drreg_unreserve_register(drcontext, ilist, instr, reserved_reg);
            }
        }
        instrlist_remove(ilist, instr);
        instr_destroy(drcontext, instr);
    }
}*/

inline void push_for_call(void* drcontext, opnd_t ebp, opnd_t esp, instrlist_t *ilist, instr_t* instr)
{
    opnd_t op12 = opnd_create_immed_int(12, OPSZ_4);
    opnd_t op8 = opnd_create_immed_int(8, OPSZ_4);
    opnd_t op0 = opnd_create_immed_int(0, OPSZ_4);

    /*dr_printf("first translate\n");
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");*/
    translate_insert(INSTR_CREATE_push(drcontext, ebp), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    dr_printf("second translate\n");
    translate_insert(XINST_CREATE_move(drcontext, ebp, esp), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //translate_insert(XINST_CREATE_sub(drcontext, esp, op8), ilist, instr);
    //translate_insert(XINST_CREATE_move(drcontext, opnd_create_reg(DR_REG_RAX), op0), ilist, instr);

    dr_printf("third translate\n");
    //translate_insert(INSTR_CREATE_push(drcontext, esp), ilist, instr);
    //translate_insert(INSTR_CREATE_push(drcontext, op0), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    dr_printf("fourth translate\n");
    //translate_insert(INSTR_CREATE_push(drcontext, esp), ilist, instr);
    /*translate_insert(XINST_CREATE_sub(drcontext, esp, op8), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    dr_printf("fifth translate\n");*/
    //translate_insert(INSTR_CREATE_movq(drcontext, 
    //    opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_STACK), esp), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    dr_printf("after fifth translate\n");
}

inline void push_for_ret(void* drcontext, opnd_t ebp, opnd_t esp, opnd_t dst, 
    instrlist_t *ilist, instr_t* instr)
{

    dr_printf("drmgr stuff\n");
    drmgr_insert_read_tls_field(drcontext, tls_index, ilist, instr, DR_REG_RAX);

    dr_printf("first translate\n");
    translate_insert(INSTR_CREATE_push(drcontext, opnd_create_reg(DR_REG_RAX)), ilist, instr);

    dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM0),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4)), ilist, instr);

    dr_printf("third translate\n");
    translate_insert(INSTR_CREATE_pop(drcontext, opnd_create_reg(DR_REG_RAX)), ilist, instr);
}


template<typename FTYPE>
inline int push_xmm(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr);

template<>
inline int push_xmm<double>(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr)
{
    dr_printf("first translate\n");
    translate_insert(XINST_CREATE_sub(drcontext, esp, 
        opnd_create_immed_int(16, OPSZ_4)), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movsd(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4), src), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    dr_printf("after second translate\n");

    return 8;
}

template<>
inline int push_xmm<float>(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr)
{
    dr_printf("first translate\n");
    translate_insert(XINST_CREATE_sub(drcontext, esp, 
        opnd_create_immed_int(16, OPSZ_4)), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movss(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4), src), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    dr_printf("after second translate\n");

    return 4;
}

inline void push_call(void *drcontext, void *callee, instrlist_t *ilist, instr_t* instr)
{
    dr_printf("first translate\n");
    /*movss   xmm0, DWORD PTR [rbp-4]
        addss   xmm0, DWORD PTR [rbp-8]
        mov     rax, QWORD PTR [rbp-16]
        movss   DWORD PTR [rax], xmm0*/
    translate_insert(INSTR_CREATE_call(drcontext, 
        opnd_create_pc((byte*) callee)), ilist, instr);
    dr_printf("after first translate\n");
}

static void test() {
    //return a + b
    float temp_A;
    float temp_B;

    asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
    dr_printf("one\n");
    dr_printf("tempa = %.30f\n", temp_A);

    asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
    dr_printf("two\n");
    dr_printf("temp_B = %.30f\n", temp_B);

    //Add the two values we got from xmm
    float temp_C = temp_A + temp_B + 2;
    dr_printf("three\n");
    dr_printf("temp_C = %.30f\n", temp_C);

    union{ float f; uint32_t ptr;};

    f = temp_C;

    drmgr_set_tls_field(testcontext, tls_index, (void*) ptr);
    
    dr_printf("pas\n");
}

static void printlol() {
    printf("on est sortie !\n");
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline bool insert_corresponding_parameters(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(instr_num_srcs(instr) == 2)
    {
        opnd_t ebp = opnd_create_reg(DR_REG_XBP);
        opnd_t esp = opnd_create_reg(DR_REG_XSP);
        dr_printf("before push_for_call\n");
        //push_for_call(drcontext, ebp, esp, bb, instr);
        dr_printf("after push_for_call\n");

        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);
        opnd_t dst = instr_get_dst(instr, 0);
        //instr_set_dst(instr, 0, opnd_shrink_to_32_bits(opnd_create_reg(DR_REG_XMM2)));

        if(opnd_is_reg(src1) && opnd_is_reg(dst))
        {
            dr_printf("before movss to xmm2\n");

            translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM2), 
                src1), bb, instr);

            dr_printf("after movss to xmm2\n");

            reg_id_t reg_src1 = opnd_get_reg(src1), reg_dst = opnd_get_reg(dst);
            if(opnd_is_reg(src0))
            {
                dr_printf("reg\n");
                //reg reg -> reg

                translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM7),
                    src0), bb, instr);
                
                dr_printf("before insert_call\n");
                dr_insert_call(drcontext, bb, instr, (void*)test, 0);
                dr_insert_call(drcontext, bb, instr, (void*)printlol, 0);

                dr_printf("before push_for_ret\n");
                push_for_ret(drcontext, ebp, esp, dst, bb, instr);
                dr_printf("after push_for_ret src\n");
                return true;
            }else if(opnd_is_base_disp(src0))
            {
                reg_id_t base = opnd_get_base(src0);
                dr_printf("base disp\n");
                int flags = DR_MC_MULTIMEDIA;
                if(base == DR_REG_XSP) //Cannot find XIP
                {
                    flags |= DR_MC_CONTROL;
                }else if(reg_is_gpr(base))
                {
                    flags |= DR_MC_INTEGER;
                }
                //long disp = opnd_get_disp(src0);

                translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM7),
                    src0), bb, instr);

                dr_printf("before insert_call\n");
    //asm volatile("\tmov -16(%%rbp), %%eax\n" : "=m" (tempb));
    //asm volatile("\tmov %%eax, %0\n" : "=m" (tempb));
                //translate_insert(INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_R10),
                //    src0), bb, instr);
                dr_insert_call(drcontext, bb, instr, (void*)test, 0);
                dr_insert_call(drcontext, bb, instr, (void*)printlol, 0);
                //dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_base_disp<FTYPE, FN, SIMD_TYPE>, false, 5, OPND_CREATE_INT32(base),OPND_CREATE_INT64(disp),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(flags));
                dr_printf("before push_for_ret\n");

                //translate_insert(INSTR_CREATE_movss(drcontext, src1,
                //    opnd_create_reg(DR_REG_EAX)), bb, instr);
                push_for_ret(drcontext, ebp, esp, dst, bb, instr);
                dr_printf("after push_for_ret src\n");
                dr_print_instr(drcontext, STDERR, instr, "Found : ");
                return true;

            }else if(opnd_is_rel_addr(src0) || opnd_is_abs_addr(src0))
            {
                dr_printf("before insert_call\n");
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_addr<FTYPE, FN, SIMD_TYPE>, false, 4, OPND_CREATE_INTPTR(opnd_get_addr(src0)),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(DR_MC_MULTIMEDIA));
                
                dr_printf("before push_for_ret\n");
                push_for_ret(drcontext, ebp, esp, dst, bb, instr);
                dr_printf("after push_for_ret src\n");
                return true;
            }
        }

        dr_printf("before push_for_ret\n");
        push_for_ret(drcontext, ebp, esp, dst, bb, instr);
        dr_printf("after push_for_ret src\n");
    }
    return false;
}

/*template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
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

        push_for_ret(drcontext, ebp, esp);
    }
    return false;
}*/

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

/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "stdint.h"
#include "interflop/interflop_operations.hpp"
#include "interflop/interflop_compute.hpp"
#include "interflop/backend/interflop.h"

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

//static void *testcontext;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,        
                                            bool translating);      

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

}

static void event_exit(void)
{
    drmgr_unregister_tls_field(tls_index);

    drmgr_exit();
    drreg_exit();
}

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

/*static void push_result_to_register(void* drcontext,instrlist_t *ilist, instr_t* instr, 
        bool removeInstr, bool is_double)
{
    if(instr && ilist && drcontext)
    {
        if(!removeInstr)
        {
            instr_t* copy = instr_clone(drcontext, instr);
            instr_set_translation(copy, instr_get_app_pc(instr));
            instr_set_app(copy);
            instrlist_preinsert(ilist,instr, copy);
        }
        

        int num_dst = instr_num_dsts(instr);
        if(num_dst > 0)
        {
            opnd_t op, opDoF, op64, opST0;
            reg_t reg;

            op = instr_get_dst(instr, 0);

            opDoF = opnd_create_rel_addr(*resultBuffer_ind, is_double? OPSZ_8: OPSZ_4);

            opST0 = opnd_create_reg(DR_REG_ST0);
            //op64 = opnd_create_reg(DR_REG_START_64);

//#ifdef SHOW_RESULTS
            dr_print_opnd(drcontext, STDERR, op, "DST : ");
//#endif
            if(opnd_is_reg(op))
            {
                reg = opnd_get_reg(op);
                if(reg_is_simd(reg)){ 
                    //SIMD scalar
                    translate_insert(INSTR_CREATE_movsd(drcontext, op, opDoF), ilist, instr);
                }else if(reg_is_mmx(reg)){ 
                    //Intel MMX
                    translate_insert(INSTR_CREATE_movq(drcontext, op, opDoF), ilist, instr);
                }else{ 
                    //General purpose register
                    translate_insert(INSTR_CREATE_mov_ld(drcontext, op, opDoF), ilist, instr);
                }
                //TODO complete if necessary
            }else if(opnd_is_immed(op)){ // Immediate value
            
                //dr_printf("immed\n");
                translate_insert(INSTR_CREATE_mov_imm(drcontext, op, opDoF), ilist, instr);
            }else if(opnd_is_memory_reference(op)){
                reg_id_t reserved_reg;
                //dr_printf("memref\n");
                drreg_reserve_register(drcontext, ilist, instr, NULL, &reserved_reg);
                op64 = opnd_create_reg(reserved_reg);
                
                if(opnd_is_rel_addr(op))
                    //dr_printf("reladdr\n");
                if(opnd_is_base_disp(op))
                    //dr_printf("Basdisp\n");
                if(opnd_is_abs_addr(op))
                    //dr_printf("absaddr\n");
                if(opnd_is_pc(op))
                    //dr_printf("pc\n");
                
                translate_insert(INSTR_CREATE_movq(drcontext, op64, opDoF), ilist, instr);
                translate_insert(INSTR_CREATE_movq(drcontext, op, op64), ilist, instr);

                drreg_unreserve_register(drcontext, ilist, instr, reserved_reg);
            }
        }
        instrlist_remove(ilist, instr);
        instr_destroy(drcontext, instr);
    }
}*/

inline void push_for_call(void* drcontext, opnd_t ebp, opnd_t esp, instrlist_t *ilist, instr_t* instr)
{
    opnd_t op12 = opnd_create_immed_int(12, OPSZ_4);
    opnd_t op8 = opnd_create_immed_int(8, OPSZ_4);
    opnd_t op0 = opnd_create_immed_int(0, OPSZ_4);

    /*//dr_printf("first translate\n");
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");*/
    //translate_insert(INSTR_CREATE_push(drcontext, ebp), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("second translate\n");
    //translate_insert(XINST_CREATE_move(drcontext, ebp, esp), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //translate_insert(XINST_CREATE_sub(drcontext, esp, op8), ilist, instr);
    //translate_insert(XINST_CREATE_move(drcontext, opnd_create_reg(DR_REG_RAX), op0), ilist, instr);

    //dr_printf("third translate\n");
    //translate_insert(INSTR_CREATE_push(drcontext, esp), ilist, instr);
    //translate_insert(INSTR_CREATE_push(drcontext, op0), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("fourth translate\n");
    //translate_insert(INSTR_CREATE_push(drcontext, esp), ilist, instr);
    /*translate_insert(XINST_CREATE_sub(drcontext, esp, op8), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("fifth translate\n");*/
    //translate_insert(INSTR_CREATE_movq(drcontext, 
    //    opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_STACK), esp), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("after fifth translate\n");
}

inline void push_for_ret(void* drcontext, opnd_t ebp, opnd_t esp, opnd_t dst, 
    instrlist_t *ilist, instr_t* instr)
{

    //dr_printf("drmgr stuff\n");
    drmgr_insert_read_tls_field(drcontext, tls_index, ilist, instr, DR_REG_RAX);

    //dr_printf("first translate\n");
    translate_insert(INSTR_CREATE_push(drcontext, opnd_create_reg(DR_REG_RAX)), ilist, instr);

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM0),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4)), ilist, instr);

    //dr_printf("third translate\n");
    translate_insert(INSTR_CREATE_pop(drcontext, opnd_create_reg(DR_REG_RAX)), ilist, instr);
}


template<typename FTYPE>
inline int push_xmm(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr);

template<>
inline int push_xmm<double>(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr)
{
    //dr_printf("first translate\n");
    translate_insert(XINST_CREATE_sub(drcontext, esp, 
        opnd_create_immed_int(16, OPSZ_4)), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movsd(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4), src), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("after second translate\n");

    return 8;
}

template<>
inline int push_xmm<float>(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr)
{
    //dr_printf("first translate\n");
    translate_insert(XINST_CREATE_sub(drcontext, esp, 
        opnd_create_immed_int(16, OPSZ_4)), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movss(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4), src), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("after second translate\n");

    return 4;
}

inline void push_call(void *drcontext, void *callee, instrlist_t *ilist, instr_t* instr)
{
    //dr_printf("first translate\n");
    /*movss   xmm0, DWORD PTR [rbp-4]
        addss   xmm0, DWORD PTR [rbp-8]
        mov     rax, QWORD PTR [rbp-16]
        movss   DWORD PTR [rax], xmm0*/
    translate_insert(INSTR_CREATE_call(drcontext, 
        opnd_create_pc((byte*) callee)), ilist, instr);
    //dr_printf("after first translate\n");
}

static void test() {
    //return a + b
    float temp_A;
    float temp_B;

    asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
    //dr_printf("one\n");
    //dr_printf("tempa = %.30f\n", temp_A);

    asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
    //dr_printf("two\n");
    //dr_printf("temp_B = %.30f\n", temp_B);

    //Add the two values we got from xmm
    float temp_C = temp_A + temp_B + 2;
    //dr_printf("three\n");
    //dr_printf("temp_C = %.30f\n", temp_C);

    union{ float f; uint32_t ptr;};

    f = temp_C;

    drmgr_set_tls_field(dr_get_current_drcontext(), tls_index, (void*) ptr);
    
    //dr_printf("pas\n");
}

static void printlol() {
    //dr_printf("on est sortie !\n");
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline bool insert_corresponding_parameters(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(instr_num_srcs(instr) == 2)
    {
        opnd_t ebp = opnd_create_reg(DR_REG_XBP);
        opnd_t esp = opnd_create_reg(DR_REG_XSP);
        //dr_printf("before push_for_call\n");
        //push_for_call(drcontext, ebp, esp, bb, instr);
        //dr_printf("after push_for_call\n");

        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);
        opnd_t dst = instr_get_dst(instr, 0);
        //instr_set_dst(instr, 0, opnd_shrink_to_32_bits(opnd_create_reg(DR_REG_XMM2)));

        if(opnd_is_reg(src1) && opnd_is_reg(dst))
        {
            //dr_printf("before movss to xmm2\n");

            translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM2), 
                src1), bb, instr);

            //dr_printf("after movss to xmm2\n");
            
            //testcontext = drcontext;

            reg_id_t reg_src1 = opnd_get_reg(src1), reg_dst = opnd_get_reg(dst);
            if(opnd_is_reg(src0))
            {
                //dr_printf("reg\n");
                //reg reg -> reg

                translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM7),
                    src0), bb, instr);
                
                //dr_printf("before insert_call\n");
                dr_insert_call(drcontext, bb, instr, (void*)test, 0);
                dr_insert_call(drcontext, bb, instr, (void*)printlol, 0);

                //dr_printf("before push_for_ret\n");
                push_for_ret(drcontext, ebp, esp, dst, bb, instr);
                //dr_printf("after push_for_ret src\n");
                return true;
            }else if(opnd_is_base_disp(src0))
            {
                reg_id_t base = opnd_get_base(src0);
                //dr_printf("base disp\n");
                int flags = DR_MC_MULTIMEDIA;
                if(base == DR_REG_XSP) //Cannot find XIP
                {
                    flags |= DR_MC_CONTROL;
                }else if(reg_is_gpr(base))
                {
                    flags |= DR_MC_INTEGER;
                }
                //long disp = opnd_get_disp(src0);

                translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM7),
                    src0), bb, instr);

                //dr_printf("before insert_call\n");
    //asm volatile("\tmov -16(%%rbp), %%eax\n" : "=m" (tempb));
    //asm volatile("\tmov %%eax, %0\n" : "=m" (tempb));
                //translate_insert(INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_R10),
                //    src0), bb, instr);
                dr_insert_call(drcontext, bb, instr, (void*)test, 0);
                dr_insert_call(drcontext, bb, instr, (void*)printlol, 0);
                //dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_base_disp<FTYPE, FN, SIMD_TYPE>, false, 5, OPND_CREATE_INT32(base),OPND_CREATE_INT64(disp),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(flags));
                //dr_printf("before push_for_ret\n");

                //translate_insert(INSTR_CREATE_movss(drcontext, src1,
                //    opnd_create_reg(DR_REG_EAX)), bb, instr);
                push_for_ret(drcontext, ebp, esp, dst, bb, instr);
                //dr_printf("after push_for_ret src\n");
                //dr_print_instr(drcontext, STDERR, instr, "Found : ");
                return true;

            }else if(opnd_is_rel_addr(src0) || opnd_is_abs_addr(src0))
            {
                //dr_printf("before insert_call\n");
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_addr<FTYPE, FN, SIMD_TYPE>, false, 4, OPND_CREATE_INTPTR(opnd_get_addr(src0)),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(DR_MC_MULTIMEDIA));
                
                //dr_printf("before push_for_ret\n");
                push_for_ret(drcontext, ebp, esp, dst, bb, instr);
                //dr_printf("after push_for_ret src\n");
                return true;
            }
        }

        //dr_printf("before push_for_ret\n");
        push_for_ret(drcontext, ebp, esp, dst, bb, instr);
        //dr_printf("after push_for_ret src\n");
    }
    return false;
}

/*template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
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

        push_for_ret(drcontext, ebp, esp);
    }
    return false;
}*/

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
                //dr_printf("Bad oc\n");
            }
            
            //dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_operation<double> : (void*)interflop_operation<float>, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)) , opnd_create_immed_int(oc & IFP_OP_MASK , OPSZ_4));
            
        }

    }
    return DR_EMIT_DEFAULT;
}

/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "stdint.h"
#include "interflop/interflop_operations.hpp"
#include "interflop/interflop_compute.hpp"
#include "interflop/backend/interflop.h"

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

//static void *testcontext;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,        
                                            bool translating);      

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

}

static void event_exit(void)
{
    drmgr_unregister_tls_field(tls_index);

    drmgr_exit();
    drreg_exit();
}

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

/*static void push_result_to_register(void* drcontext,instrlist_t *ilist, instr_t* instr, 
        bool removeInstr, bool is_double)
{
    if(instr && ilist && drcontext)
    {
        if(!removeInstr)
        {
            instr_t* copy = instr_clone(drcontext, instr);
            instr_set_translation(copy, instr_get_app_pc(instr));
            instr_set_app(copy);
            instrlist_preinsert(ilist,instr, copy);
        }
        

        int num_dst = instr_num_dsts(instr);
        if(num_dst > 0)
        {
            opnd_t op, opDoF, op64, opST0;
            reg_t reg;

            op = instr_get_dst(instr, 0);

            opDoF = opnd_create_rel_addr(*resultBuffer_ind, is_double? OPSZ_8: OPSZ_4);

            opST0 = opnd_create_reg(DR_REG_ST0);
            //op64 = opnd_create_reg(DR_REG_START_64);

//#ifdef SHOW_RESULTS
            dr_print_opnd(drcontext, STDERR, op, "DST : ");
//#endif
            if(opnd_is_reg(op))
            {
                reg = opnd_get_reg(op);
                if(reg_is_simd(reg)){ 
                    //SIMD scalar
                    translate_insert(INSTR_CREATE_movsd(drcontext, op, opDoF), ilist, instr);
                }else if(reg_is_mmx(reg)){ 
                    //Intel MMX
                    translate_insert(INSTR_CREATE_movq(drcontext, op, opDoF), ilist, instr);
                }else{ 
                    //General purpose register
                    translate_insert(INSTR_CREATE_mov_ld(drcontext, op, opDoF), ilist, instr);
                }
                //TODO complete if necessary
            }else if(opnd_is_immed(op)){ // Immediate value
            
                //dr_printf("immed\n");
                translate_insert(INSTR_CREATE_mov_imm(drcontext, op, opDoF), ilist, instr);
            }else if(opnd_is_memory_reference(op)){
                reg_id_t reserved_reg;
                //dr_printf("memref\n");
                drreg_reserve_register(drcontext, ilist, instr, NULL, &reserved_reg);
                op64 = opnd_create_reg(reserved_reg);
                
                if(opnd_is_rel_addr(op))
                    //dr_printf("reladdr\n");
                if(opnd_is_base_disp(op))
                    //dr_printf("Basdisp\n");
                if(opnd_is_abs_addr(op))
                    //dr_printf("absaddr\n");
                if(opnd_is_pc(op))
                    //dr_printf("pc\n");
                
                translate_insert(INSTR_CREATE_movq(drcontext, op64, opDoF), ilist, instr);
                translate_insert(INSTR_CREATE_movq(drcontext, op, op64), ilist, instr);

                drreg_unreserve_register(drcontext, ilist, instr, reserved_reg);
            }
        }
        instrlist_remove(ilist, instr);
        instr_destroy(drcontext, instr);
    }
}*/

inline void push_for_call(void* drcontext, opnd_t ebp, opnd_t esp, instrlist_t *ilist, instr_t* instr)
{
    opnd_t op12 = opnd_create_immed_int(12, OPSZ_4);
    opnd_t op8 = opnd_create_immed_int(8, OPSZ_4);
    opnd_t op0 = opnd_create_immed_int(0, OPSZ_4);

    /*//dr_printf("first translate\n");
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");*/
    //translate_insert(INSTR_CREATE_push(drcontext, ebp), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("second translate\n");
    //translate_insert(XINST_CREATE_move(drcontext, ebp, esp), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //translate_insert(XINST_CREATE_sub(drcontext, esp, op8), ilist, instr);
    //translate_insert(XINST_CREATE_move(drcontext, opnd_create_reg(DR_REG_RAX), op0), ilist, instr);

    //dr_printf("third translate\n");
    //translate_insert(INSTR_CREATE_push(drcontext, esp), ilist, instr);
    //translate_insert(INSTR_CREATE_push(drcontext, op0), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("fourth translate\n");
    //translate_insert(INSTR_CREATE_push(drcontext, esp), ilist, instr);
    /*translate_insert(XINST_CREATE_sub(drcontext, esp, op8), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("fifth translate\n");*/
    //translate_insert(INSTR_CREATE_movq(drcontext, 
    //    opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_STACK), esp), ilist, instr);
    dr_print_opnd(drcontext, STDERR, ebp, "EBP : ");
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("after fifth translate\n");
}

inline void push_for_ret(void* drcontext, opnd_t ebp, opnd_t esp, opnd_t dst, 
    instrlist_t *ilist, instr_t* instr)
{

    //dr_printf("drmgr stuff\n");
    drmgr_insert_read_tls_field(drcontext, tls_index, ilist, instr, DR_REG_RAX);

    //dr_printf("first translate\n");
    translate_insert(INSTR_CREATE_push(drcontext, opnd_create_reg(DR_REG_RAX)), ilist, instr);
    
    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM0),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4)), ilist, instr);
    //dr_printf("third translate\n");
    translate_insert(INSTR_CREATE_pop(drcontext, opnd_create_reg(DR_REG_RAX)), ilist, instr);
}


template<typename FTYPE>
inline int push_xmm(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr);

template<>
inline int push_xmm<double>(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr)
{
    //dr_printf("first translate\n");
    translate_insert(XINST_CREATE_sub(drcontext, esp, 
        opnd_create_immed_int(16, OPSZ_4)), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movsd(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4), src), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("after second translate\n");

    return 8;
}

template<>
inline int push_xmm<float>(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr)
{
    //dr_printf("first translate\n");
    translate_insert(XINST_CREATE_sub(drcontext, esp, 
        opnd_create_immed_int(16, OPSZ_4)), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movss(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4), src), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("after second translate\n");

    return 4;
}

inline void push_call(void *drcontext, void *callee, instrlist_t *ilist, instr_t* instr)
{
    //dr_printf("first translate\n");
    /*movss   xmm0, DWORD PTR [rbp-4]
        addss   xmm0, DWORD PTR [rbp-8]
        mov     rax, QWORD PTR [rbp-16]
        movss   DWORD PTR [rax], xmm0*/
    translate_insert(INSTR_CREATE_call(drcontext, 
        opnd_create_pc((byte*) callee)), ilist, instr);
    //dr_printf("after first translate\n");
}

static void test() {
    //return a + b
    double temp_A;
    double temp_B;

    asm volatile("\tmovsd %%xmm14, %0\n" : "=m" (temp_A));
    //dr_printf("one\n");
    dr_printf("tempa = %.30f\n", temp_A);

    asm volatile("\tmovsd %%xmm15, %0\n" : "=m" (temp_B));
    //dr_printf("two\n");
    dr_printf("temp_B = %.30f\n", temp_B);

    //Add the two values we got from xmm
    double temp_C = temp_A + temp_B + 2;
    //dr_printf("three\n");
    dr_printf("temp_C = %.30f\n", temp_C);

    asm volatile("\tmovsd %0, %%xmm0\n" : : "m" (temp_C));

    /*union{ float f; uint32_t ptr;};

    f = temp_C;

    drmgr_set_tls_field(dr_get_current_drcontext(), tls_index, (void*) ptr);*/
    
    //dr_printf("pas\n");
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline bool insert_corresponding_parameters(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(instr_num_srcs(instr) == 2)
    {
        opnd_t ebp = opnd_create_reg(DR_REG_XBP);
        opnd_t esp = opnd_create_reg(DR_REG_XSP);
        //dr_printf("before push_for_call\n");
        //push_for_call(drcontext, ebp, esp, bb, instr);
        //dr_printf("after push_for_call\n");

        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);
        opnd_t dst = instr_get_dst(instr, 0);
        //instr_set_dst(instr, 0, opnd_shrink_to_32_bits(opnd_create_reg(DR_REG_XMM2)));

        if(opnd_is_reg(src1) && opnd_is_reg(dst))
        {
            //dr_printf("before movss to xmm2\n");

            translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM14), 
                src1), bb, instr);

            //dr_printf("after movss to xmm2\n");
            
            //testcontext = drcontext;

            reg_id_t reg_src1 = opnd_get_reg(src1), reg_dst = opnd_get_reg(dst);
            if(opnd_is_reg(src0))
            {
                dr_printf("reg\n");
                //reg reg -> reg

                translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM15),
                    src0), bb, instr);
                
                //dr_printf("before insert_call\n");
                dr_insert_call(drcontext, bb, instr, (void*)test, 0);
                //dr_insert_call(drcontext, bb, instr, (void*)printlol, 0);

                //dr_printf("before push_for_ret\n");
                //push_for_ret(drcontext, ebp, esp, dst, bb, instr);
                //dr_printf("after push_for_ret src\n");
                return true;
            }else if(opnd_is_base_disp(src0))
            {
                reg_id_t base = opnd_get_base(src0);
                dr_printf("base disp\n");
                int flags = DR_MC_MULTIMEDIA;
                if(base == DR_REG_XSP) //Cannot find XIP
                {
                    flags |= DR_MC_CONTROL;
                }else if(reg_is_gpr(base))
                {
                    flags |= DR_MC_INTEGER;
                }
                //long disp = opnd_get_disp(src0);

                translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM15),
                    src0), bb, instr);

                //dr_printf("before insert_call\n");
    //asm volatile("\tmov -16(%%rbp), %%eax\n" : "=m" (tempb));
    //asm volatile("\tmov %%eax, %0\n" : "=m" (tempb));
                //translate_insert(INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_R10),
                //    src0), bb, instr);
                dr_insert_call(drcontext, bb, instr, (void*)test, 0);
                //dr_insert_call(drcontext, bb, instr, (void*)printlol, 0);
                //dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_base_disp<FTYPE, FN, SIMD_TYPE>, false, 5, OPND_CREATE_INT32(base),OPND_CREATE_INT64(disp),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(flags));
                //dr_printf("before push_for_ret\n");

                //translate_insert(INSTR_CREATE_movss(drcontext, src1,
                //    opnd_create_reg(DR_REG_EAX)), bb, instr);
                //push_for_ret(drcontext, ebp, esp, dst, bb, instr);
                //dr_printf("after push_for_ret src\n");
                //dr_print_instr(drcontext, STDERR, instr, "Found : ");
                return true;

            }else if(opnd_is_rel_addr(src0) || opnd_is_abs_addr(src0))
            {
                //dr_printf("before insert_call\n");
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_addr<FTYPE, FN, SIMD_TYPE>, false, 4, OPND_CREATE_INTPTR(opnd_get_addr(src0)),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(DR_MC_MULTIMEDIA));
                
                //dr_printf("before push_for_ret\n");
                push_for_ret(drcontext, ebp, esp, dst, bb, instr);
                //dr_printf("after push_for_ret src\n");
                return true;
            }
        }

        //dr_printf("before push_for_ret\n");
        push_for_ret(drcontext, ebp, esp, dst, bb, instr);
        //dr_printf("after push_for_ret src\n");
    }
    return false;
}

/*template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
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

        push_for_ret(drcontext, ebp, esp);
    }
    return false;
}*/

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
                //dr_printf("Bad oc\n");
            }
            
            //dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_operation<double> : (void*)interflop_operation<float>, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)) , opnd_create_immed_int(oc & IFP_OP_MASK , OPSZ_4));
            
        }

    }
    return DR_EMIT_DEFAULT;
}

/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "stdint.h"
#include "interflop/interflop_operations.hpp"
#include "interflop/interflop_compute.hpp"
#include "interflop/backend/interflop.h"

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

//static void *testcontext;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,        
                                            bool translating);      

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

}

static void event_exit(void)
{
    drmgr_unregister_tls_field(tls_index);

    drmgr_exit();
    drreg_exit();
}

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

inline void prepare_for_call(void* drcontext, instrlist_t *ilist, instr_t *instr)
{
    translate_insert(XINST_CREATE_sub(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movaps(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16), 
        opnd_create_reg(DR_REG_XMM2)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    translate_insert(XINST_CREATE_sub(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movaps(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16), 
        opnd_create_reg(DR_REG_XMM7)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
}

inline void after_call(void* drcontext, opnd_t dst, instrlist_t *ilist, instr_t* instr)
{

    translate_insert(INSTR_CREATE_movss(drcontext, dst, 
        opnd_create_reg(DR_REG_XMM2)), ilist, instr);

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movaps(drcontext, 
        opnd_create_reg(DR_REG_XMM7),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    translate_insert(XINST_CREATE_add(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movaps(drcontext, 
        opnd_create_reg(DR_REG_XMM2),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    translate_insert(XINST_CREATE_add(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
}

template<typename FTYPE>
inline int push_xmm(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr);

template<>
inline int push_xmm<double>(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr)
{
    //dr_printf("first translate\n");
    translate_insert(XINST_CREATE_sub(drcontext, esp, 
        opnd_create_immed_int(16, OPSZ_4)), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movsd(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4), src), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("after second translate\n");

    return 8;
}

template<>
inline int push_xmm<float>(void *drcontext, opnd_t esp, opnd_t src, instrlist_t *ilist, instr_t* instr)
{
    //dr_printf("first translate\n");
    translate_insert(XINST_CREATE_sub(drcontext, esp, 
        opnd_create_immed_int(16, OPSZ_4)), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movss(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_4), src), ilist, instr);
    dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
    //dr_printf("after second translate\n");

    return 4;
}

template<typename FTYPE>
inline int interface_interflop();

template<>
inline int interface_interflop<double>()
{
    //return a + b
    float temp_A;
    float temp_B;

    asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
    //dr_printf("temp_A = %.30f\n", temp_A);

    asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
    //dr_printf("temp_B = %.30f\n", temp_B);

    //Add the two values we got from xmm
    float temp_C = temp_A + temp_B;
    //dr_printf("three\n");
    //dr_printf("temp_C = %.30f\n", temp_C);

    asm volatile("\tmovss %0, %%xmm2\n" : : "m" (temp_C));
}

static void test()
{
    //return a + b
    float temp_A;
    float temp_B;

    asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
    //dr_printf("one\n");
    //dr_printf("temp_A = %.30f\n", temp_A);

    asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
    //dr_printf("two\n");
    //dr_printf("temp_B = %.30f\n", temp_B);

    //Add the two values we got from xmm
    float temp_C = temp_A + temp_B;
    //dr_printf("three\n");
    //dr_printf("temp_C = %.30f\n", temp_C);

    asm volatile("\tmovss %0, %%xmm2\n" : : "m" (temp_C));

    /*union{ float f; uint32_t ptr;};

    f = temp_C;

    drmgr_set_tls_field(dr_get_current_drcontext(), tls_index, (void*) ptr);*/
    
    //dr_printf("pas\n");
}
static void test()
{
    //return a + b
    float temp_A;
    float temp_B;

    asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
    //dr_printf("one\n");
    //dr_printf("temp_A = %.30f\n", temp_A);

    asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
    //dr_printf("two\n");
    //dr_printf("temp_B = %.30f\n", temp_B);

    //Add the two values we got from xmm
    float temp_C = temp_A + temp_B;
    //dr_printf("three\n");
    //dr_printf("temp_C = %.30f\n", temp_C);

    asm volatile("\tmovss %0, %%xmm2\n" : : "m" (temp_C));

    /*union{ float f; uint32_t ptr;};

    f = temp_C;

    drmgr_set_tls_field(dr_get_current_drcontext(), tls_index, (void*) ptr);*/
    
    //dr_printf("pas\n");
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline bool insert_corresponding_parameters(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(instr_num_srcs(instr) == 2)
    {

        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);
        opnd_t dst = instr_get_dst(instr, 0);

        prepare_for_call(drcontext, bb, instr);

        if(opnd_is_reg(src1) && opnd_is_reg(dst))
        {
            //dr_printf("before movss to xmm2\n");

            translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM2), 
                src1), bb, instr);

            //dr_printf("after movss to xmm2\n");
            

            reg_id_t reg_src1 = opnd_get_reg(src1), reg_dst = opnd_get_reg(dst);
            if(opnd_is_reg(src0))
            {
                //dr_printf("reg\n");

                translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM7),
                    src0), bb, instr);
                
                //dr_printf("before insert_call\n");
                dr_insert_call(drcontext, bb, instr, (void*)test, 0);

            }else if(opnd_is_base_disp(src0))
            {
                reg_id_t base = opnd_get_base(src0);
                //dr_printf("base disp\n");
                int flags = DR_MC_MULTIMEDIA;
                if(base == DR_REG_XSP) //Cannot find XIP
                {
                    flags |= DR_MC_CONTROL;
                }else if(reg_is_gpr(base))
                {
                    flags |= DR_MC_INTEGER;
                }
                //long disp = opnd_get_disp(src0);

                translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM7),
                    src0), bb, instr);

                dr_insert_call(drcontext, bb, instr, (void*)test, 0);

            }else if(opnd_is_rel_addr(src0) || opnd_is_abs_addr(src0))
            {
                //dr_printf("before insert_call\n");
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_addr<FTYPE, FN, SIMD_TYPE>, false, 4, OPND_CREATE_INTPTR(opnd_get_addr(src0)),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(DR_MC_MULTIMEDIA));
                
                //dr_printf("before push_for_ret\n");
                //push_for_ret(drcontext, dst, bb, instr);
                //dr_printf("after push_for_ret src\n");
            }

            after_call(drcontext, dst, bb, instr);

            return true;
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
                //dr_printf("Bad oc\n");
            }
            
            //dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_operation<double> : (void*)interflop_operation<float>, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)) , opnd_create_immed_int(oc & IFP_OP_MASK , OPSZ_4));
            
        }

    }
    return DR_EMIT_DEFAULT;
}

/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "stdint.h"
#include "interflop/interflop_operations.hpp"
#include "interflop/interflop_compute.hpp"
#include "interflop/backend/interflop.h"

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

//static void *testcontext;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,        
                                            bool translating);      

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

}

static void event_exit(void)
{
    drmgr_unregister_tls_field(tls_index);

    drmgr_exit();
    drreg_exit();
}

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

inline void prepare_for_call(void* drcontext, instrlist_t *ilist, instr_t *instr)
{
    translate_insert(XINST_CREATE_sub(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movaps(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16), 
        opnd_create_reg(DR_REG_XMM2)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    translate_insert(XINST_CREATE_sub(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movaps(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16), 
        opnd_create_reg(DR_REG_XMM7)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
}

template<typename FTYPE>
inline void move_back(void* drcontext, opnd_t dst, instrlist_t *ilist, instr_t* instr);

template<>
inline void move_back<double>(void* drcontext, opnd_t dst, instrlist_t *ilist, instr_t* instr)
{
    translate_insert(INSTR_CREATE_movsd(drcontext, dst, 
        opnd_create_reg(DR_REG_XMM2)), ilist, instr);
}

template<>
inline void move_back<float>(void* drcontext, opnd_t dst, instrlist_t *ilist, instr_t* instr)
{
    translate_insert(INSTR_CREATE_movss(drcontext, dst, 
        opnd_create_reg(DR_REG_XMM2)), ilist, instr);
}

template<typename FTYPE>
inline void move_operands(void *drcontext, opnd_t src0, 
    opnd_t src1, instrlist_t *ilist, instr_t* instr);

template<>
inline void move_operands<double>(void *drcontext, opnd_t src0, 
    opnd_t src1, instrlist_t *ilist, instr_t* instr)
{
    translate_insert(INSTR_CREATE_movsd(drcontext, opnd_create_reg(DR_REG_XMM2),
        src0), ilist, instr);

    translate_insert(INSTR_CREATE_movsd(drcontext, opnd_create_reg(DR_REG_XMM7),
        src1), ilist, instr);
}

template<>
inline void move_operands<float>(void *drcontext, opnd_t src0, 
    opnd_t src1, instrlist_t *ilist, instr_t* instr)
{
    translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM2),
        src0), ilist, instr);

    translate_insert(INSTR_CREATE_movss(drcontext, opnd_create_reg(DR_REG_XMM7),
        src1), ilist, instr);
}

inline void after_call(void* drcontext, instrlist_t *ilist, instr_t* instr)
{
    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movaps(drcontext, 
        opnd_create_reg(DR_REG_XMM7),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    translate_insert(XINST_CREATE_add(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_movaps(drcontext, 
        opnd_create_reg(DR_REG_XMM2),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    translate_insert(XINST_CREATE_add(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
}

/*template <typename FTYPE, typename Tfunctor>
struct hh;

template <typename Tfunctor>
struct hh<float, Tfunctor> {

    static void interface_interflop(Tfunctor FN)
    {
        float temp_A;
        float temp_B;

        asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
        dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
        dr_printf("temp_B = %.30f\n", temp_B);

        float temp_C = FN(temp_A, temp_B);
        dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovss %0, %%xmm2\n" : : "m" (temp_C));
    }

};*/


template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
struct machin;

/*
    FAUT FAIRE LE SIMD
*/

template <int SIMD_TYPE, float (*FN)(float, float)>
struct machin<float, FN, SIMD_TYPE> {

    static void interface_interflop()
    {
        dr_printf("out of 0\n");
        float temp_A;
        float temp_B;

        asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
        dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
        dr_printf("temp_B = %.30f\n", temp_B);

        //float temp_C = temp_A + temp_B;

        float temp_C = FN(temp_B, temp_A);

        dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovss %0, %%xmm2\n" : : "m" (temp_C));
    } 

};

template <int SIMD_TYPE, double (*FN)(double, double)>
struct machin<double, FN, SIMD_TYPE> {

    static void interface_interflop()
    {
        dr_printf("out of 0\n");
        float temp_A;
        float temp_B;

        asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
        dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
        dr_printf("temp_B = %.30f\n", temp_B);

        //float temp_C = temp_A + temp_B;

        float temp_C = FN(temp_B, temp_A);

        dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovss %0, %%xmm2\n" : : "m" (temp_C));
    } 

};

template <float (*FN)(float, float)>
struct machin<float, FN, 0> {

    static void interface_interflop()
    {
        //dr_printf("in 0\n");
        float temp_A;
        float temp_B;

        asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
        //dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
        //dr_printf("temp_B = %.30f\n", temp_B);

        float temp_C = temp_A + temp_B;

        //float temp_C = FN(temp_B, temp_A);

        //dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovss %0, %%xmm2\n" : : "m" (temp_C));
    } 

};

template <double (*FN)(double, double)>
struct machin<double, FN, 0> {

    static void interface_interflop()
    {
        //dr_printf("in 0\n");
        double temp_A;
        double temp_B;

        asm volatile("\tmovsd %%xmm2, %0\n" : "=m" (temp_A));
        //dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovsd %%xmm7, %0\n" : "=m" (temp_B));
        //dr_printf("temp_B = %.30f\n", temp_B);

        double temp_C = temp_A + temp_B;

        //double temp_C = FN(temp_B, temp_A);
        //dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovsd %0, %%xmm2\n" : : "m" (temp_C));
    } 

};


template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline void interface_interflop()
{
    machin<FTYPE, FN, SIMD_TYPE>::interface_interflop();
}

static void test()
{
    //return a + b
    double temp_A;
    double temp_B;

    asm volatile("\tmovsd %%xmm2, %0\n" : "=m" (temp_A));
    //dr_printf("one\n");
    dr_printf("temp_A = %.30f\n", temp_A);

    asm volatile("\tmovsd %%xmm7, %0\n" : "=m" (temp_B));
    //dr_printf("two\n");
    dr_printf("temp_B = %.30f\n", temp_B);

    //Add the two values we got from xmm
    double temp_C = temp_A + temp_B + 2;
    //dr_printf("three\n");
    dr_printf("temp_C = %.30f\n", temp_C);

    asm volatile("\tmovsd %0, %%xmm2\n" : : "m" (temp_C));
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
            prepare_for_call(drcontext, bb, instr);

            //dr_printf("before movss to xmm2\n");

            // SFINAE

            move_operands<FTYPE>(drcontext, src0, src1, bb, instr);

            //dr_printf("after movss to xmm2\n");

            dr_insert_call(drcontext, bb, instr, (void*)interface_interflop<FTYPE, FN, SIMD_TYPE>, 0);

            //dr_insert_call(drcontext, bb, instr, (void*)test, 0);

            move_back<FTYPE>(drcontext, dst, bb, instr);

            after_call(drcontext, bb, instr);

            return true;
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
                //dr_printf("Bad oc\n");
            }
            
            //dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_operation<double> : (void*)interflop_operation<float>, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)) , opnd_create_immed_int(oc & IFP_OP_MASK , OPSZ_4));
            
        }

    }
    return DR_EMIT_DEFAULT;
}

/*template <typename FTYPE, typename Tfunctor>
struct hh;

template <typename Tfunctor>
struct hh<float, Tfunctor> {

    static void interface_interflop(Tfunctor FN)
    {
        float temp_A;
        float temp_B;

        asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
        dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
        dr_printf("temp_B = %.30f\n", temp_B);

        float temp_C = FN(temp_A, temp_B);
        dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovss %0, %%xmm2\n" : : "m" (temp_C));
    }

};*/