/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"

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

// Buffer to contain double precision flaoting result to be copied back into a register 
static double *resultBuffer;
static double **resultBuffer_ind;

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
    // Memory allocation for global variables
    dbuffer = (double*)malloc(INTERFLOP_BUFFER_SIZE);
    dbuffer_ind = &dbuffer;
    
    resultBuffer = (double*)malloc(64);
    resultBuffer_ind = &resultBuffer;

    // Init DynamoRIO MGR extension ()
    drmgr_init();

    drreg_options_t drreg_options;
    drreg_options.conservative = true;
    drreg_options.num_spill_slots = 1;
    drreg_options.struct_size = sizeof(drreg_options_t);
    drreg_options.do_not_sum_slots=false;
    drreg_options.error_callback=NULL;
    drreg_init(&drreg_options);
    
    // Define the functions to be called before exiting this client program
    dr_register_exit_event(event_exit);

    // Define the function to executed to treat each instructions block
    drmgr_register_bb_app2app_event(event_basic_block, NULL);

}


static void event_exit(void)
{
    drmgr_exit();
    free(*dbuffer_ind);
    free(*resultBuffer_ind);
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
}


static void interflop_add()
{
    dr_printf("buffer : %lf\t%lf\n",**dbuffer_ind, *(*dbuffer_ind+1));
    **resultBuffer_ind = **dbuffer_ind+*(*dbuffer_ind+1);
    dr_printf("res : %lf\n",**resultBuffer_ind);
}

static void push_instr_to_doublebuffer(void *drcontext, instrlist_t *ilist, 
            instr_t* instr, bool is_double)
{
    if(instr && ilist && drcontext)
    {
        int num_src = instr_num_srcs(instr);
        int i=0;
        opnd_t op, opDoF, op64;
        for(; i<num_src; i++)
        {

            op = instr_get_src(instr, i);

           opDoF = opnd_create_rel_addr(*dbuffer_ind+i, is_double? OPSZ_8: OPSZ_4);

            dr_print_opnd(drcontext, STDERR, op, "\nOP :");
            if(opnd_is_reg(op)) // Register
            {
                reg_t reg = opnd_get_reg(op);
                if(reg_is_simd(reg))
                { 
                    // SIMD scalar
                    translate_insert(INSTR_CREATE_movsd(drcontext,opDoF, op), ilist, instr);
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
                dr_printf("memref\n");
                reg_id_t reserved_reg;
                drreg_reserve_register(drcontext, ilist, instr, NULL, &reserved_reg);
                dr_printf("%s\n", get_register_name(reserved_reg));
                op64 = opnd_create_reg(reserved_reg);
                if(opnd_is_rel_addr(op))
                    dr_printf("reladdr\n");
                if(opnd_is_base_disp(op))
                    dr_printf("Basdisp\n");
                if(opnd_is_abs_addr(op))
                    dr_printf("absaddr\n");
                if(opnd_is_pc(op))
                    dr_printf("pc\n");
                //dr_printf("base_disp");
                //This case needs special care because it's a memory address not accessible directly
                //We can't mov from adress to adress so we'll copy the content in a register, 
                //then copy the register to memory

                //translate_insert(INSTR_CREATE_movq(drcontext, op64, ), ilist, instr);

                translate_insert(INSTR_CREATE_movq(drcontext, op64, op), ilist, instr);
                translate_insert(INSTR_CREATE_movq(drcontext, opDoF, op64), ilist, instr);

                //translate_insert(INSTR_CREATE_pop(drcontext, op64), ilist, instr);
                drreg_unreserve_register(drcontext, ilist, instr, reserved_reg);
            }
        }
        
    }
}


static dr_emit_flags_t event_basic_block(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    for(instr = instrlist_first(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next(instr);
        if(instr_get_opcode(instr) == OP_fadd || instr_get_opcode(instr)==OP_faddp
            || instr_get_opcode(instr)==OP_addsd)
        {
            dr_print_instr(drcontext, STDERR, instr, "Found : ");
            push_instr_to_doublebuffer(drcontext, bb, instr,true);
            dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_add, false, 0);
            push_result_to_register(drcontext, bb, instr, true,true);
        }

    }
    
    return DR_EMIT_DEFAULT;
}