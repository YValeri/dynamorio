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

#define TYPE double

int tls_index;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,         //TODO
                                            bool translating);      //TODO

static void thread_init(void *drcontext);

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

    interflop_verrou_configure(VR_RANDOM , nullptr);

    
    tls_index = drmgr_register_tls_field();
    

    drmgr_register_thread_init_event(thread_init);

    drreg_options_t drreg_options;
    drreg_options.conservative = true;
    drreg_options.num_spill_slots = 1;
    drreg_options.struct_size = sizeof(drreg_options_t);
    drreg_options.do_not_sum_slots=false;
    drreg_options.error_callback=NULL;
    drreg_init(&drreg_options);

}

static void event_exit(void)
{
    drmgr_exit();
    drreg_exit();
}

static void thread_init(void *dr_context) {
    drmgr_set_tls_field(dr_context , tls_index , 0);
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

void emptyfunc() {
    //byte buf[DR_FPSTATE_BUF_SIZE];
    //proc_save_fpstate(buf);
    //dr_printf("EMPTY FUNC !!!!\n");
    //proc_restore_fpstate(buf);
}

template <typename T>
void interflop_backend(T a,  T b /*, T *resssss*/ , void *context);


template <>
void interflop_backend<double>(double a,  double b /*, double *resssss*/ , void *context)
{
    union {void* aaaaaa; double aa;};
    interflop_verrou_add_double(a , b, &aa , context);
    drmgr_set_tls_field(dr_get_current_drcontext() , tls_index , aaaaaa);
}

template <>
void interflop_backend<float>(float a,  float b /*, float *resssss */, void *context)
{
    union {unsigned int aaaaaa; float aa;};
    interflop_verrou_add_float(a , b, &aa , context);
    drmgr_set_tls_field(dr_get_current_drcontext() , tls_index , (void*)aaaaaa);
}


/*
static void before() {
    dr_printf("Before !!!!\n");
    
    void *context = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(context , &mcontext);

    reg_get_value_ex(DR_REG_XDI , &mcontext , rdi);
    reg_get_value_ex(DR_REG_XSI , &mcontext , rsi);
    reg_get_value_ex(DR_REG_XSP , &mcontext , rsp);
    reg_get_value_ex(DR_REG_XBP , &mcontext , rbp);
    

    dr_printf("RDI : ");
    for(int i = 0 ; i < 8 ; i++) dr_printf("%02X ",rdi[i]);
    dr_printf("\nRSI : ");
    for(int i = 0 ; i < 8 ; i++) dr_printf("%02X ",rsi[i]);
    dr_printf("\nRSP : ");
    for(int i = 0 ; i < 8 ; i++) dr_printf("%02X ",rsp[i]);
    dr_printf("\nRBP : ");
    for(int i = 0 ; i < 8 ; i++) dr_printf("%02X ",rbp[i]);
    dr_printf("\n\n");


}

static void after() {
     dr_printf("After !!!!\n");
     
     void *context = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(context , &mcontext);

    reg_get_value_ex(DR_REG_XDI , &mcontext , rdi);
    reg_get_value_ex(DR_REG_XSI , &mcontext , rsi);
    reg_get_value_ex(DR_REG_XSP , &mcontext , rsp);
    reg_get_value_ex(DR_REG_XBP , &mcontext , rbp);
   

    dr_printf("RDI : ");
    for(int i = 0 ; i < 8 ; i++) dr_printf("%02X ",rdi[i]);
    dr_printf("\nRSI : ");
    for(int i = 0 ; i < 8 ; i++) dr_printf("%02X ",rsi[i]);
    dr_printf("\nRSP : ");
    for(int i = 0 ; i < 8 ; i++) dr_printf("%02X ",rsp[i]);
    dr_printf("\nRBP : ");
    for(int i = 0 ; i < 8 ; i++) dr_printf("%02X ",rbp[i]);
    dr_printf("RESSSSSSSSSSSSs : %f\n",*res);
    dr_printf("\n\n");
}
*/

static void print() {
    dr_printf("Wait that's fucking illegal !!!!\n");
}


static dr_emit_flags_t event_basic_block(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    instr_t *instr2 , *next_instr2;
    OPERATION_CATEGORY oc;

    bool display = true;
    

    for(instr = instrlist_first(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next(instr);

        oc = ifp_get_operation_category(instr);

        if(oc)
        {

            int size = (ifp_is_double(oc) ? 8 : 4);
            bool is_double = ifp_is_double(oc);
            
            //dr_print_instr(drcontext, STDERR, INSTR_CREATE_push(drcontext , opnd_create_reg(DR_REG_EBP)) , "Push : ");
            //dr_print_instr(drcontext, STDERR, instr , "II : ");
            
            /*
            if(display) {
                dr_printf("\n");
                for(instr2 = instrlist_first(bb); instr2 != NULL; instr2 = next_instr2) {
                    next_instr2 = instr_get_next(instr2);
                    dr_print_instr(drcontext , STDERR , instr2 , "INSTR : ");
                    
                    //dr_print_opnd(drcontext , STDERR , instr_get_src(instr2,0) , "SRC 0 : ");
                    //if(instr_num_srcs(instr2) > 1) dr_print_opnd(drcontext , STDERR , instr_get_src(instr2,1) , "SRC 1 : ");
                    //if(instr_num_dsts(instr2) > 0) dr_print_opnd(drcontext , STDERR , instr_get_dst(instr2,0) , "DST : ");
                    //dr_printf("\n");
                }
            }
            */
            // ****************************************************************************
            // PRINT BEFORE 
            // ****************************************************************************            
            //dr_insert_clean_call(drcontext , bb , instr , (void*)before , false , 0);
            // ****************************************************************************
 
            // ****************************************************************************
            // save processor flags on stack
            // ****************************************************************************
            translate_insert(INSTR_CREATE_pushf(drcontext) , bb , instr);
            
            // ****************************************************************************
            // save rdi on stack
            // ****************************************************************************
            translate_insert(INSTR_CREATE_push(drcontext , opnd_create_reg(DR_REG_XDI)) , bb , instr);

            // ****************************************************************************
            // save rsi on stack
            // ****************************************************************************
            translate_insert(INSTR_CREATE_push(drcontext , opnd_create_reg(DR_REG_XSI)) , bb , instr);  

            // ****************************************************************************
            // Expand stack
            // ****************************************************************************
            translate_insert(INSTR_CREATE_sub(drcontext , opnd_create_reg(DR_REG_XSP) , opnd_create_immed_int(2*size , OPSZ_4)) , bb , instr);


            // ****************************************************************************
            // push function arguments in reverse order
            // ****************************************************************************

            // ***** context in %rsi *****
            translate_insert(INSTR_CREATE_movq(drcontext , opnd_create_reg(DR_REG_XSI) , opnd_create_rel_addr(drcontext,OPSZ_8)) , bb , instr);

            // ***** result address in $rdi *****
            //translate_insert(INSTR_CREATE_movq(drcontext , opnd_create_reg(DR_REG_XDI) , opnd_create_rel_addr((void*)res,OPSZ_8)) , bb , instr);

            // ***** second operand on stack *****

            if(opnd_is_reg(instr_get_src(instr,0)))
                translate_insert(is_double ? INSTR_CREATE_movsd(drcontext , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_8) , instr_get_src(instr,0)) : INSTR_CREATE_movss(drcontext , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_4),instr_get_src(instr,0)) , bb , instr);
            
            else if(opnd_is_rel_addr(instr_get_src(instr,0))) {
                
                // ****************************************************************************
                // Reserve register as intermediate
                // ****************************************************************************
                reg_id_t reserved;
                drreg_reserve_register(drcontext, bb, instr, NULL, &reserved);
                
                // ****************************************************************************
                // copy operand value from memory to the reserved register
                // ****************************************************************************
                translate_insert(is_double ? INSTR_CREATE_movsd(drcontext , opnd_create_reg(reserved),instr_get_src(instr,0)) : INSTR_CREATE_movss(drcontext , opnd_create_reg(reserved),instr_get_src(instr,0)) , bb , instr);
                
                // ****************************************************************************
                // copy operand from the reserved register at the top of the stack
                // ****************************************************************************
                translate_insert(is_double ? INSTR_CREATE_movsd(drcontext , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_8), opnd_create_reg(reserved)) : INSTR_CREATE_movss(drcontext , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_4), opnd_create_reg(reserved)) , bb , instr);

                // ****************************************************************************
                // Unreserve the regiter
                // ****************************************************************************
                drreg_unreserve_register(drcontext , bb , instr , reserved);
                
            }
        
            // ***** first operand on stack *****
            if(opnd_is_reg(instr_get_src(instr,1)))
                translate_insert(is_double ? INSTR_CREATE_movsd(drcontext , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_8) , instr_get_src(instr,1)) : INSTR_CREATE_movss(drcontext , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_4),instr_get_src(instr,1)) , bb , instr);
            
            else if(opnd_is_rel_addr(instr_get_src(instr,1))) {
                
                // ****************************************************************************
                // Reserve register as intermediate
                // ****************************************************************************
                reg_id_t reserved;
                drreg_reserve_register(drcontext, bb, instr, NULL, &reserved);
                
                // ****************************************************************************
                // copy operand value from memory to the reserved register
                // ****************************************************************************
                translate_insert(is_double ? INSTR_CREATE_movsd(drcontext , opnd_create_reg(reserved),instr_get_src(instr,1)) : INSTR_CREATE_movss(drcontext , opnd_create_reg(reserved),instr_get_src(instr,1)) , bb , instr);
                
                // ****************************************************************************
                // copy operand from the reserved register at the top of the stack
                // ****************************************************************************
                translate_insert(is_double ? INSTR_CREATE_movsd(drcontext , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_8), opnd_create_reg(reserved)) : INSTR_CREATE_movss(drcontext , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_4), opnd_create_reg(reserved)) , bb , instr);

                // ****************************************************************************
                // Unreserve the regiter
                // ****************************************************************************
                drreg_unreserve_register(drcontext , bb , instr , reserved);
                
            }

            // ****************************************************************************
            // CALL
            // ****************************************************************************
            dr_insert_call(drcontext , bb , instr ,is_double ?  (void*)interflop_backend<double> : (void*)interflop_backend<float> ,  0);
            // ****************************************************************************
            // ****************************************************************************

            // ****************************************************************************
            // Remove arguments from stack
            // ****************************************************************************
            //translate_insert(INSTR_CREATE_movq(drcontext , opnd_create_reg(DR_REG_XSP) , opnd_create_reg(DR_REG_XBP)) , bb , instr);
            translate_insert(INSTR_CREATE_add(drcontext , opnd_create_reg(DR_REG_XSP) , opnd_create_immed_int(2*size , OPSZ_4)) , bb , instr);


            // ****************************************************************************
            // Set the result in the corresponding register
            // ****************************************************************************
            //translate_insert(is_double ? INSTR_CREATE_movsd(drcontext , instr_get_dst(instr , 0) , opnd_create_rel_addr(res , OPSZ_8)) :  INSTR_CREATE_movss(drcontext , instr_get_dst(instr , 0) , opnd_create_rel_addr(res , OPSZ_4)), bb , instr);
        
            drmgr_insert_read_tls_field(drcontext , tls_index , bb , instr , DR_REG_XDI);
            translate_insert(INSTR_CREATE_push(drcontext , opnd_create_reg(DR_REG_XDI)) , bb , instr);
            translate_insert(is_double ? INSTR_CREATE_movsd(drcontext , instr_get_dst(instr,0) , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_8)) : INSTR_CREATE_movss(drcontext , instr_get_dst(instr,0) , opnd_create_base_disp(DR_REG_XSP , DR_REG_NULL , 0 , 0 , OPSZ_4)), bb , instr);
            translate_insert(INSTR_CREATE_add(drcontext , opnd_create_reg(DR_REG_XSP) , opnd_create_immed_int(8 , OPSZ_4)) , bb , instr);


            // ****************************************************************************
            // Pop saved $rsi
            // ****************************************************************************
            translate_insert(INSTR_CREATE_pop(drcontext , opnd_create_reg(DR_REG_XSI)), bb , instr);

            // ****************************************************************************
            // Pop saved $rdi
            // ****************************************************************************
            translate_insert(INSTR_CREATE_pop(drcontext , opnd_create_reg(DR_REG_XDI)), bb , instr);
            

            // ****************************************************************************
            // Restore processor flags
            // ****************************************************************************
            translate_insert(INSTR_CREATE_popf(drcontext) , bb , instr);

            // ****************************************************************************
            // PRINT AFTER 
            // ****************************************************************************
            //dr_insert_clean_call(drcontext , bb , instr , (void*)after , false , 0);

            // ****************************************************************************
            
            // ****************************************************************************
            // Remove original instruction
            // ****************************************************************************
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
            
            /*
            if(display) {
                //display = false;
                dr_printf("\n");
                for(instr2 = instrlist_first(bb); instr2 != NULL; instr2 = next_instr2) {
                    next_instr2 = instr_get_next(instr2);
                    dr_print_instr(drcontext , STDERR , instr2 , "INSTR 2 : ");
                }
            }
            */
            

            /* 
            if(insert_corresponding_call(drcontext, bb, instr, oc))
            {
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
            }else
            {
                dr_printf("Bad oc\n");
            }
            */
            //dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_operation<double> : (void*)interflop_operation<float>, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)) , opnd_create_immed_int(oc & IFP_OP_MASK , OPSZ_4));
            
        }

    }
/*
    for(instr = instrlist_first(bb); instr != NULL; instr = next_instr)
    {   
        next_instr = instr_get_next(instr);
        if(instr_get_opcode(instr) == OP_ret) {
           dr_printf("\n");
            for(instr2 = instrlist_first(bb); instr2 != NULL; instr2 = next_instr2) {
                next_instr2 = instr_get_next(instr2);
                dr_print_instr(drcontext , STDERR , instr2 , "INSTR 3 : ");
            } 
        }
    }
  */
    return DR_EMIT_DEFAULT;
}
