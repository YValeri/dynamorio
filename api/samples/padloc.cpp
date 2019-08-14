/**
 * \file
 * \brief DynamoRIO client developped in the scope of INTERFLOP project 
 * 
 * \author Brasseur Dylan, Teaudors Mickaël, Valeri Yoann
 * \date 2019
 * \copyright Interflop 
 */

#include "dr_api.h"
#include "../include/dr_ir_opcodes.h"
#include "../include/dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "drsyms.h"
#include "padloc/padloc_operations.hpp"
#include <vector>
#include <string>

#include "padloc/symbol_config.hpp"
#include "padloc/padloc_client.h"
#include "padloc/analyse.hpp"
#include "padloc/utils.hpp"

static void event_exit(void);

static void thread_init(void *drcontext);

static void thread_exit(void *drcontext);

//Function to treat each block of instructions to instrument
static dr_emit_flags_t app2app_bb_event(void *drcontext,        //Context
                                        void *tag,              // Unique identifier of the block
                                        instrlist_t *bb,        // Linked list of the instructions
                                        bool for_trace,         //TODO
                                        bool translating);      //TODO

//Function to treat each block of instructions to get the symbols
static dr_emit_flags_t symbol_lookup_event(void *drcontext,        //Context
                                           void *tag,              // Unique identifier of the block
                                           instrlist_t *bb,        // Linked list of the instructions
                                           bool for_trace,         //TODO
                                           bool translating,       //TODO
                                           OUT void **user_data);

#define INSERT_MSG(dc, bb, where, x) dr_insert_clean_call(dc,bb, where, (void*)printMessage,true, 1, opnd_create_immed_int(x, OPSZ_4))

static void module_load_handler(void *drcontext, const module_data_t *module,
                                bool loaded){
    dr_module_set_should_instrument(module->handle,
                                    should_instrument_module(module));
}

static void api_initialisation(){
    drsym_init(0);

    // Init DynamoRIO MGR extension
    drmgr_init();

    //Configure verrou in random rounding mode
    Interflop::verrou_prepare();

    drreg_options_t drreg_options;
    drreg_options.conservative = false;
    drreg_options.num_spill_slots = 1;
    drreg_options.struct_size = sizeof(drreg_options_t);
    drreg_options.do_not_sum_slots = true;
    drreg_options.error_callback = NULL;
    drreg_init(&drreg_options);
}

static void api_register(){
    // Define the functions to be called before exiting this client program
    dr_register_exit_event(event_exit);

    drmgr_register_thread_init_event(thread_init);
    drmgr_register_thread_exit_event(thread_exit);

    if(get_client_mode() == PLC_CLIENT_GENERATE){
        drmgr_register_bb_instrumentation_event(symbol_lookup_event, NULL,
                                                NULL);
    }else{
        drmgr_register_module_load_event(module_load_handler);
        drmgr_register_bb_app2app_event(app2app_bb_event, NULL);
    }
}

static void tls_register(){
    set_index_tls_result(drmgr_register_tls_field());
    set_index_tls_float(drmgr_register_tls_field());
    set_index_tls_gpr(drmgr_register_tls_field());
}

// Main function to setup the dynamoRIO client
DR_EXPORT void dr_client_main(client_id_t id, // client ID
                              int argc,
                              const char *argv[]){
    api_initialisation();

    if(arguments_parser(argc, argv)){
        dr_abort_with_code(0);
        return;
    }

    api_register();

    tls_register();

    if(get_log_level() >= 2){
        print_register_vectors();
    }else if(get_log_level() == 1){
        std::vector<reg_id_t> registers = get_all_registers();

        dr_printf("Registers used by backend :\n");
        for(auto i = registers.begin(); i != registers.end(); ++i){
            dr_printf("%s", get_register_name(*i));
            if(i + 1 != registers.end()){
                dr_printf(", ");
            }
        }
        dr_printf("\n");
    }
}

static void event_exit(){
    drmgr_unregister_tls_field(get_index_tls_result());
    drmgr_unregister_tls_field(get_index_tls_gpr());
    drmgr_unregister_tls_field(get_index_tls_float());

    if(get_client_mode() == PLC_CLIENT_GENERATE){
        write_symbols_to_file();
    }
    drreg_exit();
    drmgr_exit();
    drsym_exit();
    Interflop::verrou_end();
}

static void thread_init(void *dr_context){
    SET_TLS(dr_context, get_index_tls_result(), dr_thread_alloc(dr_context, 8));
    SET_TLS(dr_context, get_index_tls_float(),
            dr_thread_alloc(dr_context,
                            AVX_512_SUPPORTED ? 64 * 32 : AVX_SUPPORTED ? 32 *
                                                                          16 :
                                                          16 * 16));
    SET_TLS(dr_context, get_index_tls_gpr(),
            dr_thread_alloc(dr_context, 17 * 8));
}

static void thread_exit(void *dr_context){
    dr_thread_free(dr_context, GET_TLS(dr_context, get_index_tls_result()), 8);
    dr_thread_free(dr_context, GET_TLS(dr_context, get_index_tls_float()),
                   AVX_512_SUPPORTED ? 64 * 32 : AVX_SUPPORTED ? 32 * 16 : 16 *
                                                                           16);
    dr_thread_free(dr_context, GET_TLS(dr_context, get_index_tls_gpr()),
                   17 * 8);
}

#if defined(X86)
static void print() {
    static int print_bool=2;
    if(!print_bool)
        return;
    print_bool--;
    void *context = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.flags = DR_MC_ALL;
    mcontext.size = sizeof(mcontext);

    dr_get_mcontext(context, &mcontext);

    byte ymm[16][32], gpr[16][8];
    
    for(int i=0; i<16; i++)
    {
        reg_get_value_ex(DR_REG_START_YMM+i, &mcontext, ymm[i]);
        reg_get_value_ex(DR_REG_START_GPR+i, &mcontext, gpr[i]);
    }

    dr_printf("*****************************************************************************************************************************\n\n");

   unsigned long long int * tls_gpr = (unsigned long long int*)GET_TLS(dr_get_current_drcontext(), get_index_tls_gpr());
   for(int i=0; i<16; i++)
   {
       dr_printf("Context : %s %lu Saved as : %lu\n", get_register_name(DR_REG_START_GPR+i), *(unsigned long long int*)gpr[i], tls_gpr[i+1]);
   }
   for(int i=0; i<16; i++)
   {
       dr_printf("YMM%d\t%e %e %e %e\n", i, *(double*)&(ymm[i][0]), *(double*)&(ymm[i][8]), *(double*)&(ymm[i][16]), *(double*)&(ymm[i][24]));
   }
    dr_printf("*****************************************************************************************************************************\n\n");
}
#endif

static dr_emit_flags_t app2app_bb_event(void *drcontext, void *tag,
                                        instrlist_t *bb, bool for_trace,
                                        bool translating){
    instr_t *instr, *next_instr;
    OPERATION_CATEGORY oc;
    if(!needs_to_instrument(bb)){
        return DR_EMIT_DEFAULT;
    }
    static int nb = 0;
    for(instr = instrlist_first_app(bb); instr != NULL; instr = next_instr){
        oc = plc_get_operation_category(instr);
        bool registers_saved = false;
        bool should_continue = false;

        do{
            next_instr = instr_get_next_app(instr);

            if(plc_is_instrumented(oc)){

                if(get_log_level() >= 1){
                    dr_printf("%d ", nb);
                    dr_print_instr(drcontext, STDOUT, instr, ": ");
                }
                ++nb;

                bool is_double = plc_is_double(oc);

                if(!registers_saved){
                    insert_save_gpr_and_flags(drcontext, bb, instr);
                    insert_save_simd_registers(drcontext, bb, instr);
                }

                insert_set_destination_tls(drcontext, bb, instr,
                                           GET_REG(DST(instr, 0)));

                insert_set_operands(drcontext, bb, instr, instr, oc);
                if(!registers_saved){
                    insert_restore_rsp(drcontext, bb, instr);
                    translate_insert(
                            XINST_CREATE_sub(drcontext, OP_REG(DR_REG_XSP),
                                             OP_INT(32)), bb, instr);
                }
                registers_saved = true;
                insert_call(drcontext, bb, instr, oc, is_double);
                oc = plc_get_operation_category(next_instr);
                should_continue = next_instr != nullptr &&
                                  plc_is_instrumented(oc);
                if(!should_continue){
                    //It's not a floating point operation
                    insert_restore_simd_registers(drcontext, bb, instr);
                    insert_restore_gpr_and_flags(drcontext, bb, instr);
                }
                // Remove original instruction
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
                registers_saved = true;
            }
            instr = next_instr;
        }while(should_continue);
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t symbol_lookup_event(void *drcontext, void *tag,
                                           instrlist_t *bb, bool for_trace,
                                           bool translating,
                                           OUT void **user_data){
    bool already_found_fp_op = false;

    for(instr_t *instr = instrlist_first_app(bb);
        instr != NULL; instr = instr_get_next_app(instr)){
        OPERATION_CATEGORY oc = plc_get_operation_category(instr);
        if(oc != PLC_UNSUPPORTED && oc != PLC_OTHER){
            if(!already_found_fp_op){
                already_found_fp_op = true;
                log_symbol(bb);
            }
            if(get_log_level() >= 1){
                dr_print_instr(drcontext, STDERR, instr, "Found : ");
            }else{
                break;
            }
        }
    }
    return DR_EMIT_DEFAULT;
}
