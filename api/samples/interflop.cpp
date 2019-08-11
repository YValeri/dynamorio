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
#include <algorithm>

#include "interflop/symbol_config.hpp"
#include "interflop/interflop_client.h"
#include "interflop/analyse.hpp"
#include "interflop/utils.hpp"

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
		DR_REG_X30
	};
	static reg_id_t topop_reg[] = {
		DR_REG_X30, DR_REG_X29, DR_REG_X28, DR_REG_X27, DR_REG_X26, 
		DR_REG_X25, DR_REG_X24, DR_REG_X23, DR_REG_X22, DR_REG_X21, 
		DR_REG_X20, DR_REG_X19, DR_REG_X18, DR_REG_X17, DR_REG_X16, 
		DR_REG_X15, DR_REG_X14, DR_REG_X13, DR_REG_X12, DR_REG_X11, 
		DR_REG_X10, DR_REG_X9, DR_REG_X8, DR_REG_X7, DR_REG_X6, 
		DR_REG_X5, DR_REG_X4, DR_REG_X3, DR_REG_X2, DR_REG_X1, 
		DR_REG_X0
	};
	#define NB_REG_SAVED 31
#endif
										
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

void printMessage(int msg)
{
    dr_printf("%d\n", msg);
}

#define INSERT_MSG(dc, bb, where, x) dr_insert_clean_call(dc,bb, where, (void*)printMessage,true, 1, opnd_create_immed_int(x, OPSZ_4))

static void module_load_handler(void* drcontext, const module_data_t* module, bool loaded)
{
	dr_module_set_should_instrument(module->handle, should_instrument_module(module));
}

static void api_initialisation(){
    drsym_init(0);

    // Init DynamoRIO MGR extension
    drmgr_init();

    //Configure verrou in random rounding mode
    Interflop::verrou_prepare();

    drreg_options_t drreg_options;
    drreg_options.conservative = true;
    drreg_options.num_spill_slots = 3;
    drreg_options.struct_size = sizeof(drreg_options_t);
    drreg_options.do_not_sum_slots=true;
    drreg_options.error_callback=NULL;
    drreg_init(&drreg_options);
}

static void api_register(){
    // Define the functions to be called before exiting this client program
    dr_register_exit_event(event_exit);

    drmgr_register_thread_init_event(thread_init);
    drmgr_register_thread_exit_event(thread_exit);

    if(get_client_mode() == IFP_CLIENT_GENERATE){
        drmgr_register_bb_instrumentation_event(symbol_lookup_event, NULL, NULL);
    }else{
        drmgr_register_module_load_event(module_load_handler);
        drmgr_register_bb_app2app_event(app2app_bb_event, NULL);
    }  
}

static void tls_register(){
    set_index_tls_result(drmgr_register_tls_field());
    set_index_tls_float(drmgr_register_tls_field());
    set_index_tls_gpr(drmgr_register_tls_field());
    //set_index_tls_stack(drmgr_register_tls_field());
    //set_index_tls_op_A(drmgr_register_tls_field());
    //set_index_tls_op_B(drmgr_register_tls_field());
    //set_index_tls_op_C(drmgr_register_tls_field());
    //set_index_tls_saved_reg(drmgr_register_tls_field());
}

// Main function to setup the dynamoRIO client
DR_EXPORT void dr_client_main(  client_id_t id, // client ID
								int argc,   
								const char *argv[])
{
    api_initialisation();

    if(arguments_parser(argc, argv)){
        dr_abort_with_code(0);
        return;
    }

    api_register();

    tls_register();

    if(is_debug_enabled()){
        print_register_vectors();
    }

    /*
    std::vector<reg_id_t> registers = get_all_registers();

    for(auto i = registers.begin(); i != registers.end(); ++i){
        dr_printf("%s", get_register_name(*i));
        if(i+1 != registers.end()){
            dr_printf(", ");
        }
    }
    
    dr_printf("\n");*/
}

static void event_exit()
{
	drmgr_unregister_tls_field(get_index_tls_result());
	drmgr_unregister_tls_field(get_index_tls_stack());

	if(get_client_mode() == IFP_CLIENT_GENERATE){
		write_symbols_to_file();
	}
	drreg_exit();
	drmgr_exit();
	drsym_exit();
	Interflop::verrou_end();
}

static void thread_init(void *dr_context) {
    SET_TLS(dr_context , get_index_tls_result() ,dr_thread_alloc(dr_context , 8));
    SET_TLS(dr_context, get_index_tls_float(), dr_thread_alloc(dr_context, 32*16));
    SET_TLS(dr_context,get_index_tls_gpr(),dr_thread_alloc(dr_context, 8*17));

    //SET_TLS(dr_context , get_index_tls_stack() , dr_thread_alloc(dr_context , SIZE_STACK*sizeof(SLOT)));
    //SET_TLS(dr_context , get_index_tls_op_A() , dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
    //SET_TLS(dr_context , get_index_tls_op_B() , dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
    //SET_TLS(dr_context , get_index_tls_op_C() , dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
    //SET_TLS(dr_context, get_index_tls_saved_reg(), dr_thread_alloc(dr_context, 64*3));
}

static void thread_exit(void *dr_context) {
    //dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_stack()) , SIZE_STACK*sizeof(SLOT));
    dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_result()) , 8);
    dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_float()) , 32*16);
    dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_gpr()) , 8*17);
    //dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_op_A()) , MAX_OPND_SIZE_BYTES);
    //dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_op_B()) , MAX_OPND_SIZE_BYTES);
    //dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_op_C()) , MAX_OPND_SIZE_BYTES);
    //dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_saved_reg()) , 64*3);
}


//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

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

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

reg_id_t get_biggest_simd_version(reg_id_t simd)
{
    DR_ASSERT_MSG(reg_is_simd(simd), "NOT AN SIMD REGISTER");
    reg_id_t dstbase=DR_REG_START_XMM, srcbase=DR_REG_START_XMM;
    if(AVX_512_SUPPORTED)
    {
        dstbase=DR_REG_START_ZMM;
    }else if(AVX_SUPPORTED)
    {
        dstbase = DR_REG_START_YMM;
    }
    if(reg_is_strictly_zmm(simd)){
        srcbase = DR_REG_START_ZMM;
    }else if(reg_is_strictly_ymm(simd)){
        srcbase = DR_REG_START_YMM;
    }
    return dstbase+(simd-srcbase);
}

size_t get_idx_of_simd(reg_id_t simd)
{
    DR_ASSERT_MSG(reg_is_simd(simd), "NOT AN SIMD REGISTER");
    
    if(reg_is_strictly_zmm(simd))
        return simd-DR_REG_START_ZMM;
    if(reg_is_strictly_ymm(simd))
        return simd-DR_REG_START_YMM;    
    return simd-DR_REG_START_XMM;
}

reg_id_t get_biggest_gpr_version(reg_id_t gpr)
{
    return reg_resize_to_opsz(gpr, OPSZ_8);
}

struct context_status_t
{
    bool arith_flags_valid;
    bool arith_flags_valid_in_memory;

#if defined(X86) && defined(X64)
    bool gpr_valid[16];
    bool simd_valid[32];
    
    bool gpr_valid_in_memory[16];
    bool simd_valid_in_memory[32];

    context_status_t()
    {
        for(size_t i=0; i<16; i++)
        {
            gpr_valid[i]=false;
            gpr_valid_in_memory[i]=false;
        }
        for(size_t i=0; i<32; i++)
        {
            simd_valid[i]=false;
            simd_valid_in_memory[i]=false;
        }
        arith_flags_valid=false;
        arith_flags_valid_in_memory = false;
    }
#endif

    bool is_gpr_valid(reg_id_t gpr) {
        DR_ASSERT_MSG(reg_is_gpr(gpr), "is_gpr_valid : REGISTER ISN'T GPR");
        return gpr_valid[get_biggest_gpr_version(gpr) - DR_REG_START_GPR];
    }
    bool is_simd_valid(reg_id_t simd) {
        DR_ASSERT_MSG(reg_is_simd(simd), "is_simd_valid : REGISTER ISN'T SIMD");
        return simd_valid[get_idx_of_simd(simd)];
    }
    bool is_gpr_valid_in_memory(reg_id_t gpr) {
        DR_ASSERT_MSG(reg_is_gpr(gpr), "is_gpr_valid_in_memory : REGISTER ISN'T GPR");
        return gpr_valid_in_memory[get_biggest_gpr_version(gpr) - DR_REG_START_GPR];
    }
    bool is_simd_valid_in_memory(reg_id_t simd) {
        DR_ASSERT_MSG(reg_is_simd(simd), "is_simd_valid_in_memory : REGISTER ISN'T SIMD");
        return simd_valid_in_memory[get_idx_of_simd(simd)];
    }
    void set_gpr_valid(reg_id_t gpr, bool valid)
    {
        DR_ASSERT_MSG(reg_is_gpr(gpr), "set_gpr_valid : REGISTER ISN'T GPR");
        gpr_valid[get_biggest_gpr_version(gpr) - DR_REG_START_GPR] = valid;
    }
    void set_simd_valid(reg_id_t simd, bool valid) {
        DR_ASSERT_MSG(reg_is_simd(simd), "set_simd_valid : REGISTER ISN'T SIMD");
        simd_valid[get_idx_of_simd(simd)] = valid;
    }
    void set_gpr_valid_in_memory(reg_id_t gpr, bool valid)
    {
        DR_ASSERT_MSG(reg_is_gpr(gpr), "set_gpr_valid_in_memory : REGISTER ISN'T GPR");
        gpr_valid_in_memory[get_biggest_gpr_version(gpr) - DR_REG_START_GPR] = valid;
    }
    void set_simd_valid_in_memory(reg_id_t simd, bool valid) {
        DR_ASSERT_MSG(reg_is_simd(simd), "set_simd_valid_in_memory : REGISTER ISN'T SIMD");
        simd_valid_in_memory[get_idx_of_simd(simd)] = valid;
    }
    void copy(context_status_t cs)
    {
        for(size_t i=0; i<16; i++)
        {
            gpr_valid[i]=cs.gpr_valid[i];
            gpr_valid_in_memory[i]=cs.gpr_valid_in_memory[i];
        }
        for(size_t i=0; i<32; i++)
        {
            simd_valid[i]=cs.simd_valid[i];
            simd_valid_in_memory[i]=cs.simd_valid_in_memory[i];
        }
        arith_flags_valid=cs.arith_flags_valid;
        arith_flags_valid_in_memory = cs.arith_flags_valid_in_memory;
    }
};

#define MINSERT(bb, where, instr) instrlist_meta_preinsert(bb, where, instr)

context_status_t get_backend_requirements()
{
    context_status_t status;
    for(size_t i=0; i<16; i++)
    {
        status.gpr_valid_in_memory[i]=true;
        status.gpr_valid[i]=false;
    }
    status.set_gpr_valid(DR_REG_RSP, true);
    for(size_t i=0; i<(AVX_512_SUPPORTED ? 32 : 16); i++)
    {
        status.simd_valid_in_memory[i]=true;
        status.simd_valid[i]=false;
    }
    status.arith_flags_valid_in_memory=true;
    status.arith_flags_valid=false;
    return status;
}

void update_status_after_for_backend(context_status_t & status)
{
    for(size_t i=0; i<16; i++)
    {
        status.gpr_valid[i]=false;
    }
    for(size_t i=0; i<(AVX_512_SUPPORTED ? 32 : 16); i++)
    {
        status.simd_valid[i]=false;
    }
    status.arith_flags_valid=false;
}

struct sub_block_t
{
    bool instrumentable;
    context_status_t requirements;
    context_status_t status_after;
    uint64 numberOfInstructions;
    bool is_last;
};

static std::vector<sub_block_t> preanalysis(instrlist_t* bb)
{
    std::vector<sub_block_t> blocks;
    instr_t *instr, *next_instr;
    OPERATION_CATEGORY oc;
    bool shouldContinue;
    for(instr = instrlist_first_app(bb); instr != nullptr; instr = next_instr)
    {
        oc = ifp_get_operation_category(instr);
        sub_block_t currBlock; 
        currBlock.instrumentable = ifp_is_instrumented(oc);
        currBlock.numberOfInstructions=0;
        currBlock.is_last=false;
        if(currBlock.instrumentable)
        {
            currBlock.requirements.copy(get_backend_requirements());
            if(!blocks.empty())
            {
                currBlock.status_after.copy(blocks[blocks.size()-1].status_after);
            }
            for(size_t i=0; i<16; ++i)
            {
                currBlock.status_after.gpr_valid[i] |= currBlock.requirements.gpr_valid[i];
                currBlock.status_after.gpr_valid_in_memory[i] |= currBlock.requirements.gpr_valid_in_memory[i];
            }
            for(size_t i=0; i<(AVX_512_SUPPORTED ? 32 : 16); ++i)
            {
                currBlock.status_after.simd_valid[i] |= currBlock.requirements.simd_valid[i];
                currBlock.status_after.simd_valid_in_memory[i] |= currBlock.requirements.simd_valid_in_memory[i];
            }
            update_status_after_for_backend(currBlock.status_after);
            
        }else
        {
            if(!blocks.empty())
            {
                currBlock.status_after.copy(blocks[blocks.size()-1].status_after); 
            }
        }
        
        shouldContinue=true;
        do{
            int nsrc = instr_num_srcs(instr);
            int ndst = instr_num_dsts(instr);

            for(int i=0; i<nsrc; i++)
            {
                opnd_t src = instr_get_src(instr, i);
                if(opnd_is_reg(src))
                {
                    reg_id_t reg = opnd_get_reg(src);
                    if(currBlock.instrumentable)
                    {
                        currBlock.requirements.set_simd_valid_in_memory(reg, true);
                    }else
                    {
                        if(reg_is_gpr(reg))
                        {
                            currBlock.requirements.set_gpr_valid(reg, true);
                        }else
                        {
                            currBlock.requirements.set_simd_valid(reg, true);
                        }
                    }  
                }else if(opnd_is_base_disp(src))
                {
                    reg_id_t base = opnd_get_base(src), index = opnd_get_index(src);
                    if(base != DR_REG_NULL)
                    {
                        if(currBlock.instrumentable)
                        {
                            currBlock.requirements.set_gpr_valid_in_memory(base, true);
                        }else
                        {
                            currBlock.requirements.set_gpr_valid(base, true);
                        }
                    }
                    if(index != DR_REG_NULL)
                    {
                        if(currBlock.instrumentable)
                        {
                            currBlock.requirements.set_gpr_valid_in_memory(index, true);
                        }else
                        {
                            currBlock.requirements.set_gpr_valid(index, true);
                        }
                    }
                }
            }
            for(int i=0; i<ndst; i++)
            {
                opnd_t dst = instr_get_dst(instr, i);
                if(opnd_is_reg(dst)){
                    reg_id_t reg = opnd_get_reg(dst);
                    if(currBlock.instrumentable){
                        currBlock.requirements.set_simd_valid_in_memory(reg, true);
                        currBlock.status_after.set_simd_valid_in_memory(reg, true);
                        currBlock.status_after.set_simd_valid(reg, false);
                    }else
                    {
                        if(reg_is_gpr(reg))
                        {
                            currBlock.status_after.set_gpr_valid(reg, true);
                            currBlock.status_after.set_gpr_valid_in_memory(reg, false);
                        }else
                        {
                            currBlock.status_after.set_simd_valid(reg, true);
                            currBlock.status_after.set_simd_valid_in_memory(reg, false);
                        }
                        
                    }
                }else if(opnd_is_base_disp(dst))
                {
                    //Only in non instrumentable
                    reg_id_t base = opnd_get_base(dst), index = opnd_get_index(dst);
                    if(base != DR_REG_NULL)
                    {
                        currBlock.requirements.set_gpr_valid(base, true);
                    }
                    if(index != DR_REG_NULL)
                    {
                        currBlock.requirements.set_gpr_valid(index, true);
                    }
                }
            }
            if(!currBlock.instrumentable)
            {
                for(size_t i=0; i<16; i++)
                {
                    currBlock.status_after.gpr_valid[i] |= currBlock.requirements.gpr_valid[i];
                }
                currBlock.requirements.arith_flags_valid = ((instr_get_eflags(instr, DR_QUERY_INCLUDE_ALL) & (EFLAGS_READ_ARITH|EFLAGS_WRITE_ARITH)) != 0);
                currBlock.status_after.arith_flags_valid = (blocks.empty() ? true : currBlock.requirements.arith_flags_valid | blocks[blocks.size()-1].status_after.arith_flags_valid);
                //currBlock.status_after.arith_flags_valid_in_memory=false;
                if(((instr_get_eflags(instr, DR_QUERY_INCLUDE_ALL) & (EFLAGS_WRITE_ARITH)) != 0))
                {
                    currBlock.status_after.arith_flags_valid_in_memory=false;
                }else
                {
                    currBlock.status_after.arith_flags_valid_in_memory = (blocks.empty() ? false : blocks[blocks.size()-1].status_after.arith_flags_valid_in_memory);
                }
                
            }else
            {
                currBlock.requirements.arith_flags_valid=false;
                currBlock.requirements.arith_flags_valid_in_memory=true;
                currBlock.status_after.arith_flags_valid=false;
                currBlock.status_after.arith_flags_valid_in_memory=true;
            }
            
            ++currBlock.numberOfInstructions;
            next_instr = instr_get_next_app(instr);
            instr = next_instr;
            if(next_instr != nullptr)
            {
                oc = ifp_get_operation_category(next_instr);
                if(ifp_is_instrumented(oc) != currBlock.instrumentable)
                {
                    shouldContinue=false;
                }
            }else
            {
                currBlock.is_last=true;
                shouldContinue=false;
            }
        }while(shouldContinue);
        blocks.push_back(currBlock);
    }
    return blocks;
}

bool status_is_compatible(context_status_t const & current, context_status_t const & desired)
{
    if(desired.arith_flags_valid && !current.arith_flags_valid)
    {
        dr_printf("ARITH_FLAGS_INVALID Current : %d Desired : %d\n", current.arith_flags_valid, desired.arith_flags_valid);
        return false;
    }
    if(desired.arith_flags_valid_in_memory && !current.arith_flags_valid_in_memory)
    {
        dr_printf("ARITH_FLAGS_INVALID_in_mem Current : %d Desired : %d\n", current.arith_flags_valid_in_memory, desired.arith_flags_valid_in_memory);
        return false;
    }
    for(size_t i=0; i<16; i++)
    {
        if((desired.gpr_valid[i] && !current.gpr_valid[i]) || (desired.gpr_valid_in_memory[i] && !current.gpr_valid_in_memory[i]))
        {
            dr_printf("%s Current : %d Desired : %d Current_mem : %d Desired_mem : %d\n", get_register_name(DR_REG_START_GPR+i),current.gpr_valid[i], desired.gpr_valid[i], current.gpr_valid_in_memory[i], desired.gpr_valid_in_memory[i]);
            return false;
        }
    }
    const size_t nb_of_simd = AVX_512_SUPPORTED ? 32 : 16;
    for(size_t i=0; i<nb_of_simd; i++)
    {
        if((desired.simd_valid[i] && !current.simd_valid[i]) || (desired.simd_valid_in_memory[i] && !current.simd_valid_in_memory[i]))
        {
            dr_printf("%s Current : %d Desired : %d Current_mem : %d Desired_mem : %d\n", get_register_name(DR_REG_START_YMM+i),current.simd_valid[i], desired.simd_valid[i], current.simd_valid_in_memory[i], desired.simd_valid_in_memory[i]);
            return false;
        }
    }
    return true;
}

void meet_requirements(void* drc, instrlist_t* bb, instr_t* where, context_status_t & current, context_status_t const & desired, bool desired_instrumentable)
{
    static const bool is_avx = AVX_SUPPORTED;
    //dr_printf("STATUS BEFORE :\n");
    //dr_printf("Arith : %d->%d\nArith mem : %d->%d\n",current.arith_flags_valid, desired.arith_flags_valid, current.arith_flags_valid_in_memory, desired.arith_flags_valid_in_memory);
    /*for(size_t i=0; i<16; i++)
    {
        dr_printf("%s : %d->%d  mem : %d->%d\n", get_register_name(DR_REG_START_GPR+i), current.gpr_valid[i], desired.gpr_valid[i], current.gpr_valid_in_memory[i], desired.gpr_valid_in_memory[i]);
    }
    for(size_t i=0; i<16; i++)
    {
        dr_printf("%s : %d->%d  mem : %d->%d\n", get_register_name(DR_REG_START_YMM+i), current.simd_valid[i], desired.simd_valid[i], current.simd_valid_in_memory[i], desired.simd_valid_in_memory[i]);
    }*/
    std::vector<reg_id_t> gpr_needs_save;
    std::vector<reg_id_t> simd_needs_save;
    std::vector<reg_id_t> gpr_needs_restore;
    std::vector<reg_id_t> simd_needs_restore;
    bool flags_need_save=false;
    bool flags_need_restore=false;

    bool found_reg_for_tls=false;
    bool need_to_save_in_spill=false;
    bool need_to_save_tls_reg=false;
    bool reg_for_tls_valid_in_spill=false;
    bool need_to_restore_tls_reg=false;
    reg_id_t tls_reg=DR_REG_NULL;

    if(desired.arith_flags_valid && !current.arith_flags_valid)
    {
        DR_ASSERT_MSG(current.arith_flags_valid_in_memory, "CANNOT RESTORE FROM INVALID ARITH IN MEMORY\n");
        flags_need_restore=true;
    }
    if(desired.arith_flags_valid_in_memory && !current.arith_flags_valid_in_memory)
    {
        DR_ASSERT_MSG(current.arith_flags_valid, "CANNOT SAVE FROM INVALID ARITH\n");
        flags_need_save=true;
    }

    for(int i=0; i<16; i++)
    {
        if(desired.gpr_valid[i] && !current.gpr_valid[i])
        {
            DR_ASSERT_MSG(current.gpr_valid_in_memory[i], "CANNOT RESTORE FROM INVALID GPR IN MEMORY\n");
            gpr_needs_restore.push_back(DR_REG_START_GPR+i);
            if(i > 0 && !found_reg_for_tls) //We don't want to use RAX, as it would interfere with arith_flags save/restore
            {
                found_reg_for_tls=true;
                tls_reg=DR_REG_START_GPR+i;
                need_to_restore_tls_reg=true;
            }
        }
        if(desired.gpr_valid_in_memory[i] && !current.gpr_valid_in_memory[i])
        {
            DR_ASSERT_MSG(current.gpr_valid[i], "CANNOT SAVE FROM INVALID GPR\n");
            gpr_needs_save.push_back(DR_REG_START_GPR+i);
        }
    }
    for(int i=0; i<(AVX_512_SUPPORTED ? 32 : 16); i++)
    {
        if(desired.simd_valid[i] && !current.simd_valid[i])
        {
            DR_ASSERT_MSG(current.simd_valid_in_memory[i], "CANNOT RESTORE FROM INVALID SIMD IN MEMORY\n");
            simd_needs_restore.push_back(get_biggest_simd_version(DR_REG_START_XMM+i));
        }
        if(desired.simd_valid_in_memory[i] && !current.simd_valid_in_memory[i])
        {
            DR_ASSERT_MSG(current.simd_valid[i], "CANNOT SAVE FROM INVALID SIMD\n");
            simd_needs_save.push_back(get_biggest_simd_version(DR_REG_START_XMM+i));
        }
    }
    if(!found_reg_for_tls)
    {
        //Try to use an invalid register that we don't restore anyway
        for(int i=1; i<16;i++)
        {
            if(!current.gpr_valid[i])
            {
                found_reg_for_tls=true;
                tls_reg = DR_REG_START_GPR+i;
                break;
            }
        }
        if(!found_reg_for_tls)
        {
            //We'll save a valid register in a spill slot 
            need_to_save_in_spill=true;
            found_reg_for_tls=true;
            tls_reg = DR_REG_STOP_GPR;
            need_to_save_tls_reg = (std::find(gpr_needs_save.begin(),gpr_needs_save.end(), tls_reg) != gpr_needs_save.end());
        }
    }
    //dr_printf("TLS REG : %s\n", get_register_name(tls_reg));
    //Do the moves
    if(need_to_save_in_spill)
    {
        dr_save_reg(drc, bb, where, tls_reg, SPILL_SLOT_SCRATCH_REG);
        reg_for_tls_valid_in_spill=true;
    }
    INSERT_READ_TLS(drc, get_index_tls_gpr(),bb, where, tls_reg);
    current.set_gpr_valid(tls_reg,false);

    bool arith_reg_needs_restore=false;

    //Save GPR
    for(auto gpr : gpr_needs_save)
    {
        if(gpr != tls_reg)
        {
            MINSERT(bb, where, XINST_CREATE_store(drc, OP_BASE_DISP(tls_reg, offset_of_gpr(gpr), OPSZ_8),OP_REG(gpr)));
            current.set_gpr_valid_in_memory(gpr, true);
        }
    }

    //Save the register used as tls if needed
    if(need_to_save_tls_reg && need_to_save_in_spill)
    {
        if(current.is_gpr_valid(DR_REG_RAX) && !current.is_gpr_valid_in_memory(DR_REG_RAX))
        {
            //If RAX hasn't been saved, we need to save it
            MINSERT(bb, where, XINST_CREATE_store(drc, OP_BASE_DISP(tls_reg, offset_of_gpr(DR_REG_RAX), OPSZ_8),OP_REG(DR_REG_RAX)));
            current.set_gpr_valid_in_memory(DR_REG_RAX, true);
        }
        arith_reg_needs_restore=true;
        dr_restore_reg(drc, bb, where, DR_REG_RAX, SPILL_SLOT_SCRATCH_REG);
        current.set_gpr_valid(DR_REG_RAX, false);
        //We need an application instruction here to ensure a restore, but this invalidates the spill slot
        translate_insert(XINST_CREATE_store(drc, OP_BASE_DISP(tls_reg, offset_of_gpr(tls_reg), OPSZ_8),OP_REG(DR_REG_RAX)), bb, where);
        current.set_gpr_valid_in_memory(tls_reg, true);
        reg_for_tls_valid_in_spill=false;
    }

    //Save Arithmetic flags
    if(flags_need_save)
    {
        if(current.is_gpr_valid(DR_REG_RAX) && !current.is_gpr_valid_in_memory(DR_REG_RAX))
        {
            //If RAX hasn't been saved, we need to save it
            MINSERT(bb, where, XINST_CREATE_store(drc, OP_BASE_DISP(tls_reg, offset_of_gpr(DR_REG_RAX), OPSZ_8),OP_REG(DR_REG_RAX)));
            current.set_gpr_valid_in_memory(DR_REG_RAX, true);
        }
        arith_reg_needs_restore=true;
        MINSERT(bb, where, INSTR_CREATE_lahf(drc));
        current.set_gpr_valid(DR_REG_RAX, false);
        MINSERT(bb, where, XINST_CREATE_store(drc, OP_BASE_DISP(tls_reg, 0, OPSZ_8),OP_REG(DR_REG_RAX)));
        current.arith_flags_valid_in_memory=true;
    }


    //Restore Arithmetic flags
    if(flags_need_restore)
    {
        if(current.is_gpr_valid(DR_REG_RAX) && !current.is_gpr_valid_in_memory(DR_REG_RAX))
        {
            //If RAX hasn't been saved, we need to save it
            MINSERT(bb, where, XINST_CREATE_store(drc, OP_BASE_DISP(tls_reg, offset_of_gpr(DR_REG_RAX), OPSZ_8),OP_REG(DR_REG_RAX)));
            current.set_gpr_valid_in_memory(DR_REG_RAX, true);
        }
        arith_reg_needs_restore=true;
        MINSERT(bb, where, XINST_CREATE_load(drc, OP_REG(DR_REG_RAX), OP_BASE_DISP(tls_reg, 0, OPSZ_8)));
        current.set_gpr_valid(DR_REG_RAX, false);
        MINSERT(bb, where, INSTR_CREATE_sahf(drc));
        current.arith_flags_valid=true;
    }

    //Save and restore SIMD registers
    if(!simd_needs_save.empty() || !simd_needs_restore.empty())
    {
        opnd_size_t simd_size = AVX_512_SUPPORTED ? OPSZ_64 : AVX_SUPPORTED ? OPSZ_32 : OPSZ_16;
        INSERT_READ_TLS(drc, get_index_tls_float(),bb, where, tls_reg);
        for(auto simd : simd_needs_save)
        {
            DR_ASSERT_MSG(current.is_simd_valid(simd), "CANT SAVE SIMD BECAUSE IT'S INVALID\n");
            MINSERT(bb, where, MOVE_FLOATING_PACKED(is_avx, drc, OP_BASE_DISP(tls_reg, offset_of_simd(simd), simd_size), OP_REG(simd)));
            current.set_simd_valid_in_memory(simd, true);
        }
        for(auto simd : simd_needs_restore)
        {
            DR_ASSERT_MSG(current.is_simd_valid_in_memory(simd), "CANT RESTORE SIMD BECAUSE IT'S INVALID IN MEMORY\n");
            MINSERT(bb, where, MOVE_FLOATING_PACKED(is_avx, drc, OP_REG(simd), OP_BASE_DISP(tls_reg, offset_of_simd(simd), simd_size)));
            current.set_simd_valid(simd, true);
        }
        INSERT_READ_TLS(drc, get_index_tls_gpr(),bb, where, tls_reg);
    }

    //Restore gpr
    for(auto gpr : gpr_needs_restore)
    {
        if(gpr != tls_reg)
        {
            DR_ASSERT_MSG(current.is_gpr_valid_in_memory(gpr), "CANT RESTORE GPR BECAUSE IT'S INVALID IN MEMORY\n");
            MINSERT(bb, where, XINST_CREATE_load(drc, OP_REG(gpr), OP_BASE_DISP(tls_reg, offset_of_gpr(gpr), OPSZ_8)));
            current.set_gpr_valid(gpr, true);
        }
    }

    //Restore the reg used for arithmetic movement if necessary
    if(arith_reg_needs_restore && !current.is_gpr_valid(DR_REG_RAX))
    {
        DR_ASSERT_MSG(current.is_gpr_valid_in_memory(DR_REG_RAX), "CANT RESTORE ARITH REG BECAUSE IT'S INVALID IN MEMORY\n");
        MINSERT(bb, where, XINST_CREATE_load(drc, OP_REG(DR_REG_RAX), OP_BASE_DISP(tls_reg, offset_of_gpr(DR_REG_RAX), OPSZ_8)));
        current.set_gpr_valid(DR_REG_RAX, true);
    }

    //Restore the value of the spilled register
    if(need_to_save_in_spill || need_to_restore_tls_reg)
    {
        if(reg_for_tls_valid_in_spill)
        {
            dr_restore_reg(drc, bb, where, tls_reg, SPILL_SLOT_SCRATCH_REG);
        }else
        {
            DR_ASSERT_MSG(current.is_gpr_valid_in_memory(tls_reg), "CANT RESTORE TLS REG BECAUSE IT'S INVALID\n");
            MINSERT(bb, where, XINST_CREATE_load(drc, OP_REG(tls_reg), OP_BASE_DISP(tls_reg, offset_of_gpr(tls_reg), OPSZ_8)));
        }
        current.set_gpr_valid(tls_reg, true);
    }
    //dr_printf("\nSTATUS AFTER :\n");
    //dr_printf("Arith : %d->%d\nArith mem : %d->%d\n",current.arith_flags_valid, desired.arith_flags_valid, current.arith_flags_valid_in_memory, desired.arith_flags_valid_in_memory);
    /*for(size_t i=0; i<16; i++)
    {
        dr_printf("%s : %d->%d  mem : %d->%d\n", get_register_name(DR_REG_START_GPR+i), current.gpr_valid[i], desired.gpr_valid[i], current.gpr_valid_in_memory[i], desired.gpr_valid_in_memory[i]);
    }
    for(size_t i=0; i<16; i++)
    {
        dr_printf("%s : %d->%d  mem : %d->%d\n", get_register_name(DR_REG_START_YMM+i), current.simd_valid[i], desired.simd_valid[i], current.simd_valid_in_memory[i], desired.simd_valid_in_memory[i]);
    }*/
    DR_ASSERT_MSG(status_is_compatible(current, desired), "STATUS ISN'T COMPATIBLE");

}

static dr_emit_flags_t app2app_bb_event(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    OPERATION_CATEGORY oc;

    if(!needs_to_instrument(bb)) {
        return DR_EMIT_DEFAULT;
    }

    /*dr_printf("**\n");
    for(instr = instrlist_first_app(bb); instr!=nullptr; instr = instr_get_next_app(instr))
    {
        dr_print_instr(drcontext, STDERR, instr, "");
    }*/

    
    std::vector<sub_block_t> blocks = preanalysis(bb);
    if(blocks.empty() || (blocks.size() == 1 && !blocks[0].instrumentable))
    {
        //No instrumentable sub_block
        return DR_EMIT_DEFAULT;
    }

    //dr_printf("************************************\n");
    //static int nb=0;
    int instrCount=0;

    context_status_t current_status;
    current_status.arith_flags_valid=true;
    for(size_t i=0; i<16; i++)
    {
        current_status.gpr_valid[i]=true;
    }
    for(size_t i=0; i<32; i++)
    {
        current_status.simd_valid[i]=true;
    }
    context_status_t endstatus;
    endstatus.copy(current_status);
    //context_requirement_t current_register_statuses = get_context_full_of(IFP_REG_ORIGINAL, true);
    instr = instrlist_first_app(bb);

    for(size_t block_idx=0; block_idx<blocks.size(); ++block_idx)
    {
        sub_block_t block = blocks[block_idx];
        if(block.is_last)
        {
            if(!block.instrumentable)
            {
                //Last block, we restore everything
                meet_requirements(drcontext, bb, instr, current_status, endstatus, false);
                //for(;instr !=nullptr; instr=instr_get_next_app(instr)) dr_print_instr(drcontext, STDOUT, instr, "");
                //And since we don't need to do anything, we return
            }else
            {
                meet_requirements(drcontext, bb, instr, current_status, block.requirements, block.instrumentable);
                for(size_t i=0; i<block.numberOfInstructions; i++)
                {
                    oc = ifp_get_operation_category(instr);
                    bool is_double = ifp_is_double(oc);
                    insert_set_destination_tls(drcontext, bb, instr, GET_REG(DST(instr, 0)));
                    insert_set_operands(drcontext, bb, instr, instr, oc);
                    if(i==0)
                    {
                        insert_restore_rsp(drcontext, bb, instr);
                        translate_insert(XINST_CREATE_sub(drcontext, OP_REG(DR_REG_XSP), OP_INT(32)), bb, instr);
                    }
                    //dr_print_instr(drcontext, STDOUT, instr, "");
                    insert_call(drcontext, bb, instr, oc, is_double);
                    next_instr = instr_get_next_app(instr);
                    if(next_instr == nullptr)
                    {
                        //Last instruction, we need to restore the states before removing the last instruction
                        current_status.copy(block.status_after);
                        meet_requirements(drcontext, bb, instr, current_status, endstatus, false);
                    }
                    instrlist_remove(bb, instr);
                    instr_destroy(drcontext, instr);
                    instr=next_instr;
                }
                DR_ASSERT_MSG(instr==nullptr, "END OF BB WITHOUT FINDING NULL INSTR ");
            }
            
        }else
        {
            if(!block.instrumentable)
            {
                if(block_idx != 0) //We don't have to do anything for the first non instrumentable block
                {
                    meet_requirements(drcontext, bb, instr, current_status, block.requirements, block.instrumentable);
                    //context_modification_t modifs = meet_block_requirements(drcontext, bb, instr, current_register_statuses, block.requirements, false);
                    //update_status(current_register_statuses, modifs, false);
                }
                //We don't instrument it
                for(size_t i=0; i<block.numberOfInstructions; i++)
                {
                    //dr_print_instr(drcontext, STDOUT, instr, "");
                    instr=instr_get_next_app(instr);
                }
                if(block_idx != 0) //We don't have to do anything for the first non instrumentable block
                {
                    current_status.copy(block.status_after);
                }
            }else
            {
                meet_requirements(drcontext, bb, instr, current_status, block.requirements, block.instrumentable);
                for(size_t i=0; i<block.numberOfInstructions; i++)
                {
                    oc = ifp_get_operation_category(instr);
                    bool is_double = ifp_is_double(oc);
                    insert_set_destination_tls(drcontext, bb, instr, GET_REG(DST(instr, 0)));
                    insert_set_operands(drcontext, bb, instr, instr, oc);
                    if(i==0)
                    {
                        insert_restore_rsp(drcontext, bb, instr);
                        translate_insert(XINST_CREATE_sub(drcontext, OP_REG(DR_REG_XSP), OP_INT(32)), bb, instr);
                    }
                    
                    //dr_print_instr(drcontext, STDOUT, instr, "");
                    insert_call(drcontext, bb, instr, oc, is_double);
                    next_instr = instr_get_next_app(instr);
                    instrlist_remove(bb, instr);
                    instr_destroy(drcontext, instr);
                    instr=next_instr;
                }
                current_status.copy(block.status_after);
            }
        }
        
        
    }

    /*
    for(instr = instrlist_first_app(bb); instr != NULL; instr = next_instr)
    {
        oc = ifp_get_operation_category(instr);
        bool registers_saved=false;
        bool should_continue=false;
        do{
            next_instr = instr_get_next_app(instr);

            if(ifp_is_instrumented(oc)) {

                if(is_debug_enabled()) {
                    //dr_printf("%d ", nb++);
                    dr_print_instr(drcontext, STDOUT, instr , ": ");
                }

                bool is_double = ifp_is_double(oc);
                
                if(!registers_saved)
                {
                    insert_save_gpr_and_flags(drcontext, bb, instr);
                    insert_save_simd_registers(drcontext, bb, instr);
                }

                insert_set_destination_tls(drcontext, bb, instr, GET_REG(DST(instr, 0)));
                insert_set_operands(drcontext, bb, instr, instr, oc);
                //insert_restore_rsp(drcontext, bb, instr);
                if(!registers_saved)
                {
                    insert_restore_rsp(drcontext, bb, instr);
                    translate_insert(XINST_CREATE_sub(drcontext, OP_REG(DR_REG_XSP), OP_INT(32)), bb, instr);
                }
                registers_saved=true;
                insert_call(drcontext, bb, instr, oc, is_double);
                oc = ifp_get_operation_category(next_instr);
                should_continue = next_instr != nullptr && ifp_is_instrumented(oc);
                if(!should_continue)
                {
                    //It's not a floating point operation
                    insert_restore_simd_registers(drcontext, bb, instr);
                    insert_restore_gpr_and_flags(drcontext, bb, instr);
                }
                //dr_insert_clean_call(drcontext, bb, instr, (void*)print, true, 0);
                // Remove original instruction
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
                registers_saved=true;
            }
            instr = next_instr;
        }while(should_continue);
    }*/
    /* for(instr = instrlist_first(bb); instr != nullptr; instr=instr_get_next(instr))
    {
        dr_print_instr(drcontext, STDOUT, instr, " - ");
    }*/
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t symbol_lookup_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, OUT void** user_data)
{	
	bool already_found_fp_op = false;

	for(instr_t * instr = instrlist_first_app(bb); instr != NULL; instr = instr_get_next_app(instr))
	{
		OPERATION_CATEGORY oc = ifp_get_operation_category(instr);
		if(oc != IFP_UNSUPPORTED && oc != IFP_OTHER)
		{
			if(!already_found_fp_op)
			{
				already_found_fp_op = true;
				log_symbol(bb);
			}
			if(is_debug_enabled())
			{
				dr_print_instr(drcontext, STDERR, instr, "Found : ");
			}else
			{
				break;
			}
			
		}

	}
	return DR_EMIT_DEFAULT;
}
