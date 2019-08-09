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
    drreg_options.num_spill_slots = 5;
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
    */
    dr_printf("\n");
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

    byte rdi[8], rbp[8], rsp[8],rax[8], rsi[8], rbx[8], rdx[8], rcx[8],
         edi[4], ebp[4], esp[4], eax[4], esi[4], ebx[4], edx[4], ecx[4],
         xmm[16], xmm1[16], xmm2[16], xmm3[16], xmm4[16],
         ymm[16][32], gpr[16][8];
    
    reg_get_value_ex(DR_REG_RDI, &mcontext, rdi);
    reg_get_value_ex(DR_REG_RBP, &mcontext, rbp);
    reg_get_value_ex(DR_REG_RSP, &mcontext, rsp);
    reg_get_value_ex(DR_REG_RAX, &mcontext, rax);
    reg_get_value_ex(DR_REG_RSI, &mcontext, rsi);
    reg_get_value_ex(DR_REG_RBX, &mcontext, rbx);
    reg_get_value_ex(DR_REG_RDX, &mcontext, rdx);
    reg_get_value_ex(DR_REG_RCX, &mcontext, rcx);
    for(int i=0; i<16; i++)
    {
        reg_get_value_ex(DR_REG_START_YMM+i, &mcontext, ymm[i]);
        reg_get_value_ex(DR_REG_START_GPR+i, &mcontext, gpr[i]);
    }

    dr_printf("*****************************************************************************************************************************\n\n");
/*
    dr_printf("RDI : %02X \nEDI : %02X\nRDI : %e\n\n",*((uint64 *)rdi), *((unsigned int*)edi), *((double*)rdi));
    dr_printf("RAX : %02X \nEAX : %02X\nRAX : %e\n\n",*((uint64 *)rax), *((unsigned int*)eax), *((double*)rax));
    dr_printf("RBP : %02X \nEBP : %02X\nRBP : %e\n\n",*((uint64 *)rbp), *((unsigned int*)ebp), *((double*)rbp));    
    dr_printf("RSP : %02X \nESP : %02X\nRSP : %e\n\n",*((uint64 *)rsp), *((unsigned int*)esp), *((double*)rsp));
    dr_printf("RSI : %02X \nESI : %02X\nRSI : %e\n\n",*((uint64 *)rsi), *((unsigned int*)esi), *((double*)rsi));
    dr_printf("RBX : %02X \nEBX : %02X\nRBX : %e\n\n",*((uint64 *)rbx), *((unsigned int*)ebx), *((double*)rbx));
    dr_printf("RDX : %02X \nEDX : %02X\nRDX : %e\n\n",*((uint64 *)rdx), *((unsigned int*)edx), *((double*)rdx));
    dr_printf("RCX : %02X \nECX : %02X\nRCX : %e\n\n",*((uint64 *)rcx), *((unsigned int*)ecx), *((double*)rcx));
    */
    /*
    for(int i = 0 ; i < NB_XMM_REG ; i++) {
        reg_get_value_ex(XMM_REG[i], &mcontext, xmm);
        dr_printf("XMM%d : ",i);
        for(int k = 0 ; k < 4 ; k++) dr_printf("%e ",*(float*)&(xmm[4*k]));
        dr_printf("\n");
    }
    */
   unsigned long long int * tls_gpr = (unsigned long long int*)GET_TLS(dr_get_current_drcontext(), get_index_tls_gpr());
   for(int i=0; i<16; i++)
   {
       dr_printf("Context : %s %lu Saved as : %lu\n", get_register_name(DR_REG_START_GPR+i), *(unsigned long long int*)gpr[i], tls_gpr[i+1]);
   }
   for(int i=0; i<16; i++)
   {
       dr_printf("YMM%d\t%e %e %e %e\n", i, *(double*)&(ymm[i][0]), *(double*)&(ymm[i][8]), *(double*)&(ymm[i][16]), *(double*)&(ymm[i][24]));
   }
   /*
    dr_printf("YMM 0 : ");
    for(int i = 0 ; i < 4 ; i++) dr_printf("%e ",*(double*)&(ymm[8*i]));
    dr_printf("\n");

    dr_printf("YMM 2 : ");
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

    dr_printf("\n");*/
    dr_printf("*****************************************************************************************************************************\n\n");
}
#endif

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

enum register_status_t : uint{
    IFP_REG_CORRUPTED=32,
    IFP_REG_SAVED=1,
    IFP_REG_MODIFIED=2,
    IFP_REG_RESTORED=4,
    IFP_REG_MODIFIED_IN_MEMORY=8,
    IFP_REG_ORIGINAL=16,
    IFP_REG_VALID= IFP_REG_RESTORED | IFP_REG_ORIGINAL,
    IFP_REG_INVALID = IFP_REG_CORRUPTED | IFP_REG_MODIFIED_IN_MEMORY,
    IFP_REG_VALID_IN_MEMORY = IFP_REG_SAVED | IFP_REG_MODIFIED_IN_MEMORY,
    IFP_REG_INVALID_IN_MEMORY = IFP_REG_MODIFIED | IFP_REG_ORIGINAL
};

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
        for(size_t i; i<16; i++)
        {
            gpr_valid[i]=false;
            gpr_valid_in_memory[i]=false;
        }
        for(size_t i; i<32; i++)
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
        for(size_t i; i<16; i++)
        {
            gpr_valid[i]=cs.gpr_valid[i];
            gpr_valid_in_memory[i]=cs.gpr_valid_in_memory[i];
        }
        for(size_t i; i<32; i++)
        {
            simd_valid[i]=cs.simd_valid[i];
            simd_valid_in_memory[i]=cs.simd_valid_in_memory[i];
        }
        arith_flags_valid=cs.arith_flags_valid;
        arith_flags_valid_in_memory = cs.arith_flags_valid_in_memory;
    }
};


/*
struct register_requirement_t
{
    register_requirement_t(reg_id_t _reg, register_status_t _status): reg(_reg), status(_status){}
    bool operator==(reg_id_t _reg)const{return reg==_reg;}
    bool operator==(register_requirement_t _reg)const{return reg==_reg.reg;}
    reg_id_t reg;
    register_status_t status;
};

typedef register_requirement_t register_modification_t;

struct context_requirement_t
{
    context_requirement_t()=default;
    int get_idx_of(reg_id_t reg){
            return vec_idx_of(reg_is_gpr(reg) ? gpr : simd, register_requirement_t(reg, IFP_REG_VALID));
    }
    bool valid_arith_flags;
    std::vector<register_requirement_t> gpr;
    std::vector<register_requirement_t> simd;
};

struct context_modification_t
{
    context_modification_t()=default;
    bool modifies_arith_flags;
    std::vector<register_modification_t> gpr;
    std::vector<register_modification_t> simd;
};

struct sub_block{
    bool instrumentable;
    context_requirement_t requirements;
    context_modification_t modifications;
    uint64 numberOfInstructions;
    bool is_last;
};
*/

/*context_requirement_t get_backend_requirements()
{
    //TODO Get the actual required registers from library analysis

    //FIXME Currently treats every register as needed to be saved for the backend
    static context_requirement_t requirements;
    if(requirements.gpr.empty())
    {
        requirements.valid_arith_flags=false;
        for(ushort i=DR_REG_START_GPR; i<=DR_REG_STOP_GPR; i++)
        {
            requirements.gpr.emplace_back(i, IFP_REG_VALID_IN_MEMORY);
        }
        for(ushort i=DR_REG_START_XMM; i<= DR_REG_START_XMM+(AVX_512_SUPPORTED ? 31 : 15); i++)
        {
            requirements.simd.emplace_back(get_biggest_simd_version(i), IFP_REG_VALID_IN_MEMORY);
        }
    }
    return requirements;
}*/

context_status_t get_backend_requirements()
{
    context_status_t status;
    for(size_t i; i<16; i++)
    {
        status.gpr_valid_in_memory[i]=true;
        status.gpr_valid[i]=false;
    }
    status.set_gpr_valid(DR_REG_RSP, true);
    for(size_t i; i<(AVX_512_SUPPORTED ? 32 : 16); i++)
    {
        status.simd_valid_in_memory[i]=true;
        status.simd_valid[i]=false;
    }
    status.arith_flags_valid_in_memory=true;
    status.arith_flags_valid=false;
}

context_status_t get_backend_status_after()
{
    context_status_t status;
    for(size_t i; i<16; i++)
    {
        status.gpr_valid_in_memory[i]=true;
        status.gpr_valid[i]=false;
    }
    for(size_t i; i<(AVX_512_SUPPORTED ? 32 : 16); i++)
    {
        status.simd_valid_in_memory[i]=true;
        status.simd_valid[i]=false;
    }
    status.arith_flags_valid_in_memory=true;
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
            currBlock.status_after.copy(get_backend_status_after());
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
                currBlock.status_after.arith_flags_valid = currBlock.requirements.arith_flags_valid = ((instr_get_eflags(instr, DR_QUERY_INCLUDE_ALL) & (EFLAGS_READ_ARITH|EFLAGS_WRITE_ARITH)) != 0);
                if(((instr_get_eflags(instr, DR_QUERY_INCLUDE_ALL) & (EFLAGS_WRITE_ARITH)) != 0))
                {
                    currBlock.status_after.arith_flags_valid_in_memory=false;
                }
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

/*void meet_requirements(void* drc, instrlist_t* bb, instr_t* where, context_status_t & current, context_status_t const & desired, bool desired_instrumentable)
{
    std::vector<reg_id_t> gpr_needs_save;
    std::vector<reg_id_t> simd_needs_save;
    std::vector<reg_id_t> gpr_needs_restore;
    std::vector<reg_id_t> simd_needs_restore;
    bool flags_need_save=false;
    bool flags_need_restore=false;

    for(int i=0; i<16; i++)
    {
        if(desired.gpr_valid[i] && !current.gpr_valid[i])
        {
            DR_ASSERT_MSG(current.gpr)
        }
    }
    
}*/

/*
context_modification_t get_backend_modifications()
{
    //TODO Get the actual modified registers from library analysis

    //FIXME Currently treats every register as corrupted by the backend
    static context_modification_t modifications;
    if(modifications.gpr.empty())
    {
        modifications.modifies_arith_flags=true;
        for(ushort i=DR_REG_START_GPR; i<=DR_REG_STOP_GPR; i++)
        {
            modifications.gpr.emplace_back(i, IFP_REG_CORRUPTED);
        }
        for(ushort i=DR_REG_START_XMM; i<= DR_REG_START_XMM+(AVX_512_SUPPORTED ? 31 : 15); i++)
        {
            modifications.simd.emplace_back(get_biggest_simd_version(i), IFP_REG_CORRUPTED);
        }
    }
    return modifications;
}

static void add_or_modify(std::vector<register_requirement_t>& vec, register_requirement_t value)
{
    std::size_t i=vec_idx_of(vec, value);
    if(i==vec.size())
    {
        vec.push_back(register_requirement_t(value.reg, value.status));
    }else
    {
        vec[i].status = value.status;
    }
    
}*/
/*
static std::vector<sub_block> preanalysis(void *drcontext, void* tag, instrlist_t *bb)
{
    std::vector<sub_block> blocks;
    instr_t *instr, *next_instr;
    OPERATION_CATEGORY oc;
    bool shouldContinue;
    for(instr = instrlist_first_app(bb); instr != NULL; instr = next_instr){
        oc = ifp_get_operation_category(instr);
        sub_block currBlock; 
        currBlock.instrumentable = ifp_is_instrumented(oc);
        currBlock.numberOfInstructions=0;
        currBlock.is_last=false;
        currBlock.requirements.valid_arith_flags=false;
        currBlock.modifications.modifies_arith_flags=false;
        if(currBlock.instrumentable)
        {
            currBlock.requirements = get_backend_requirements();
            currBlock.modifications = get_backend_modifications();
        }
        shouldContinue=true;
        do
        {
            int numSrc = instr_num_srcs(instr);
            int numDst = instr_num_dsts(instr);

            for(int i=0; i < numSrc; i++)
            {
                opnd_t src = instr_get_src(instr, i);
                if(opnd_is_reg(src))
                {
                    reg_id_t reg = opnd_get_reg(src);
                    if(reg_is_gpr(reg))
                    {
                        add_or_modify(currBlock.requirements.gpr, register_requirement_t(get_biggest_gpr_version(reg), currBlock.instrumentable ? IFP_REG_VALID_IN_MEMORY : IFP_REG_VALID));
                    }else
                    {
                        add_or_modify(currBlock.requirements.simd, register_requirement_t(get_biggest_simd_version(reg), currBlock.instrumentable ? IFP_REG_VALID_IN_MEMORY : IFP_REG_VALID));
                    }
                    
                }else if(opnd_is_base_disp(src))
                {
                    reg_id_t base = opnd_get_base(src), index = opnd_get_index(src);
                    if(base != DR_REG_NULL)
                    {
                        add_or_modify(currBlock.requirements.gpr, register_requirement_t(get_biggest_gpr_version(base), currBlock.instrumentable ? IFP_REG_VALID_IN_MEMORY : IFP_REG_VALID));
                    }
                    if(index != DR_REG_NULL)
                    {
                        add_or_modify(currBlock.requirements.gpr, register_requirement_t(get_biggest_gpr_version(index), currBlock.instrumentable ? IFP_REG_VALID_IN_MEMORY : IFP_REG_VALID));
                    }
                }
            }
            for(int i=0; i<numDst; i++)
            {
                opnd_t dst = instr_get_dst(instr, i);
                if(opnd_is_reg(dst)){
                    reg_id_t reg = opnd_get_reg(dst);
                    if(currBlock.instrumentable){
                        add_or_modify(currBlock.requirements.simd, register_requirement_t(get_biggest_simd_version(reg), IFP_REG_VALID_IN_MEMORY));
                        add_or_modify(currBlock.modifications.simd, register_modification_t(get_biggest_simd_version(reg), IFP_REG_MODIFIED_IN_MEMORY));
                    }else
                    {
                        if(reg_is_gpr(reg))
                        {
                            add_or_modify(currBlock.modifications.gpr, register_modification_t(get_biggest_gpr_version(reg), IFP_REG_MODIFIED));
                        }else
                        {
                            add_or_modify(currBlock.modifications.simd, register_modification_t(get_biggest_simd_version(reg), IFP_REG_MODIFIED));
                        }
                        
                    }
                }else if(opnd_is_base_disp(dst))
                {
                    //Only in non instrumentable
                    reg_id_t base = opnd_get_base(dst), index = opnd_get_index(dst);
                    if(base != DR_REG_NULL)
                    {
                        add_or_modify(currBlock.requirements.gpr, register_requirement_t(get_biggest_gpr_version(base), IFP_REG_VALID));
                    }
                    if(index != DR_REG_NULL)
                    {
                        add_or_modify(currBlock.requirements.gpr, register_requirement_t(get_biggest_gpr_version(index), IFP_REG_VALID));
                    }
                }
            }
            if(!currBlock.instrumentable)
            {
                currBlock.requirements.valid_arith_flags |= ((instr_get_eflags(instr, DR_QUERY_INCLUDE_ALL) & (EFLAGS_READ_ARITH|EFLAGS_WRITE_ARITH)) != 0);
                currBlock.modifications.modifies_arith_flags |= ((instr_get_eflags(instr, DR_QUERY_INCLUDE_ALL) & (EFLAGS_WRITE_ARITH)) != 0);
            }
            currBlock.numberOfInstructions++;
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
            
        }while (next_instr != nullptr && shouldContinue);
        blocks.push_back(currBlock);
    }
    
    return blocks;
}*/
/*
static context_requirement_t get_context_full_of(register_status_t reg_status, bool arith_flags_valid)
{
    context_requirement_t status;
    for(ushort i=DR_REG_START_GPR; i<=DR_REG_STOP_GPR; i++)
    {
        status.gpr.emplace_back(i, reg_status);
    }
    for(ushort i=DR_REG_START_XMM; i<= DR_REG_START_XMM+(AVX_512_SUPPORTED ? 31 : 15); i++)
    {
        status.simd.emplace_back(get_biggest_simd_version(i), reg_status);
    }
    status.valid_arith_flags=arith_flags_valid;
    return status;
}
#define MINSERT(bb, where, instr) instrlist_meta_preinsert(bb, where, instr)
/**
 * \brief Does the necessary saves and restore so that the current status matches the desired status
 * 
 * \param current_status 
 * \param desired_status 
 */
/*static context_modification_t meet_block_requirements(void* drc, instrlist_t* bb, instr_t* where, context_requirement_t current_status, context_requirement_t desired_status, bool instrumentable)
{
    context_modification_t modifications;
    std::vector<reg_id_t> gpr_needs_save;
    std::vector<reg_id_t> simd_needs_save;
    std::vector<reg_id_t> gpr_needs_restore;
    std::vector<reg_id_t> simd_needs_restore;
    bool arith_flags_needs_restore=desired_status.valid_arith_flags && !current_status.valid_arith_flags;

    bool found_reg_for_tls=false;
    bool needs_spill_slot_restore=false;
    bool save_content_of_spill_slot=false;
    reg_id_t reg_for_tls;
    //GPR handling
    for(register_requirement_t rr : desired_status.gpr)
    {
        int i = current_status.get_idx_of(rr.reg);
        if(i < current_status.gpr.size())
        {
            register_requirement_t curr_reg_status = current_status.gpr[i];
            if(rr.status == IFP_REG_VALID_IN_MEMORY)
            {
                //We need it to be valid in memory
                if(curr_reg_status.status & IFP_REG_INVALID_IN_MEMORY)
                {
                    //Invalid in memory, we need to save it
                    gpr_needs_save.push_back(rr.reg);
                }else
                {
                    //It's valid in memory, we can use it as tls holder
                    if(!found_reg_for_tls){
                        reg_for_tls = rr.reg;
                        found_reg_for_tls=true;
                    }
                }
                
            }else if((rr.status == IFP_REG_VALID) && (curr_reg_status.status & IFP_REG_INVALID))
            {
                gpr_needs_restore.push_back(rr.reg);
                //Since we'll need to restore it anyway, we can use it as a tls holder
                if(!found_reg_for_tls){
                    reg_for_tls = rr.reg;
                    found_reg_for_tls=true;
                }
            }
        }
    }

    //SIMD handling
    for(register_requirement_t rr : desired_status.simd)
    {
        int i = current_status.get_idx_of(rr.reg);
        if(i < current_status.simd.size())
        {
            register_requirement_t curr_reg_status = current_status.simd[i];
            if((rr.status == IFP_REG_VALID_IN_MEMORY) && (curr_reg_status.status & IFP_REG_INVALID_IN_MEMORY))
            {
                simd_needs_save.push_back(rr.reg);
            }else if((rr.status == IFP_REG_VALID) && (curr_reg_status.status & IFP_REG_INVALID))
            {
                simd_needs_restore.push_back(rr.reg);
            }
        }
    }

    //Arith flags
    if(arith_flags_needs_restore || instrumentable)
    {
        int i= vec_idx_of(desired_status.gpr, register_requirement_t(DR_REG_RAX, IFP_REG_VALID));
        if(i < desired_status.gpr.size() && desired_status.gpr[i].status == IFP_REG_VALID && vec_idx_of(gpr_needs_restore, (reg_id_t)DR_REG_RAX) == gpr_needs_restore.size())
        {
            if((current_status.gpr[current_status.get_idx_of(DR_REG_RAX)].status & IFP_REG_VALID)&& vec_idx_of(gpr_needs_save, (reg_id_t)DR_REG_RAX) == gpr_needs_save.size())
                gpr_needs_save.push_back(DR_REG_RAX);
            gpr_needs_restore.push_back(DR_REG_RAX);
        }
    }

    if(instrumentable || arith_flags_needs_restore || !gpr_needs_restore.empty() || !gpr_needs_save.empty() || !simd_needs_restore.empty() || !simd_needs_save.empty())
    {
        //We need to move some stuff around
        if(!found_reg_for_tls)
        {
            //We need to find a suitable gpr to hold the tls adresses
            //Let's first try to find an invalid gpr that isn't restored
            for(register_requirement_t rr : current_status.gpr)
            {
                if((rr.status & IFP_REG_INVALID) && rr.reg != DR_REG_RAX)
                {
                    //If it's invalid, and isn't restored anyway (because otherwise we would have found the tls location before), we can use it
                    found_reg_for_tls=true;
                    reg_for_tls = rr.reg;
                    break;
                }
            }
            if(!found_reg_for_tls)
            {
                needs_spill_slot_restore=true;
                //Still not found it, so all gpr are valid, let's try to find a GPR that doesn't need to be saved right now
                for(register_requirement_t rr : current_status.gpr)
                {
                    if(vec_idx_of(gpr_needs_save, rr.reg) == gpr_needs_save.size())
                    {
                        found_reg_for_tls=true;
                        reg_for_tls = rr.reg;
                        break;
                    }
                }
                if(!found_reg_for_tls)
                {
                    //They all need to be saved, last resort so we need to save the used gpr in a spill slot and save it at the end
                    found_reg_for_tls=true;
                    save_content_of_spill_slot=true;
                    //We'll use the last gpr
                    reg_for_tls=DR_REG_STOP_GPR;
                }
                dr_save_reg(drc, bb, where, reg_for_tls, SPILL_SLOT_SCRATCH_REG);
            }
        }
        for(auto reg : gpr_needs_save)
        {
            dr_printf("%s ", get_register_name(reg));
        }
        for(auto reg : simd_needs_save)
        {
            dr_printf("%s ", get_register_name(reg));
        }
        for(auto reg : gpr_needs_restore)
        {
            dr_printf("%s ", get_register_name(reg));
        }
        for(auto reg : simd_needs_restore)
        {
            dr_printf("%s ", get_register_name(reg));
        }
        DR_ASSERT_MSG(found_reg_for_tls, "COULDN'T FIND A SUITABLE GPR TO HOLD TLS");
        INSERT_READ_TLS(drc, get_index_tls_float(), bb, where, reg_for_tls);
        //Let's handle the SIMD registers first
        for(reg_id_t simd : simd_needs_save)
        {
            MINSERT(bb, where, MOVE_FLOATING_PACKED(AVX_SUPPORTED, drc, OP_BASE_DISP(reg_for_tls, offset_of_simd(simd), reg_get_size(simd)), OP_REG(simd)));
            modifications.simd.emplace_back(simd, IFP_REG_SAVED);
        }
        for(reg_id_t simd : simd_needs_restore)
        {
            MINSERT(bb, where, MOVE_FLOATING_PACKED(AVX_SUPPORTED, drc, OP_REG(simd), OP_BASE_DISP(reg_for_tls, offset_of_simd(simd), reg_get_size(simd))));
            modifications.simd.emplace_back(simd, IFP_REG_RESTORED);
        }
        //Let's save the gpr
        INSERT_READ_TLS(drc, get_index_tls_gpr(), bb, where, reg_for_tls);
        for(reg_id_t gpr : gpr_needs_save)
        {
            if(gpr != reg_for_tls)
            {
                MINSERT(bb, where, XINST_CREATE_store(drc, OP_BASE_DISP(reg_for_tls, offset_of_gpr(gpr), OPSZ_8), OP_REG(gpr)));
                modifications.gpr.emplace_back(gpr, IFP_REG_SAVED);
            }
                
        }
        if(save_content_of_spill_slot)
        {
            dr_restore_reg(drc, bb, where, DR_REG_STOP_GPR-1, SPILL_SLOT_SCRATCH_REG);
            MINSERT(bb, where, XINST_CREATE_store(drc, OP_BASE_DISP(reg_for_tls, offset_of_gpr(reg_for_tls), OPSZ_8), OP_REG(DR_REG_STOP_GPR-1)));
            MINSERT(bb, where, XINST_CREATE_load(drc, OP_REG(DR_REG_STOP_GPR-1), OP_BASE_DISP(reg_for_tls, offset_of_gpr(DR_REG_STOP_GPR-1), OPSZ_8)));
            modifications.gpr.emplace_back(reg_for_tls, IFP_REG_SAVED);
            modifications.gpr.emplace_back(DR_REG_STOP_GPR-1, IFP_REG_RESTORED);
        }

        if(arith_flags_needs_restore)
        {
            MINSERT(bb, where, XINST_CREATE_load(drc, OP_REG(DR_REG_RAX), OP_BASE_DISP(reg_for_tls, 0, OPSZ_8)));
            MINSERT(bb, where, INSTR_CREATE_sahf(drc));
            modifications.modifies_arith_flags=true;
        }else if(instrumentable)
        {
            MINSERT(bb, where, INSTR_CREATE_lahf(drc));
            MINSERT(bb, where, XINST_CREATE_store(drc, OP_BASE_DISP(reg_for_tls, 0, OPSZ_8), OP_REG(DR_REG_RAX)));
            modifications.modifies_arith_flags=true;
        }

        for(reg_id_t gpr : gpr_needs_restore)
        {
            if(gpr != reg_for_tls)
            {
                MINSERT(bb, where, XINST_CREATE_load(drc, OP_REG(gpr), OP_BASE_DISP(reg_for_tls, offset_of_gpr(gpr), OPSZ_8)));
                modifications.gpr.emplace_back(gpr, IFP_REG_RESTORED);
            }
            
        }
        if(needs_spill_slot_restore)
        {
            dr_restore_reg(drc, bb, where, reg_for_tls, SPILL_SLOT_SCRATCH_REG);
            modifications.gpr.emplace_back(reg_for_tls, current_status.gpr[vec_idx_of(current_status.gpr, register_requirement_t(reg_for_tls, IFP_REG_VALID))].status);
        }else if(vec_idx_of(gpr_needs_restore, reg_for_tls) != gpr_needs_restore.size())
        {
            MINSERT(bb, where, XINST_CREATE_load(drc, OP_REG(reg_for_tls), OP_BASE_DISP(reg_for_tls, offset_of_gpr(reg_for_tls), OPSZ_8)));
            modifications.gpr.emplace_back(reg_for_tls, IFP_REG_RESTORED);
        }
    }
    return modifications;
}

void update_status(context_requirement_t & current_context, context_modification_t const & modifications, bool instrumentable)
{
    for(auto && rmod : modifications.gpr)
    {
        current_context.gpr.at(vec_idx_of(current_context.gpr, rmod)).status = rmod.status;
    }
    for(auto && rmod : modifications.simd)
    {
        current_context.simd.at(vec_idx_of(current_context.simd, rmod)).status = rmod.status;
    }
    if(!instrumentable && modifications.modifies_arith_flags)
    {
        current_context.valid_arith_flags=true;
    }else if(instrumentable)
    {
        current_context.valid_arith_flags=false;
    }
    //else unchanged

}

static std::string stringStatus(register_status_t status)
{
    switch(status)
    {
        case IFP_REG_CORRUPTED:
        return std::string("corrupted");
        case IFP_REG_INVALID:
        return std::string("invalid");
        case IFP_REG_INVALID_IN_MEMORY:
        return std::string("invalid in mem");
        case IFP_REG_MODIFIED:
        return std::string("modified");
        case IFP_REG_MODIFIED_IN_MEMORY:
        return std::string("modified in mem");
        case IFP_REG_ORIGINAL:
        return std::string("original");
        case IFP_REG_RESTORED:
        return std::string("restored");
        case IFP_REG_SAVED:
        return std::string("saved");
        case IFP_REG_VALID:
        return std::string("valid");
        case IFP_REG_VALID_IN_MEMORY:
        return std::string("valid in mem");
    }
}
*/
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

    /*
    std::vector<sub_block> blocks = preanalysis(drcontext, tag, bb);
    if(blocks.empty() || blocks.size() == 1 && !blocks[0].instrumentable)
    {
        //No instrumentable sub_block
        return DR_EMIT_DEFAULT;
    }

    static int nb=0;
    int instrCount=0;

    context_requirement_t current_register_statuses = get_context_full_of(IFP_REG_ORIGINAL, true);
    instr = instrlist_first_app(bb);

    for(int block_idx=0; block_idx<blocks.size(); ++block_idx)
    {
        sub_block block = blocks[block_idx];
        if(!block.instrumentable)
        {
            if(block_idx != 0) //We don't have to do anything for the first non instrumentable block
            {
                context_modification_t modifs = meet_block_requirements(drcontext, bb, instr, current_register_statuses, block.requirements, false);
                update_status(current_register_statuses, modifs, false);
            }
            //We don't instrument it
            for(int i=0; i<block.numberOfInstructions; i++)
            {
                next_instr=instr_get_next_app(instr);
                if(next_instr != nullptr)
                {
                    instr = instr_get_next_app(instr);
                }
            }
            if(block_idx != 0) //We don't have to do anything for the first non instrumentable block
            {
                update_status(current_register_statuses, block.modifications, false);
            }
        }else
        {
            context_modification_t modifs = meet_block_requirements(drcontext, bb, instr, current_register_statuses, block.requirements, true);

            update_status(current_register_statuses, modifs, true);
            for(int i=0; i<block.numberOfInstructions; i++)
            {
                oc = ifp_get_operation_category(instr);
                bool is_double = ifp_is_double(oc);
                insert_set_destination_tls(drcontext, bb, instr, GET_REG(DST(instr, 0)));
                insert_set_operands(drcontext, bb, instr, instr, oc);
                insert_restore_rsp(drcontext, bb, instr);
                translate_insert(XINST_CREATE_sub(drcontext, OP_REG(DR_REG_XSP), OP_INT(32)), bb, instr);
                insert_call(drcontext, bb, instr, oc, is_double);
                next_instr=instr_get_next_app(instr);
                if(next_instr != nullptr)
                {
                    instrlist_remove(bb, instr);
                    instr_destroy(drcontext, instr);
                    instr=next_instr;
                }
            }
            update_status(current_register_statuses, block.modifications, true);
            
        }
    }

    DR_ASSERT_MSG(instr != nullptr, "INSTR NULL");
    bool instrumentable = blocks[blocks.size()-1].instrumentable;
    meet_block_requirements(drcontext, bb, instr, current_register_statuses, get_context_full_of(IFP_REG_VALID, true), instrumentable);
    if(instrumentable){
        instrlist_remove(bb, instr);
        instr_destroy(drcontext, instr);
    }*/

    
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
                insert_restore_rsp(drcontext, bb, instr);
                if(!registers_saved)
                {
                    //insert_restore_rsp(drcontext, bb, instr);
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
    }
    
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
