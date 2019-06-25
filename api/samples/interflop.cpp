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

#include <string.h>

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

}



static void event_exit(void)
{
    drreg_exit();
    drmgr_exit();
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
        else if(opnd_is_abs_addr(src) || opnd_is_rel_addr(src)) {
            memcpy((void*)res , opnd_get_addr(src) , sizeof(T));
        }
        else if(opnd_is_immed(src)) {
            // Possible only with float
            *res = opnd_get_immed_float(src);
        }

    }
}

template <typename T>
void get_all_src(void *drcontext , dr_mcontext_t mcontext , instr_t *instr, T *res) {
    int nb_src = instr_num_srcs(instr);
    for(int i = 0 ; i < nb_src ; i++) {
        get_value_from_opnd<T>(drcontext , mcontext , instr_get_src(instr , i) , &res[i]);
    }
}


template <typename T>
static void interflop_mul()
{
    
}

template <typename T>
static void interflop_div()
{
    
}

template <typename T>
static void interflop_sub()
{

}

template <typename T>
static void interflop_add(app_pc apppc)
{   
    
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    void *drcontext = dr_get_current_drcontext();
    dr_get_mcontext(drcontext , &mcontext);

    instr_t instr;
    instr_init(drcontext, &instr);

    decode(drcontext , apppc , &instr);


    T *src = (T*)malloc(instr_num_srcs(&instr)*sizeof(T));

    get_all_src<T>(drcontext , mcontext , &instr , src);

    T res = src[0]+src[1];
   
    reg_id_t dst = opnd_get_reg(instr_get_dst(&instr,0));
    
    reg_set_value_ex(dst , &mcontext , (byte*)&res);

    dr_set_mcontext(drcontext , &mcontext);

    free(src);
    instr_free(drcontext , &instr);
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
                if(ifp_is_add(oc))
                {
                    dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_add<double> : (void*)interflop_add<float>, false, 1, OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
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
                
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
            }
        }

    }
    return DR_EMIT_DEFAULT;
}
