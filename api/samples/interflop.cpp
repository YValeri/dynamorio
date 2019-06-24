/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"

#include <string.h>

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

instr_t *instrrrr;

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
    drreg_exit();
    drmgr_exit();
    free(*dbuffer_ind);
    free(*resultBuffer_ind);
    dr_printf("ENDDDDDDDDDDDDDD\n");
}


template <typename T>
void get_value_from_opnd(void *drcontext , dr_mcontext_t mcontext , opnd_t src , T *res) {
    if(!opnd_is_null(src)) {
        if(opnd_is_reg(src)) {
           reg_get_value_ex(opnd_get_reg(src) , &mcontext , (byte*)res);
        }
        else if(opnd_is_base_disp(src)) {
            reg_id_t base = opnd_get_base(src);
            int disp = opnd_get_disp(src);
            unsigned long *addr;

            reg_get_value_ex(base , &mcontext , (byte*)&addr);
            memcpy((void*)res , ((byte*)addr)+disp , sizeof(T));
        }
        else if(opnd_is_abs_addr(src)) {
            //TODO
        }
        //TO COMPLETE
    }
}

template <typename T>
void get_all_src(void *drcontext , dr_mcontext_t mcontext , instr_t *instr, T *res, uint nb_src) {
    for(int i = 0 ; i < nb_src ; i++) {
        get_value_from_opnd<T>(drcontext , mcontext , instr_get_src(instr , i) , &res[i]);
    }
}



template <typename T>
static void interflop_add()
{   
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;

    void *drcontext = dr_get_current_drcontext();
    dr_get_mcontext(drcontext , &mcontext);

    T *src = (T*)malloc(2*sizeof(T));

    get_all_src<T>(drcontext , mcontext , instrrrr , src , 2);

    T res = src[0]*src[1];
   
    reg_id_t dst = opnd_get_reg(instr_get_dst(instrrrr,0));
    
    reg_set_value_ex(dst , &mcontext , (byte*)&res);

    dr_set_mcontext(drcontext , &mcontext);

    free(src);
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
            dr_print_instr(drcontext, STDOUT, instr, "Found : ");
            instrrrr = instr;
            //push_instr_to_doublebuffer(drcontext, bb, instr,true);
            dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_add<double>, true, 0);
            instrlist_remove(bb, instr);
            //instr_destroy(drcontext, instr);
            //push_result_to_register(drcontext, bb, instr, true,true);
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
