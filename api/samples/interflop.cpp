/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "../include/dr_ir_opcodes.h"
#include "../include/dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "drsyms.h"
#include "interflop/interflop_operations.hpp"
#include <vector>
#include <string>

#include "interflop/symbol_config.hpp"
#include "interflop/interflop_client.h"

#if defined(X86)
    static reg_id_t topush_reg[] = {
        DR_REG_XDI, DR_REG_XSI, DR_REG_XAX, DR_REG_XBP, DR_REG_XSP,
        DR_REG_XBX, DR_REG_R8, DR_REG_R9, DR_REG_R10, DR_REG_R11,
        DR_REG_R12, DR_REG_R13, DR_REG_R14, DR_REG_R15
    };
    static reg_id_t topop_reg[] = {
        DR_REG_R15, DR_REG_R14, DR_REG_R13, DR_REG_R12, DR_REG_R11,
        DR_REG_R10, DR_REG_R9, DR_REG_R8, DR_REG_XBX, DR_REG_XSP,
        DR_REG_XBP, DR_REG_XAX, DR_REG_XSI, DR_REG_XDI
    };
    #define NB_REG_SAVED 14
#elif defined(AARCH64)
    static reg_id_t topush_reg[] = {
        DR_REG_X0, DR_REG_X1, DR_REG_X2, DR_REG_X3, DR_REG_X4,
        DR_REG_X5, DR_REG_X6, DR_REG_X7, DR_REG_X8, DR_REG_X9,
        DR_REG_X10, DR_REG_X11, DR_REG_X12, DR_REG_X13, DR_REG_X14, 
        DR_REG_X15, DR_REG_X16, DR_REG_X17, DR_REG_X18, DR_REG_X19,
        DR_REG_X20, DR_REG_X21, DR_REG_X22, DR_REG_X23, DR_REG_X24,
        DR_REG_X25, DR_REG_X26, DR_REG_X27, DR_REG_X28, DR_REG_X29,
        DR_REG_X30, DR_REG_XSP, DR_REG_XZR
    };
    static reg_id_t topop_reg[] = {
        DR_REG_XZR, DR_REG_XSP, DR_REG_X30, DR_REG_X29, DR_REG_X28,
        DR_REG_X27, DR_REG_X26, DR_REG_X25, DR_REG_X24, DR_REG_X23,
        DR_REG_X22, DR_REG_X21, DR_REG_X20, DR_REG_X19, DR_REG_X18,
        DR_REG_X17, DR_REG_X16, DR_REG_X15, DR_REG_X14, DR_REG_X13,
        DR_REG_X12, DR_REG_X11, DR_REG_X10, DR_REG_X9, DR_REG_X8, 
        DR_REG_X7, DR_REG_X6, DR_REG_X5, DR_REG_X4, DR_REG_X3, 
        DR_REG_X2, DR_REG_X1, DR_REG_X0
    };
    #define NB_REG_SAVED 33
#endif

/*
    ORIGINE POSSIBLE DU PROBLEME DU ADD AVEC X7 :
    - problème dans la sauvegarde des registres et les tailles qui n'étaient pas concordantes
    - problème par rapport à DR_BUFFER_REG qui est X7, changer les valeurs pour test
    - problème avec le add dans le fonction push stack list truc, qui fait faisait un add avec x7 et un offset
        - peut-être que du au fait que la taille de la stack n'est plus un multiple de 16, et il aime pas
    - vérifier les XINST add pour voir s'il y a pas des conneries
    - peut-être changer les X en R dans les registres à sauver, pour éviter de potentiels conflits
    - tester avec les autres add
*/
                                        
static void event_exit(void);

static void thread_init(void *drcontext);

static void thread_exit(void *drcontext);

//Function to treat each block of instructions to instrument
static dr_emit_flags_t app2app_bb_event( void *drcontext,        //Context
                                         void *tag,              // Unique identifier of the block
                                         instrlist_t *bb,        // Linked list of the instructions 
                                         bool for_trace,         //TODO
                                         bool translating);      //TODO

//Function to treat each block of instructions to get the symbols
static dr_emit_flags_t symbol_lookup_event( void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,         //TODO
                                            bool translating,       //TODO
                                            OUT void** user_data);

static void module_load_handler(void* drcontext, const module_data_t* module, bool loaded)
{
    dr_module_set_should_instrument(module->handle, shouldInstrumentModule(module));
    //dr_printf("%s %d\n", dr_module_preferred_name(module), dr_module_should_instrument(module->handle));
}    

// Main function to setup the dynamoRIO client
DR_EXPORT void dr_client_main(  client_id_t id, // client ID
                                int argc,   
                                const char *argv[])
{
    drsym_init(0);
    symbol_lookup_config_from_args(argc, argv);
    interflop_client_mode_t client_mode = get_client_mode();
    if(client_mode == IFP_CLIENT_HELP)
    {
        dr_abort_with_code(0);
        return;
    }

    // Init DynamoRIO MGR extension ()
    drmgr_init();
    
    
    // Define the functions to be called before exiting this client program
    dr_register_exit_event(event_exit);

    //Configure verrou in random rounding mode
    interflop_verrou_configure(VR_RANDOM, nullptr);

    
    set_index_tls_result(drmgr_register_tls_field());
    set_index_tls_stack(drmgr_register_tls_field());
    set_index_tls_op_A(drmgr_register_tls_field());
    set_index_tls_op_B(drmgr_register_tls_field());

    drmgr_register_thread_init_event(thread_init);
    drmgr_register_thread_exit_event(thread_exit);


    drreg_options_t drreg_options;
    drreg_options.conservative = true;
    drreg_options.num_spill_slots = 1;
    drreg_options.struct_size = sizeof(drreg_options_t);
    drreg_options.do_not_sum_slots=false;
    drreg_options.error_callback=NULL;
    drreg_init(&drreg_options);
    
    if(client_mode == IFP_CLIENT_GENERATE)
    {
        drmgr_register_bb_instrumentation_event(symbol_lookup_event, NULL, NULL);
    }else
    {
        drmgr_register_module_load_event(module_load_handler);
        drmgr_register_bb_app2app_event(app2app_bb_event, NULL);
    }    
}

static void event_exit(void)
{
    drmgr_unregister_tls_field(get_index_tls_result());
    drmgr_unregister_tls_field(get_index_tls_stack());

    if(get_client_mode() == IFP_CLIENT_GENERATE)
    {
        write_symbols_to_file();
    }
    drmgr_exit();
    drsym_exit();
}

static void thread_init(void *dr_context) {
    SET_TLS(dr_context, get_index_tls_result(),dr_thread_alloc(dr_context, MAX_OPND_SIZE_BYTES));
    SET_TLS(dr_context, get_index_tls_stack(), dr_thread_alloc(dr_context, SIZE_STACK*sizeof(SLOT)));
    SET_TLS(dr_context, get_index_tls_op_A(), dr_thread_alloc(dr_context, MAX_OPND_SIZE_BYTES));
    SET_TLS(dr_context, get_index_tls_op_B(), dr_thread_alloc(dr_context, MAX_OPND_SIZE_BYTES));
}

static void thread_exit(void *dr_context) {
    dr_thread_free(dr_context, GET_TLS(dr_context, get_index_tls_stack()), SIZE_STACK*sizeof(SLOT));
    dr_thread_free(dr_context, GET_TLS(dr_context, get_index_tls_result()), MAX_OPND_SIZE_BYTES);
    dr_thread_free(dr_context, GET_TLS(dr_context, get_index_tls_op_A()), MAX_OPND_SIZE_BYTES);
    dr_thread_free(dr_context, GET_TLS(dr_context, get_index_tls_op_B()), MAX_OPND_SIZE_BYTES);
}


//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

#if defined(X86)
static void print() {
    void *context = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.flags = DR_MC_ALL;
    mcontext.size = sizeof(mcontext);

    dr_get_mcontext(context, &mcontext);

    byte rdi[8], rbp[8], rsp[8],rax[8], rsi[8], rbx[8], rdx[8], rcx[8],
         edi[4], ebp[4], esp[4], eax[4], esi[4], ebx[4], edx[4], ecx[4],
         xmm[16], xmm1[16], xmm2[16], xmm3[16], xmm4[16],
         ymm[32], ymm1[32];
    
    reg_get_value_ex(DR_REG_RDI, &mcontext, rdi);
    reg_get_value_ex(DR_REG_RBP, &mcontext, rbp);
    reg_get_value_ex(DR_REG_RSP, &mcontext, rsp);
    reg_get_value_ex(DR_REG_RAX, &mcontext, rax);
    reg_get_value_ex(DR_REG_RSI, &mcontext, rsi);
    reg_get_value_ex(DR_REG_RBX, &mcontext, rbx);
    reg_get_value_ex(DR_REG_RDX, &mcontext, rdx);
    reg_get_value_ex(DR_REG_RCX, &mcontext, rcx);

    reg_get_value_ex(DR_REG_EDI, &mcontext, edi);
    reg_get_value_ex(DR_REG_EBP, &mcontext, ebp);
    reg_get_value_ex(DR_REG_ESP, &mcontext, esp);
    reg_get_value_ex(DR_REG_EAX, &mcontext, eax);
    reg_get_value_ex(DR_REG_ESI, &mcontext, esi);
    reg_get_value_ex(DR_REG_EBX, &mcontext, ebx);
    reg_get_value_ex(DR_REG_EDX, &mcontext, edx);
    reg_get_value_ex(DR_REG_ECX, &mcontext, ecx);

    reg_get_value_ex(DR_REG_XMM0, &mcontext, xmm);
    reg_get_value_ex(DR_REG_XMM1, &mcontext, xmm1);
    reg_get_value_ex(DR_REG_XMM2, &mcontext, xmm2);
    reg_get_value_ex(DR_REG_XMM3, &mcontext, xmm3);
    reg_get_value_ex(DR_REG_XMM4, &mcontext, xmm4);

    reg_get_value_ex(DR_REG_YMM0, &mcontext, ymm);
    reg_get_value_ex(DR_REG_YMM1, &mcontext, ymm1);

    dr_printf("*****************************************************************************************************************************\n\n");

    dr_printf("RDI : %02X \nEDI : %02X\nRDI : %e\n\n",*((uint64 *)rdi), *((unsigned int*)edi), *((double*)rdi));
    dr_printf("RAX : %02X \nEAX : %02X\nRAX : %e\n\n",*((uint64 *)rax), *((unsigned int*)eax), *((double*)rax));
    dr_printf("RBP : %02X \nEBP : %02X\nRBP : %e\n\n",*((uint64 *)rbp), *((unsigned int*)ebp), *((double*)rbp));    
    dr_printf("RSP : %02X \nESP : %02X\nRSP : %e\n\n",*((uint64 *)rsp), *((unsigned int*)esp), *((double*)rsp));
    dr_printf("RSI : %02X \nESI : %02X\nRSI : %e\n\n",*((uint64 *)rsi), *((unsigned int*)esi), *((double*)rsi));
    dr_printf("RBX : %02X \nEBX : %02X\nRBX : %e\n\n",*((uint64 *)rbx), *((unsigned int*)ebx), *((double*)rbx));
    dr_printf("RDX : %02X \nEDX : %02X\nRDX : %e\n\n",*((uint64 *)rdx), *((unsigned int*)edx), *((double*)rdx));
    dr_printf("RCX : %02X \nECX : %02X\nRCX : %e\n\n",*((uint64 *)rcx), *((unsigned int*)ecx), *((double*)rcx));
    
    /*
    for(int i = 0 ; i < NB_XMM_REG ; i++) {
        reg_get_value_ex(XMM_REG[i], &mcontext, xmm);
        dr_printf("XMM%d : ",i);
        for(int k = 0 ; k < 4 ; k++) dr_printf("%e ",*(float*)&(xmm[4*k]));
        dr_printf("\n");
    }
    */
    dr_printf("YMM 0 : ");
    for(int i = 0 ; i < 4 ; i++) dr_printf("%e ",*(double*)&(ymm[8*i]));
    dr_printf("\n");

    dr_printf("YMM 1 : ");
    for(int i = 0 ; i < 4 ; i++) dr_printf("%e ",*(double*)&(ymm1[8*i]));
    dr_printf("\n");
    
    dr_printf("TLS STACK : %d\t%p\n",get_index_tls_stack(),GET_TLS(context,get_index_tls_stack()));
    dr_printf("TLS RESULT : %d\t%p\n",get_index_tls_result(),GET_TLS(context,get_index_tls_result()));
    dr_printf("TLS A : %d\t%p\n",get_index_tls_op_A(),GET_TLS(context,get_index_tls_op_A()));
    dr_printf("TLS B : %d\t%p\n",get_index_tls_op_B(),GET_TLS(context,get_index_tls_op_B()));

    dr_printf("OPERAND A : ");
    for(int i = 0 ; i < 4 ; i++) dr_printf("%d ",(int)(*((double*)(GET_TLS(context,get_index_tls_op_A()))+i)));
    dr_printf("\n");
    dr_printf("OPERAND B : ");
    for(int i = 0 ; i < 4 ; i++) dr_printf("%d ",(int)(*((double*)(GET_TLS(context,get_index_tls_op_B()))+i)));
    dr_printf("\n");

    dr_printf("\n");
    dr_printf("*****************************************************************************************************************************\n\n");
}
#endif

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

static dr_emit_flags_t app2app_bb_event(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    OPERATION_CATEGORY oc;

    if(!needsToInstrument(bb)) {
        return DR_EMIT_DEFAULT;
    }

    for(instr = instrlist_first_app(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next_app(instr);
        oc = ifp_get_operation_category(instr);

        if(oc)
        {
            bool is_double = ifp_is_double(oc);
            bool is_scalar = ifp_is_scalar(oc);

            
            //#ifdef DEBUG
                dr_print_instr(drcontext, STDERR, instr, "II : ");
            //#endif

            // ****************************************************************************
            // Reserve two registers
            // ****************************************************************************
            reg_id_t buffer_reg  = DR_BUFFER_REG, scratch = DR_SCRATCH_REG;

            dr_save_reg(drcontext, bb, instr, buffer_reg, SPILL_SLOT_BUFFER_REG);
            dr_save_reg(drcontext, bb, instr, scratch, SPILL_SLOT_SCRATCH_REG);
        
            // ****************************************************************************
            // save processor flags
            // ****************************************************************************
            dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_ARITH_FLAG);

            // ****************************************************************************
            // push general purpose registers on pseudo stack 
            // All GPR except DR_BUFFER_REG and DR_SCRATCH_REG
            // ****************************************************************************
            insert_push_pseudo_stack_list(drcontext, topush_reg, bb, instr, buffer_reg, scratch, NB_REG_SAVED);

            // ****************************************************************************
            // Push all Floating point registers
            // ****************************************************************************
            insert_save_floating_reg(drcontext, bb, instr, buffer_reg, scratch);

            // ****************************************************************************
            // ***** Move operands to thread local memory 
            // ****************************************************************************
            insert_move_operands_to_tls_memory(drcontext, bb, instr, oc, is_double);
            // ****************************************************************************
            // ****************************************************************************
            // ****************************************************************************

            // ***** Sub stack pointer to handle the case where XSP is equal to XBP and XSP doesn't match the top of the stack *****
            // ***** Otherwise the call will erase data when pushing the return address *****
            // ***** If the gap is greater than 32 bytes, the program may crash !!!!!!!!!!!!!!! *****
            translate_insert(XINST_CREATE_sub(drcontext, OP_REG(DR_REG_XSP), OP_INT(32)), bb, instr);

            //****************************************************************************
            // ***** CALL *****
            // ****************************************************************************
            insert_call(drcontext, bb, instr, oc, is_double);
            // ****************************************************************************
            // ****************************************************************************
            // ****************************************************************************

            // ****************************************************************************
            // Restore all FLoating point registers 
            // ****************************************************************************
            insert_restore_floating_reg(drcontext, bb, instr, buffer_reg, scratch);
            
            // ****************************************************************************
            // Set the result in the corresponding register
            // ****************************************************************************  
            insert_set_result_in_corresponding_register(drcontext, bb, instr, is_double, is_scalar);

            // ****************************************************************************
            // pop general purpose registers on pseudo stack 
            // All GPR except DR_BUFFER_REG and DR_SCRATCH_REG
            // ****************************************************************************
            insert_pop_pseudo_stack_list(drcontext, topop_reg, bb, instr, buffer_reg, scratch, NB_REG_SAVED);

            // ****************************************************************************
            // Restore processor flags
            // ****************************************************************************
            dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_ARITH_FLAG);
           
            // Restore registers
            // ****************************************************************************
            dr_restore_reg(drcontext, bb, instr, buffer_reg, SPILL_SLOT_BUFFER_REG);
            dr_restore_reg(drcontext, bb, instr, scratch, SPILL_SLOT_SCRATCH_REG);

            // ****************************************************************************
            // Remove original instruction
            // ****************************************************************************
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);           
        }       
    }
    return DR_EMIT_DEFAULT;
}


//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

static dr_emit_flags_t symbol_lookup_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, OUT void** user_data)
{
    instr_t *instr, *next_instr;
    OPERATION_CATEGORY oc;
    
    bool already_found_fp_op = false;

    for(instr = instrlist_first_app(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next_app(instr);
        oc = ifp_get_operation_category(instr);
        if(oc)
        {
            dr_print_instr(drcontext, STDERR, instr, "Found : ");
            if(!already_found_fp_op)
            {
                already_found_fp_op=true;
                logSymbol(bb);
            }
        }

    }
    return DR_EMIT_DEFAULT;
}
