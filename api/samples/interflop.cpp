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
    Interflop::verrou_prepare();

    if(analyse_config_from_args(argc, argv)){
		dr_abort_with_code(0);
		return;
	}
    if(is_debug()){
        print_register_vectors();
    }
	
    
    set_index_tls_result(drmgr_register_tls_field());
    set_index_tls_float(drmgr_register_tls_field());
    set_index_tls_gpr(drmgr_register_tls_field());
    //set_index_tls_stack(drmgr_register_tls_field());
    //set_index_tls_op_A(drmgr_register_tls_field());
    //set_index_tls_op_B(drmgr_register_tls_field());
    //set_index_tls_op_C(drmgr_register_tls_field());
    //set_index_tls_saved_reg(drmgr_register_tls_field());

    drmgr_register_thread_init_event(thread_init);
    drmgr_register_thread_exit_event(thread_exit);


    drreg_options_t drreg_options;
    drreg_options.conservative = true;
    drreg_options.num_spill_slots = 5;
    drreg_options.struct_size = sizeof(drreg_options_t);
    drreg_options.do_not_sum_slots=true;
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

static void event_exit()
{
	drmgr_unregister_tls_field(get_index_tls_result());
	drmgr_unregister_tls_field(get_index_tls_stack());

	if(get_client_mode() == IFP_CLIENT_GENERATE)
	{
		write_symbols_to_file();
	}
	drreg_exit();
	drmgr_exit();
	drsym_exit();
	drreg_exit();
	Interflop::verrou_end();
}

static void thread_init(void *dr_context) {
    SET_TLS(dr_context , get_index_tls_result() ,dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
    SET_TLS(dr_context, get_index_tls_float(), dr_thread_alloc(dr_context, 4096));
    SET_TLS(dr_context,get_index_tls_gpr(),dr_thread_alloc(dr_context, 4096));

    //SET_TLS(dr_context , get_index_tls_stack() , dr_thread_alloc(dr_context , SIZE_STACK*sizeof(SLOT)));
    //SET_TLS(dr_context , get_index_tls_op_A() , dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
    //SET_TLS(dr_context , get_index_tls_op_B() , dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
    //SET_TLS(dr_context , get_index_tls_op_C() , dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
    //SET_TLS(dr_context, get_index_tls_saved_reg(), dr_thread_alloc(dr_context, 64*3));
}

static void thread_exit(void *dr_context) {
    //dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_stack()) , SIZE_STACK*sizeof(SLOT));
    dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_result()) , MAX_OPND_SIZE_BYTES);
    dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_float()) , 4096);
    dr_thread_free(dr_context , GET_TLS(dr_context , get_index_tls_gpr()) , 4096);
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

struct register_requirement_t
{
    register_requirement_t(reg_id_t _reg, register_status_t _status): reg(_reg), status(_status){}
    bool operator==(reg_id_t _reg)const{return reg==_reg;}
    reg_id_t reg;
    register_status_t status;
};

typedef register_requirement_t register_modification_t;

struct sub_block{
    bool instrumentable;
    bool modifies_arith_flags;
    std::vector<register_requirement_t> requirements;
    std::vector<register_modification_t> modifications;
    uint64 numberOfInstructions;
    bool is_last;
};

std::vector<register_requirement_t> get_backend_requirements()
{
    //TODO Get the actual required registers from library analysis

    //FIXME Currently treats every register as needed to be saved for the backend
    static std::vector<register_modification_t> modifications;
    if(modifications.empty())
    {
        for(ushort i=DR_REG_START_GPR; i<=DR_REG_STOP_GPR; i++)
        {
            modifications.emplace_back(i, IFP_REG_VALID_IN_MEMORY);
        }
        for(ushort i=DR_REG_START_XMM; i<= DR_REG_START_XMM+(AVX_512_SUPPORTED ? 31 : 15); i++)
        {
            modifications.emplace_back(get_biggest_simd_version(i), IFP_REG_VALID_IN_MEMORY);
        }
    }
    return modifications;
}

std::vector<register_modification_t> get_backend_modifications()
{
    //TODO Get the actual modified registers from library analysis

    //FIXME Currently treats every register as corrupted by the backend
    static std::vector<register_modification_t> modifications;
    if(modifications.empty())
    {
        for(ushort i=DR_REG_START_GPR; i<=DR_REG_STOP_GPR; i++)
        {
            modifications.emplace_back(i, IFP_REG_CORRUPTED);
        }
        for(ushort i=DR_REG_START_XMM; i<= DR_REG_START_XMM+(AVX_512_SUPPORTED ? 31 : 15); i++)
        {
            modifications.emplace_back(get_biggest_simd_version(i), IFP_REG_CORRUPTED);
        }
    }
    return modifications;
}

static void add_or_modify(std::vector<register_requirement_t>& vec, register_requirement_t && value)
{
    std::size_t i=0;
    for(; i<vec.size(); i++)
    {
        if(vec[i].reg == value.reg)
        {
            vec[i].status = value.status;
            break;
        }
    }
    if(i==vec.size())
    {
        vec.push_back(value);
    }
}

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
        currBlock.modifies_arith_flags = currBlock.instrumentable;
        currBlock.numberOfInstructions=0;
        currBlock.is_last=false;
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
                    //currBlock.requirements.push_back(register_requirement_t(opnd_get_reg(src), currBlock.instrumentable ? IFP_REG_VALID_IN_MEMORY : IFP_REG_VALID));
                    add_or_modify(currBlock.requirements, register_requirement_t(opnd_get_reg(src), currBlock.instrumentable ? IFP_REG_VALID_IN_MEMORY : IFP_REG_VALID));
                }else if(opnd_is_base_disp(src))
                {
                    reg_id_t base = opnd_get_base(src), index = opnd_get_index(src);
                    if(base != DR_REG_NULL)
                    {
                        add_or_modify(currBlock.requirements, register_requirement_t(base, currBlock.instrumentable ? IFP_REG_VALID_IN_MEMORY : IFP_REG_VALID));
                    }
                    if(index != DR_REG_NULL)
                    {
                        add_or_modify(currBlock.requirements, register_requirement_t(index, currBlock.instrumentable ? IFP_REG_VALID_IN_MEMORY : IFP_REG_VALID));
                    }
                }
            }
            for(int i=0; i<numDst; i++)
            {
                opnd_t dst = instr_get_dst(instr, i);
                if(opnd_is_reg(dst)){
                    if(currBlock.instrumentable){
                        add_or_modify(currBlock.requirements, register_requirement_t(opnd_get_reg(dst), IFP_REG_VALID_IN_MEMORY));
                        add_or_modify(currBlock.modifications, register_modification_t(opnd_get_reg(dst), IFP_REG_MODIFIED_IN_MEMORY));
                    }else
                    {
                        add_or_modify(currBlock.modifications, register_modification_t(opnd_get_reg(dst), IFP_REG_MODIFIED));
                    }
                }else if(opnd_is_base_disp(dst))
                {
                    //Only in non instrumentable
                    reg_id_t base = opnd_get_base(dst), index = opnd_get_index(dst);
                    if(base != DR_REG_NULL)
                    {
                        add_or_modify(currBlock.requirements, register_requirement_t(base, IFP_REG_VALID));
                    }
                    if(index != DR_REG_NULL)
                    {
                        add_or_modify(currBlock.requirements, register_requirement_t(index, IFP_REG_VALID));
                    }
                }
            }
            if(!currBlock.instrumentable)
            {
                currBlock.modifies_arith_flags |= ((instr_get_eflags(instr, DR_QUERY_INCLUDE_ALL) & (EFLAGS_READ_ARITH|EFLAGS_WRITE_ARITH)) != 0);
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
}

static std::vector<register_requirement_t> get_original_status_vector()
{
    std::vector<register_requirement_t> status;
    for(ushort i=DR_REG_START_GPR; i<=DR_REG_STOP_GPR; i++)
    {
        status.emplace_back(i, IFP_REG_ORIGINAL);
    }
    for(ushort i=DR_REG_START_XMM; i<= DR_REG_START_XMM+(AVX_512_SUPPORTED ? 31 : 15); i++)
    {
        status.emplace_back(get_biggest_simd_version(i), IFP_REG_ORIGINAL);
    }
    return status;
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

    
    std::vector<sub_block> blocks = preanalysis(drcontext, tag, bb);
    if(blocks.empty() || blocks.size() == 1 && !blocks[0].instrumentable)
    {
        //No instrumentable sub_block
        return DR_EMIT_DEFAULT;
    }

    static int nb=0;
    int block_idx=0, instrCount=0;

    
    for(instr = instrlist_first_app(bb); instr != NULL; instr = next_instr)
    {
        sub_block currBlock = blocks[block_idx];
        oc = ifp_get_operation_category(instr);
        bool registers_saved=false;
        bool should_continue=false;
        do{
            next_instr = instr_get_next_app(instr);

            if(ifp_is_instrumented(oc)) {

                if(is_debug()) {
                    dr_printf("%d ", nb++);
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
			if(is_debug())
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
