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
//#include "interflop/backend/interflop.h"

//Define the display function
#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg)
#endif

#ifndef MAX_INSTR_OPND_COUNT

// Generally floating operations needs three operands (2src + 1 dst) and FMA needs 4 (3src + 1dst)
#define MAX_INSTR_OPND_COUNT 4
#endif

#ifndef MAX_OPND_SIZE_BYTES

// operand size is maximum 512 bits (AVX512 instructions) = 64 bytes 
#define MAX_OPND_SIZE_BYTES 64 
#endif


#define INT2OPND(x) (opnd_create_immed_int((x), OPSZ_PTR))

#define INTERFLOP_BUFFER_SIZE (MAX_INSTR_OPND_COUNT*MAX_OPND_SIZE_BYTES)


static void event_exit(void);


    // Global variables 

// Buffer to contain double precision floating operands copied from registers  
static double *dbuffer;
static double **dbuffer_ind;

// Buffer to contain double precision floating result to be copied back into a register 
static double *resultBuffer;
static double **resultBuffer_ind;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,         //TODO
                                            bool translating);      //TODO

static dr_emit_flags_t runtime(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, void **user_data);

// Main function to setup the dynamoRIO client
DR_EXPORT void dr_client_main(  client_id_t id, // client ID
                                int argc,   
                                const char *argv[])
{
    // Memory allocation for global variables
    dbuffer = (double*)malloc(INTERFLOP_BUFFER_SIZE);
    dbuffer_ind = &dbuffer;
    
    resultBuffer = (double*)malloc(64);
    resultBuffer_ind = &resultBuffer;

    dr_printf("\ndbuffer_ind : %p\n&dbuffer_ind : %p\n", dbuffer_ind , &dbuffer_ind);
    dr_printf("dbuffer : %p\n&dbuffer : %p\n\n", dbuffer , &dbuffer);

    dr_printf("resultBuffer_ind : %p\n&resultBuffer_ind : %p\n", resultBuffer_ind , &resultBuffer_ind);
    dr_printf("resultBuffer : %p\n&resultBuffer : %p\n\n", resultBuffer , &resultBuffer);
    // Init DynamoRIO MGR extension ()
    drmgr_init();

    drreg_options_t drreg_options;
    drreg_options.conservative = true;
    drreg_options.num_spill_slots = 2;
    drreg_options.struct_size = sizeof(drreg_options_t);
    drreg_options.do_not_sum_slots=false;
    drreg_options.error_callback=NULL;
    drreg_init(&drreg_options);
    
    // Define the functions to be called before exiting this client program
    dr_register_exit_event(event_exit);

    // Define the function to executed to treat each instructions block
    drmgr_register_bb_app2app_event(event_basic_block, NULL);
    drmgr_register_bb_instrumentation_event(runtime,NULL,NULL);

}



static void event_exit(void)
{
    //drreg_exit();
    drmgr_exit();
    drreg_exit();
    free(*dbuffer_ind);
    free(*resultBuffer_ind);
    //dr_printf("ENDDDDDDDDDDDDDD\n");
}


/**
 * Insert a new instruction to be executed by the application
 * newinstr : the new instruction
 * ilist : the instructions of the current block
 * instr : the reference instruction in the list -> newinstr is inserted just before instr
 */
static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

static void push_result_to_register(void* drcontext,instrlist_t *ilist, instr_t* instr, 
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
        
        reg_id_t reserved_reg;
        int num_dst = instr_num_dsts(instr);
        if(num_dst > 0)
        {
            opnd_t op, opDoF, op64, op_result_addr;
            reg_t reg;

            op = instr_get_dst(instr, 0);
            op_result_addr = opnd_create_rel_addr(*resultBuffer_ind , OPSZ_PTR);

            drreg_reserve_register(drcontext, ilist, instr, NULL, &reserved_reg);
            op64 = opnd_create_reg(reserved_reg);

            translate_insert(INSTR_CREATE_movq(drcontext, op64 , op_result_addr),ilist , instr);

            //opDoF = opnd_create_rel_addr(*resultBuffer_ind, is_double? OPSZ_8: OPSZ_4);
            opDoF = opnd_create_base_disp(opnd_get_reg(op64) , DR_REG_NULL , 0 , 0 , is_double ? OPSZ_8 : OPSZ_4);


//#ifdef SHOW_RESULTS
            dr_print_opnd(drcontext, STDERR, op, "DST : ");
            dr_print_opnd(drcontext, STDERR, opDoF, "RESULT : ");
//#endif
            if(opnd_is_reg(op))
            {
                reg = opnd_get_reg(op);
                if(reg_is_simd(reg)){ 
                    //SIMD scalar
                    dr_printf("SIMD scalar\n");
                    translate_insert(INSTR_CREATE_movsd(drcontext, op, opDoF), ilist, instr);
                    dr_printf("SIMD scalar after\n");
                }else if(reg_is_mmx(reg)){ 
                    //Intel MMX
                    dr_printf("MMX\n");
                    translate_insert(INSTR_CREATE_movq(drcontext, op, opDoF), ilist, instr);
                }else{ 
                    //General purpose register
                    dr_printf("GPR\n");
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

                
            }
            drreg_unreserve_register(drcontext, ilist, instr, reserved_reg);
        }
        
        instrlist_remove(ilist, instr);
        instr_destroy(drcontext, instr);
    }
}

template <typename T>
static void interflop_mul()
{
    dr_printf("buffer : %lf\t%lf\n",**dbuffer_ind, *(*dbuffer_ind+1));
    ifp_compute_mul((T*)*dbuffer_ind, (T*)*resultBuffer_ind);
    dr_printf("res : %lf\n",**resultBuffer_ind);
}

template <typename T>
static void interflop_div()
{
    dr_printf("buffer : %lf\t%lf\n",**dbuffer_ind, *(*dbuffer_ind+1));
    ifp_compute_div((T*)*dbuffer_ind, (T*)*resultBuffer_ind);
    dr_printf("res : %lf\n",**resultBuffer_ind);
}

template <typename T>
static void interflop_sub()
{
    dr_printf("buffer : %lf\t%lf\n",**dbuffer_ind, *(*dbuffer_ind+1));
    ifp_compute_sub((T*)*dbuffer_ind, (T*)*resultBuffer_ind);
    dr_printf("res : %lf\n",**resultBuffer_ind);
}

template <typename T>
static void interflop_add()
{
    dr_printf("buffer : %lf\t%lf\n",**dbuffer_ind, *(*dbuffer_ind+1));
    ifp_compute_add((T*)*dbuffer_ind, (T*)*resultBuffer_ind);
    dr_printf("res : %lf\n",**resultBuffer_ind);
}

static void push_instr_to_doublebuffer(void *drcontext, instrlist_t *ilist, 
            instr_t* instr, bool is_double)
{
    if(instr && ilist && drcontext)
    {
        int num_src = instr_num_srcs(instr);
        
        opnd_t op, opDoF, op64, op_dbuffer_addr,op_dbuffer/*,opnd_temp*/;
        for(int i=0; i<num_src; i++) {
            op = instr_get_src(instr, i);
            dr_print_opnd(drcontext , STDOUT , op , "OP : ");

            op_dbuffer_addr = opnd_create_rel_addr(*dbuffer_ind , OPSZ_PTR);
            //dr_print_opnd(drcontext , STDOUT , op_dbuffer_addr , "OPND ADDRESS BUFFER : ");

            reg_id_t reserved_reg;
            drreg_reserve_register(drcontext, ilist, instr, NULL, &reserved_reg);
            op64 = opnd_create_reg(reserved_reg);

            // Move buffer address in reserved register
            translate_insert(INSTR_CREATE_movq(drcontext, op64 , op_dbuffer_addr),ilist , instr);

            dr_print_opnd(drcontext , STDOUT , op64 , "OPND OP64 : ");
            
            /*
            opnd_temp = opnd_create_rel_addr(&buffer_address_reg , OPSZ_PTR);
            translate_insert(INSTR_CREATE_movq(drcontext,opnd_temp, op64), ilist, instr);
            */

            opDoF = opnd_create_base_disp(opnd_get_reg(op64) , DR_REG_NULL , 0 , i*(is_double ? sizeof(double) : sizeof(float)) , is_double ? OPSZ_8 : OPSZ_4);

            dr_print_opnd(drcontext, STDOUT, op, "\nOP :");
            if(opnd_is_reg(op)) // Register
            {
                reg_t reg = opnd_get_reg(op);
                if(reg_is_simd(reg))
                { 
                    // SIMD scalar 
                    //translate_insert(INSTR_CREATE_movsd(drcontext,opDoF, op), ilist, instr);
                    translate_insert(INSTR_CREATE_movsd(drcontext,opDoF, op), ilist, instr);
                    dr_print_opnd(drcontext , STDOUT , opDoF , "OPDOF : ");
                    
                } else if(reg_is_mmx(reg)) {
                    // Intel MMX
                    translate_insert(INSTR_CREATE_movq(drcontext, opDoF, op), ilist, instr);
                } else { 
                    // General purpose register
                    translate_insert(INSTR_CREATE_movq(drcontext, opDoF, op), ilist, instr);
                }
                //TODO complete if necessary

            } else if(opnd_is_immed(op)) { // Immediate value
                translate_insert(INSTR_CREATE_mov_imm(drcontext, opDoF, op), ilist, instr);
            }else if(opnd_is_memory_reference(op)){
                //dr_printf("memref\n");
                //dr_printf("%s\n", get_register_name(reserved_reg));
                
                reg_id_t temp_reg;
                drreg_reserve_register(drcontext, ilist, instr, NULL, &temp_reg);
                opnd_t op_temp_reg = opnd_create_reg(temp_reg);
                dr_print_opnd(drcontext , STDOUT , op_temp_reg , "TEMP_REG : ");

                /*
                if(opnd_is_rel_addr(op))
                    dr_printf("reladdr\n");
                if(opnd_is_base_disp(op))
                    dr_printf("Basdisp\n");
                if(opnd_is_abs_addr(op))
                    dr_printf("absaddr\n");
                if(opnd_is_pc(op))
                    dr_printf("pc\n");
                */

                //dr_printf("base_disp");
                //This case needs special care because it's a memory address not accessible directly
                //We can't mov from adress to adress so we'll copy the content in a register, 
                //then copy the register to memory

                //translate_insert(INSTR_CREATE_movq(drcontext, op64, ), ilist, instr);
                dr_print_opnd(drcontext , STDOUT , opDoF , "OPDOF : ");
                translate_insert(INSTR_CREATE_movq(drcontext, op_temp_reg, op), ilist, instr);
                translate_insert(INSTR_CREATE_movq(drcontext, opDoF, op_temp_reg), ilist, instr);

                //translate_insert(INSTR_CREATE_pop(drcontext, op64), ilist, instr);
                drreg_unreserve_register(drcontext, ilist, instr, temp_reg);
                
            }
            drreg_unreserve_register(drcontext, ilist, instr, reserved_reg);
        }
        
    }
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
            if(ifp_is_scalar(oc))
            {
                push_instr_to_doublebuffer(drcontext, bb, instr,ifp_is_double(oc));
                if(ifp_is_add(oc))
                {
                    dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_add<double> : (void*)interflop_add<float>, false, 0);
                }else if(ifp_is_sub(oc))
                {
                    dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_sub<double> : (void*)interflop_sub<float>, false, 0);
                }else if(ifp_is_mul(oc))
                {
                    dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_mul<double> : (void*)interflop_mul<float>, false, 0);
                }else if(ifp_is_div(oc))
                {
                    dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_div<double> : (void*)interflop_div<float>, false, 0);
                }
                
                push_result_to_register(drcontext, bb, instr, true,ifp_is_double(oc));
            }
        }

    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t runtime(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, void **user_data) {
    instr_t *instr, *next_instr;
    for(instr = instrlist_first(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next(instr);
        //dr_printf("BUFFER ADDRESS IN REGISTER : %p\tREAL BUFFER ADDRESS : %p\n",buffer_address_reg,*dbuffer_ind);
        //dr_print_instr(drcontext, STDERR, instr, "RUNTIME Found : ");

    }
    return DR_EMIT_DEFAULT;
}
