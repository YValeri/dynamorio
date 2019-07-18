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
#define MAX_OPND_SIZE_BYTES 512 
#endif

#define ERROR(message) dr_fprintf(STDERR, "%s\n" , (message));

#define DR_REG_SRC_0 DR_REG_XMM0
#define DR_REG_SRC_1 DR_REG_XMM1

#define DR_BUFFER_REG DR_REG_XCX
#define DR_SCRATCH_REG DR_REG_XDX

#define SPILL_SLOT_BUFFER_REG SPILL_SLOT_1
#define SPILL_SLOT_SCRATCH_REG SPILL_SLOT_2
#define SPILL_SLOT_ARITH_FLAG SPILL_SLOT_3

#define SRC(instr,n) instr_get_src((instr),(n))
#define DST(instr,n) instr_get_dst((instr),(n))

#define OP_REG(reg_id) opnd_create_reg((reg_id))
#define OP_INT(val) opnd_create_immed_int((val) , OPSZ_4)
#define OP_BASE_DISP(base,disp,size) opnd_create_base_disp((base) , DR_REG_NULL , 0 , (disp) , (size))
#define OP_REL_ADDR(addr) opnd_create_rel_addr((addr) , OPSZ_8)


#define IS_REG(opnd) opnd_is_reg((opnd))
#define GET_REG(opnd) opnd_get_reg((opnd))
#define OPSZ(bytes) opnd_size_from_bytes((bytes))

#define IS_GPR(reg) reg_is_gpr((reg))
#define IS_XMM(reg) reg_is_strictly_xmm((reg))
#define IS_YMM(reg) reg_is_strictly_ymm((reg))
#define IS_ZMM(reg) reg_is_strictly_zmm((reg))
#define OP_IS_BASE_DISP(opnd) opnd_is_base_disp((opnd))

#define MOVE_FLOATING(is_double, drcontext , dest , srcd , srcs) (is_double) ? INSTR_CREATE_movsd((drcontext) , (dest) , (srcd)) : INSTR_CREATE_movss((drcontext) , (dest), (srcs))

#define GET_TLS(drcontext,tls) drmgr_get_tls_field((drcontext) , (tls))
#define SET_TLS(drcontext,tls,value) drmgr_set_tls_field((drcontext) , (tls) , (void*)(value))
#define INSERT_READ_TLS(drcontext , tls , bb , instr , reg) drmgr_insert_read_tls_field((drcontext) , (tls) , (bb) , (instr) , (reg))
#define INSERT_WRITE_TLS(drcontext , tls , bb , instr , reg , temp_reg) drmgr_insert_write_tls_field((drcontext) , (tls_stack) , (bb) , (instr) , (buffer_reg) , (temp_reg));

#define SIZE_STACK 2048
#define PTR_SIZE sizeof(void*)
#define FLOAT_SIZE sizeof(float)
#define DOUBLE_SIZE sizeof(double)
#define REG_SIZE(reg) opnd_size_in_bytes(reg_get_size((reg)))

typedef byte SLOT;

int tls_result /* index of thread local storage to store the result of floating point operations */, 
    tls_op_A, tls_op_B /* index of thread local storage to store the operands of vectorial floating point operations */,
    tls_stack /* index of thread local storage to store the address of the shallow stack */;

// Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);     

static dr_emit_flags_t runtime(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, void **user_data);
                                        
static void event_exit(void);

static void thread_init(void *drcontext);

static void thread_exit(void *drcontext);

static inline void insert_push_pseudo_stack(void *drcontext , reg_id_t reg_to_push, instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg, reg_id_t temp_buf);
static inline void insert_pop_pseudo_stack(void *drcontext , reg_id_t reg, instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg, reg_id_t temp_buf);

static inline void insert_push_pseudo_stack_list(void *drcontext , reg_id_t *reg_to_push_list , _instr_list_t *bb , instr_t *instr , reg_id_t buffer_reg , reg_id_t temp_buf , unsigned int nb_reg);
static inline void insert_pop_pseudo_stack_list(void *drcontext , reg_id_t *reg_to_pop_list , _instr_list_t *bb , instr_t *instr , reg_id_t buffer_reg , reg_id_t temp_buf , unsigned int nb_reg);


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

    drmgr_register_bb_instrumentation_event(runtime,NULL,NULL);

    interflop_verrou_configure(VR_RANDOM , nullptr);

    
    tls_result = drmgr_register_tls_field();
    tls_stack = drmgr_register_tls_field();
    tls_op_A = drmgr_register_tls_field();
    tls_op_B = drmgr_register_tls_field();

    drmgr_register_thread_init_event(thread_init);
    drmgr_register_thread_exit_event(thread_exit);


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
    drmgr_unregister_tls_field(tls_result);
    drmgr_unregister_tls_field(tls_stack);

    drmgr_exit();
    drreg_exit();
}

static void thread_init(void *dr_context) {
    SET_TLS(dr_context , tls_result ,dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
    SET_TLS(dr_context , tls_stack , dr_thread_alloc(dr_context , SIZE_STACK*sizeof(SLOT)));
    SET_TLS(dr_context , tls_op_A , dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
    SET_TLS(dr_context , tls_op_B , dr_thread_alloc(dr_context , MAX_OPND_SIZE_BYTES));
}

static void thread_exit(void *dr_context) {
    dr_thread_free(dr_context , GET_TLS(dr_context , tls_stack) , SIZE_STACK*sizeof(SLOT));
    dr_thread_free(dr_context , GET_TLS(dr_context , tls_result) , MAX_OPND_SIZE_BYTES);
    dr_thread_free(dr_context , GET_TLS(dr_context , tls_op_A) , MAX_OPND_SIZE_BYTES);
    dr_thread_free(dr_context , GET_TLS(dr_context , tls_op_B) , MAX_OPND_SIZE_BYTES);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################


template <typename FTYPE , FTYPE (*FN)(FTYPE, FTYPE) , int SIMD_TYPE = IFP_OP_SCALAR>
struct interflop_backend {

    static void apply(FTYPE a,  FTYPE b) {
        // ***** Call to backend and get the result *****
        FTYPE res = FN(a,b);

        // ***** Copy the result in thread local memory *****
        *((FTYPE*)GET_TLS(dr_get_current_drcontext() , tls_result)) = res;
    }

    static void apply_vect(FTYPE *vect_a,  FTYPE *vect_b) {
       
        int vect_size = (SIMD_TYPE == IFP_OP_128) ? 16 : (SIMD_TYPE == IFP_OP_256) ? 32 : (SIMD_TYPE == IFP_OP_512) ? 64 : 0;
        int nb_elem = vect_size/sizeof(FTYPE);
        
        FTYPE res;
        
        for(int i = 0 ; i < nb_elem ; i++) {
            res = FN(vect_a[i],vect_b[i]);
            *(((FTYPE*)GET_TLS(dr_get_current_drcontext() , tls_result))+i) = res;
        }
       
       /*
        dr_printf("A : %p\nB : %p\n",vect_a , vect_b);

        dr_printf("A : ");
        for(int i = 0 ; i < 4 ; i++) dr_printf("%d ",(int)(*((double*)(vect_a)+i)));
        dr_printf("\n");
        dr_printf("B : ");
        for(int i = 0 ; i < 4 ; i++) dr_printf("%d ",(int)(*((double*)(vect_b)+i)));
        dr_printf("\n");
        

        dr_printf("RES : ");
        for(int i = 0 ; i < 4 ; i++) dr_printf("%d ",(int)(*((FTYPE*)(GET_TLS(dr_get_current_drcontext(), tls_result))+i)));
        dr_printf("\n");
        */
    }


};


//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

static void print() {
    void *context = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.flags = DR_MC_ALL;
    mcontext.size = sizeof(mcontext);

    dr_get_mcontext(context , &mcontext);

    byte rdi[8] , rbp[8], rsp[8],rax[8] , rsi[8], rbx[8] , rdx[8], rcx[8],
         edi[4] , ebp[4] , esp[4] , eax[4] , esi[4] , ebx[4] , edx[4] , ecx[4],
         xmm[16] , xmm1[16], xmm2[16], xmm3[16] , xmm4[16],
         ymm[32] , ymm1[32];
    
    reg_get_value_ex(DR_REG_RDI , &mcontext , rdi);
    reg_get_value_ex(DR_REG_RBP , &mcontext , rbp);
    reg_get_value_ex(DR_REG_RSP , &mcontext , rsp);
    reg_get_value_ex(DR_REG_RAX , &mcontext , rax);
    reg_get_value_ex(DR_REG_RSI , &mcontext , rsi);
    reg_get_value_ex(DR_REG_RBX , &mcontext , rbx);
    reg_get_value_ex(DR_REG_RDX , &mcontext , rdx);
    reg_get_value_ex(DR_REG_RCX , &mcontext , rcx);

    reg_get_value_ex(DR_REG_EDI , &mcontext , edi);
    reg_get_value_ex(DR_REG_EBP , &mcontext , ebp);
    reg_get_value_ex(DR_REG_ESP , &mcontext , esp);
    reg_get_value_ex(DR_REG_EAX , &mcontext , eax);
    reg_get_value_ex(DR_REG_ESI , &mcontext , esi);
    reg_get_value_ex(DR_REG_EBX , &mcontext , ebx);
    reg_get_value_ex(DR_REG_EDX , &mcontext , edx);
    reg_get_value_ex(DR_REG_ECX , &mcontext , ecx);

    reg_get_value_ex(DR_REG_XMM0 , &mcontext , xmm);
    reg_get_value_ex(DR_REG_XMM1 , &mcontext , xmm1);
    reg_get_value_ex(DR_REG_XMM2 , &mcontext , xmm2);
    reg_get_value_ex(DR_REG_XMM3 , &mcontext , xmm3);
    reg_get_value_ex(DR_REG_XMM4 , &mcontext , xmm4);

    reg_get_value_ex(DR_REG_YMM0 , &mcontext , ymm);
    reg_get_value_ex(DR_REG_YMM1 , &mcontext , ymm1);

    dr_printf("*****************************************************************************************************************************\n\n");

    dr_printf("RDI : %02X \nEDI : %02X\nRDI : %e\n\n",*((uint64 *)rdi) , *((unsigned int*)edi), *((double*)rdi));
    dr_printf("RAX : %02X \nEAX : %02X\nRAX : %e\n\n",*((uint64 *)rax) , *((unsigned int*)eax), *((double*)rax));
    dr_printf("RBP : %02X \nEBP : %02X\nRBP : %e\n\n",*((uint64 *)rbp) , *((unsigned int*)ebp), *((double*)rbp));    
    dr_printf("RSP : %02X \nESP : %02X\nRSP : %e\n\n",*((uint64 *)rsp) , *((unsigned int*)esp), *((double*)rsp));
    dr_printf("RSI : %02X \nESI : %02X\nRSI : %e\n\n",*((uint64 *)rsi) , *((unsigned int*)esi), *((double*)rsi));
    dr_printf("RBX : %02X \nEBX : %02X\nRBX : %e\n\n",*((uint64 *)rbx) , *((unsigned int*)ebx), *((double*)rbx));
    dr_printf("RDX : %02X \nEDX : %02X\nRDX : %e\n\n",*((uint64 *)rdx) , *((unsigned int*)edx), *((double*)rdx));
    dr_printf("RCX : %02X \nECX : %02X\nRCX : %e\n\n",*((uint64 *)rcx) , *((unsigned int*)ecx), *((double*)rcx));

    dr_printf("XMM 0 : ");
    for(int i = 0 ; i < 2 ; i++) dr_printf("%e ",*(double*)&(xmm[8*i]));
    dr_printf("\n");

    dr_printf("XMM 1 : ");
    for(int i = 0 ; i < 2 ; i++) dr_printf("%e ",*(double*)&(xmm1[8*i]));
    dr_printf("\n");

    dr_printf("XMM 2 : ");
    for(int i = 0 ; i < 2 ; i++) dr_printf("%e ",*(double*)&(xmm2[8*i]));
    dr_printf("\n");

    dr_printf("XMM 3 : ");
    for(int i = 0 ; i < 2 ; i++) dr_printf("%e ",*(double*)&(xmm3[8*i]));
    dr_printf("\n");

    dr_printf("XMM 4 : ");
    for(int i = 0 ; i < 2 ; i++) dr_printf("%e ",*(double*)&(xmm4[8*i]));
    dr_printf("\n");

    dr_printf("YMM 0 : ");
    for(int i = 0 ; i < 4 ; i++) dr_printf("%e ",*(double*)&(ymm[8*i]));
    dr_printf("\n");

    dr_printf("YMM 1 : ");
    for(int i = 0 ; i < 4 ; i++) dr_printf("%e ",*(double*)&(ymm1[8*i]));
    dr_printf("\n");
    
    dr_printf("TLS STACK : %d\t%p\n",tls_stack,GET_TLS(context,tls_stack));
    dr_printf("TLS RESULT : %d\t%p\n",tls_result,GET_TLS(context,tls_result));
    dr_printf("TLS A : %d\t%p\n",tls_op_A,GET_TLS(context,tls_op_A));
    dr_printf("TLS B : %d\t%p\n",tls_op_B,GET_TLS(context,tls_op_B));

    dr_printf("OPERAND A : ");
    for(int i = 0 ; i < 4 ; i++) dr_printf("%d ",(int)(*((double*)(GET_TLS(context,tls_op_A))+i)));
    dr_printf("\n");
    dr_printf("OPERAND B : ");
    for(int i = 0 ; i < 4 ; i++) dr_printf("%d ",(int)(*((double*)(GET_TLS(context,tls_op_B))+i)));
    dr_printf("\n");

    dr_printf("\n");
    dr_printf("*****************************************************************************************************************************\n\n");
}

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

static inline void insert_pop_pseudo_stack(void *drcontext , reg_id_t reg, instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg, reg_id_t temp_buf) {
    
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    // ****************************************************************************
    // Decrement the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_sub(drcontext , OP_REG(buffer_reg) , OP_INT(REG_SIZE(reg))) , bb , instr);

    // ****************************************************************************
    // Load the value in the defined register
    // ****************************************************************************
    if(IS_GPR(reg))
        translate_insert(XINST_CREATE_load(drcontext, OP_REG(reg) , OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg))) , bb , instr); 
    else if(IS_XMM(reg))
        translate_insert(INSTR_CREATE_movupd(drcontext, OP_REG(reg), OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg))) , bb , instr);
    else if(IS_YMM(reg) || IS_ZMM(reg))
            translate_insert(INSTR_CREATE_vmovupd(drcontext, OP_REG(reg), OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg))) , bb , instr);
    else 
        dr_fprintf(STDERR, "LOAD ERROR\n");

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

static inline void insert_pop_pseudo_stack_list(void *drcontext , reg_id_t *reg_to_pop_list , _instr_list_t *bb , instr_t *instr , reg_id_t buffer_reg , reg_id_t temp_buf , unsigned int nb_reg) {
    
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    int offset = 0;

    for(unsigned int i = 0 ; i < nb_reg ; i++) {
        offset -= REG_SIZE(reg_to_pop_list[i]);
        if(IS_GPR(reg_to_pop_list[i]))
            translate_insert(XINST_CREATE_load(drcontext, OP_REG(reg_to_pop_list[i]) , OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_pop_list[i]))) , bb , instr); 
        else if(IS_XMM(reg_to_pop_list[i]))
            translate_insert(INSTR_CREATE_movupd(drcontext, OP_REG(reg_to_pop_list[i]), OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_pop_list[i]))) , bb , instr);
        else if(IS_YMM(reg_to_pop_list[i]) || IS_ZMM(reg_to_pop_list[i]))
            translate_insert(INSTR_CREATE_vmovupd(drcontext, OP_REG(reg_to_pop_list[i]), OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_pop_list[i]))) , bb , instr);
        else 
            dr_fprintf(STDERR, "LOAD ERROR\n");

    }

    // ****************************************************************************
    // Decrement the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_add(drcontext , OP_REG(buffer_reg) , OP_INT(offset)) , bb , instr);

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

static inline void insert_push_pseudo_stack(void *drcontext , reg_id_t reg_to_push, instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg, reg_id_t temp_buf) {
 
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    // ****************************************************************************
    // Store the value of the register to save
    // ****************************************************************************
    if(IS_GPR(reg_to_push))
        translate_insert(XINST_CREATE_store(drcontext , OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg_to_push)) , OP_REG(reg_to_push)) , bb , instr); 
    else if(IS_XMM(reg_to_push))
        translate_insert(INSTR_CREATE_movupd(drcontext, OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg_to_push)), OP_REG(reg_to_push)) , bb , instr);
    else if(IS_YMM(reg_to_push) || IS_ZMM(reg_to_push))
        translate_insert(INSTR_CREATE_vmovupd(drcontext, OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg_to_push)), OP_REG(reg_to_push)) , bb , instr);
    else 
        dr_fprintf(STDERR, "STORE ERROR\n");

    // ****************************************************************************
    // Increment the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_add(drcontext , OP_REG(buffer_reg) , OP_INT(REG_SIZE(reg_to_push))) , bb , instr); 

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

static inline void insert_push_pseudo_stack_list(void *drcontext , reg_id_t *reg_to_push_list , _instr_list_t *bb , instr_t *instr , reg_id_t buffer_reg , reg_id_t temp_buf , unsigned int nb_reg) {
    
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    int offset=0;

    for(unsigned int i = 0 ; i < nb_reg ; i++) {
        if(IS_GPR(reg_to_push_list[i]))
            translate_insert(XINST_CREATE_store(drcontext , OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_push_list[i])) , OP_REG(reg_to_push_list[i])) , bb , instr); 
        else if(IS_XMM(reg_to_push_list[i]))
            translate_insert(INSTR_CREATE_movupd(drcontext, OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_push_list[i])),  OP_REG(reg_to_push_list[i])) , bb , instr);
        else if(IS_YMM(reg_to_push_list[i]) || IS_ZMM(reg_to_push_list[i]))
            translate_insert(INSTR_CREATE_vmovupd(drcontext, OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_push_list[i])),  OP_REG(reg_to_push_list[i])) , bb , instr);
        else
            dr_fprintf(STDERR, "STORE ERROR\n");

        offset += REG_SIZE(reg_to_push_list[i]);
    }

    // ****************************************************************************
    // Increment the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_add(drcontext , OP_REG(buffer_reg) , OP_INT(offset)) , bb , instr); 

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################


template <typename FTYPE , FTYPE (*FN)(FTYPE, FTYPE)>
inline void insert_corresponding_vect_call(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    switch(oc & IFP_SIMD_TYPE_MASK)
    {
        case IFP_OP_128:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend<FTYPE, FN, IFP_OP_128>::apply_vect , 0);
        break;
        case IFP_OP_256:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend<FTYPE, FN, IFP_OP_256>::apply_vect , 0);
        break;
        case IFP_OP_512:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend<FTYPE, FN, IFP_OP_512>::apply_vect , 0);
        break;
        default:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend<FTYPE, FN>::apply_vect , 0);
    }
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

static dr_emit_flags_t event_basic_block(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    instr_t *instr2 , *next_instr2;
    OPERATION_CATEGORY oc;

    bool display = false;
    bool display_after = false;
    bool saveSRC0 = false, saveSRC1 = false, saveYMM0 = false, saveYMM1 = false;

    for(instr = instrlist_first(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next(instr);

        oc = ifp_get_operation_category(instr);

        if(oc)
        {
            bool is_double = ifp_is_double(oc);
            bool is_scalar = ifp_is_scalar(oc);

            /*
            dr_print_instr(drcontext, STDERR, instr , "II : ");
            dr_print_opnd(drcontext , STDERR , SRC(instr,0) , "SRC 0 : ");
            dr_print_opnd(drcontext , STDERR , SRC(instr,1) , "SRC 1 : ");
            */
            if(display) {
                display = false;
                dr_printf("\n");
                for(instr2 = instrlist_first(bb); instr2 != NULL; instr2 = next_instr2) {
                    next_instr2 = instr_get_next(instr2);
                    dr_print_instr(drcontext , STDERR , instr2 , "INSTR : ");
                }
            }

            // ****************************************************************************
            // PRINT BEFORE
            // ****************************************************************************            
            //dr_insert_clean_call(drcontext , bb , instr , (void*)print , false , 0);
            // ****************************************************************************

            // ****************************************************************************
            // Reserve two registers among all registers available except RDIk
            // ****************************************************************************
            reg_id_t buffer_reg  = DR_BUFFER_REG, 
                     scratch     = DR_SCRATCH_REG;

            dr_save_reg(drcontext , bb , instr , buffer_reg , SPILL_SLOT_BUFFER_REG);
            dr_save_reg(drcontext , bb , instr , scratch , SPILL_SLOT_SCRATCH_REG);
        
            // ****************************************************************************
            // save processor flags
            // ****************************************************************************
            dr_save_arith_flags(drcontext , bb , instr , SPILL_SLOT_ARITH_FLAG);

            // ****************************************************************************
            // push general purpose registers on pseudo stack 
            // All GPR except DR_BUFFER_REG and DR_SCRATCH_REG
            // ****************************************************************************
            reg_id_t topush_reg[] = {DR_REG_XDI , DR_REG_XSI , DR_REG_XAX , DR_REG_XBP , DR_REG_XSP , DR_REG_XBX};
            insert_push_pseudo_stack_list(drcontext , topush_reg , bb , instr , buffer_reg , scratch , 6);
        

            if(is_scalar) { /* SCALAR */

                // ****************************************************************************
                // pass arguments using XMM registers for floating point operands 
                // ****************************************************************************
                
                // Handle the case where the first oeprand is the register SRC_0
                if(IS_REG(SRC(instr,0)) && IS_REG(SRC(instr,1)) && GET_REG(SRC(instr,0)) == DR_REG_SRC_0) {
                    
                    saveSRC0 = true;
                    saveSRC1 = true;

                    insert_push_pseudo_stack(drcontext , DR_REG_SRC_0 , bb , instr , buffer_reg , scratch);
                    insert_push_pseudo_stack(drcontext , DR_REG_SRC_1 , bb , instr , buffer_reg , scratch);

                    if(GET_REG(SRC(instr,1)) == DR_REG_SRC_1) {
                        // Swap SRC_0 and SRC_1
                        translate_insert(INSTR_CREATE_pxor(drcontext , OP_REG(DR_REG_SRC_0) , OP_REG(DR_REG_SRC_1)) , bb , instr);
                        translate_insert(INSTR_CREATE_pxor(drcontext , OP_REG(DR_REG_SRC_1) , OP_REG(DR_REG_SRC_0)) , bb , instr);
                        translate_insert(INSTR_CREATE_pxor(drcontext , OP_REG(DR_REG_SRC_0) , OP_REG(DR_REG_SRC_1)) , bb , instr);
                    }
                    else {
                        translate_insert(MOVE_FLOATING(is_double , drcontext , OP_REG(DR_REG_SRC_1) , SRC(instr,0), SRC(instr,0)) , bb , instr);
                        translate_insert(MOVE_FLOATING(is_double , drcontext , OP_REG(DR_REG_SRC_0) , SRC(instr,1), SRC(instr,1)) , bb , instr);
                    }
                }
                else {
                    // ***** First operand in XMM0 *****
                    if(!IS_REG(SRC(instr,1)) || GET_REG(SRC(instr,1)) != DR_REG_SRC_0) {
                        saveSRC0 = true;

                        // Save current value of XMM0 in pseudo stack
                        insert_push_pseudo_stack(drcontext , DR_REG_SRC_0 , bb , instr , buffer_reg , scratch);

                        // Set the XMM register with the operand floating point value 
                        translate_insert(MOVE_FLOATING(is_double , drcontext , OP_REG(DR_REG_SRC_0) , SRC(instr,1), SRC(instr,1)) , bb , instr);
                    }

                    // ***** Second operand in XMM1 *****
                    if(!IS_REG(SRC(instr,0)) || GET_REG(SRC(instr,0)) != DR_REG_SRC_1) {
                        saveSRC1 = true;

                        // Save current value of XMM1 in pseudo stack
                        insert_push_pseudo_stack(drcontext , DR_REG_SRC_1 , bb , instr , buffer_reg , scratch);
                    
                        // Set the XMM register with the operand floating point value 
                        translate_insert(MOVE_FLOATING(is_double , drcontext , OP_REG(DR_REG_SRC_1) , SRC(instr,0) , SRC(instr,0)) , bb , instr);
                    }
                }
            

                // ***** Sub stack pointer to handle the case where XSP is equal to XBP and XSP doesn't match the top of the stack *****
                // ***** Otherwise the call will erase data when pushing the return address *****
                // ***** If the gap is greater than 32 bytes, the program may crash !!!!!!!!!!!!!!! *****
                translate_insert(INSTR_CREATE_sub(drcontext , OP_REG(DR_REG_XSP) , OP_INT(32)) , bb , instr);
                
                // ****************************************************************************
                // ***** SCALAR CALL *****
                // ****************************************************************************
                switch (oc & IFP_OP_TYPE_MASK)
                {
                    case IFP_OP_ADD:
                        dr_insert_call(drcontext , bb , instr ,is_double ?  (void*)interflop_backend<double,Interflop::Op<double>::add>::apply : (void*)interflop_backend<float,Interflop::Op<float>::add>::apply , 0);
                    break;
                    case IFP_OP_SUB:
                        dr_insert_call(drcontext , bb , instr ,is_double ?  (void*)interflop_backend<double,Interflop::Op<double>::sub>::apply : (void*)interflop_backend<float,Interflop::Op<float>::sub>::apply , 0);
                    break;
                    case IFP_OP_MUL:
                        dr_insert_call(drcontext , bb , instr ,is_double ?  (void*)interflop_backend<double,Interflop::Op<double>::mul>::apply : (void*)interflop_backend<float,Interflop::Op<float>::mul>::apply , 0);
                    break;
                    case IFP_OP_DIV:
                        dr_insert_call(drcontext , bb , instr ,is_double ?  (void*)interflop_backend<double,Interflop::Op<double>::div>::apply : (void*)interflop_backend<float,Interflop::Op<float>::div>::apply , 0);
                    break;
                    default:
                        ERROR("Error insert scalar call : Operation not found !");
                }
                // ****************************************************************************
                // ****************************************************************************

                if(saveSRC1) {
                    saveSRC1 = false;
                    insert_pop_pseudo_stack(drcontext , DR_REG_SRC_1 , bb , instr , buffer_reg , scratch);
                }

                if(saveSRC0) {
                    saveSRC0 = false;   
                    insert_pop_pseudo_stack(drcontext , DR_REG_SRC_0 , bb , instr , buffer_reg , scratch);
                }

                // ****************************************************************************
                // Set the result in the corresponding register
                // ****************************************************************************  
                INSERT_READ_TLS(drcontext , tls_result , bb , instr , DR_REG_XDI);
                translate_insert(MOVE_FLOATING(is_double , drcontext , DST(instr,0) , OP_BASE_DISP(DR_REG_XDI, 0, OPSZ(DOUBLE_SIZE)) , OP_BASE_DISP(DR_REG_XDI,0,OPSZ(FLOAT_SIZE))), bb , instr);     
            }

            else { /* PACKED */
                
                // Move addresses of thread memory location of each operand in regiters XDI and XSI
                INSERT_READ_TLS(drcontext , tls_op_A , bb , instr , DR_REG_XDI);
                INSERT_READ_TLS(drcontext , tls_op_B , bb , instr , DR_REG_XSI);

                // ****************************************************************************
                // ***** Move operands to thread local memory 
                // ****************************************************************************

                // ****** FIRST OPERAND *****

                if(IS_XMM(GET_REG(SRC(instr,1)))) {
                    translate_insert(INSTR_CREATE_movupd(drcontext , OP_BASE_DISP(DR_REG_XSI, 0, reg_get_size(GET_REG(SRC(instr,1)))) , SRC(instr,1)) , bb , instr);
                }
                else if(IS_YMM(GET_REG(SRC(instr,1)))) {
                    translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_BASE_DISP(DR_REG_XSI, 0, reg_get_size(GET_REG(SRC(instr,1)))) , SRC(instr,1)) , bb , instr);
                }
                else if(OP_IS_BASE_DISP(SRC(instr,1))) {
                    if(ifp_is_128(oc)) {
                        saveSRC1 = true;
                        insert_push_pseudo_stack(drcontext , DR_REG_SRC_1 , bb , instr , buffer_reg , scratch);

                        translate_insert(INSTR_CREATE_movupd(drcontext , OP_REG(DR_REG_SRC_1) , OP_BASE_DISP(opnd_get_base(SRC(instr,1)) , opnd_get_disp(SRC(instr,1)), reg_get_size(DR_REG_SRC_1))) , bb  , instr);
                        translate_insert(INSTR_CREATE_movupd(drcontext , OP_BASE_DISP(DR_REG_XSI, 0, reg_get_size(DR_REG_SRC_1)) , OP_REG(DR_REG_SRC_1)) , bb  , instr);
                    }
                    else { /* 256 */
                        saveYMM1 = true;
                        insert_push_pseudo_stack(drcontext , DR_REG_YMM1 , bb , instr , buffer_reg , scratch);

                        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_REG(DR_REG_YMM1) , OP_BASE_DISP(opnd_get_base(SRC(instr,1)) , opnd_get_disp(SRC(instr,1)), reg_get_size(DR_REG_YMM1))) , bb  , instr);
                        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_BASE_DISP(DR_REG_XSI, 0, reg_get_size(DR_REG_YMM1)) , OP_REG(DR_REG_YMM1)) , bb  , instr);
                    }
                }

                // ****** SECOND OPERAND *****
                
                if(IS_XMM(GET_REG(SRC(instr,0)))) {
                    translate_insert(INSTR_CREATE_movupd(drcontext , OP_BASE_DISP(DR_REG_XDI, 0, reg_get_size(GET_REG(SRC(instr,0)))) , SRC(instr,0)) , bb , instr);
                }
                else if(IS_YMM(GET_REG(SRC(instr,0)))) {
                    translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_BASE_DISP(DR_REG_XDI, 0, reg_get_size(GET_REG(SRC(instr,0)))) , SRC(instr,0)) , bb , instr);
                }
                else if(OP_IS_BASE_DISP(SRC(instr,0))) {
                    if(ifp_is_128(oc)) {
                        saveSRC0 = true;
                        insert_push_pseudo_stack(drcontext , DR_REG_SRC_0 , bb , instr , buffer_reg , scratch);

                        translate_insert(INSTR_CREATE_movupd(drcontext , OP_REG(DR_REG_SRC_0) , OP_BASE_DISP(opnd_get_base(SRC(instr,0)) , opnd_get_disp(SRC(instr,0)), reg_get_size(DR_REG_SRC_0))) , bb  , instr);
                        translate_insert(INSTR_CREATE_movupd(drcontext , OP_BASE_DISP(DR_REG_XDI, 0, reg_get_size(DR_REG_SRC_0)) , OP_REG(DR_REG_SRC_0)) , bb  , instr);
                    }
                    else { /* 256 */
                        saveYMM0 = true;
                        insert_push_pseudo_stack(drcontext , DR_REG_YMM0 , bb , instr , buffer_reg , scratch);

                        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_REG(DR_REG_YMM0) , OP_BASE_DISP(opnd_get_base(SRC(instr,0)) , opnd_get_disp(SRC(instr,0)), reg_get_size(DR_REG_YMM0))) , bb  , instr);
                        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_BASE_DISP(DR_REG_XDI, 0, reg_get_size(DR_REG_YMM0)) , OP_REG(DR_REG_YMM0)) , bb  , instr);
                    }
                }   

                // ***** Sub stack pointer to handle the case where XSP is equal to XBP and XSP doesn't match the top of the stack *****
                // ***** Otherwise the call will erase data when pushing the return address *****
                // ***** If the gap is greater than 32 bytes, the program may crash !!!!!!!!!!!!!!! *****
                translate_insert(INSTR_CREATE_sub(drcontext , OP_REG(DR_REG_XSP) , OP_INT(32)) , bb , instr);
                
                //****************************************************************************
                // ***** VECTORIAL CALL *****
                // ****************************************************************************
                switch (oc & IFP_OP_TYPE_MASK)
                {
                    case IFP_OP_ADD:
                        if(is_double)
                            insert_corresponding_vect_call<double,Interflop::Op<double>::add>(drcontext , bb , instr, oc);
                        else
                            insert_corresponding_vect_call<float,Interflop::Op<float>::add>(drcontext , bb , instr, oc);
                    break;
                    case IFP_OP_SUB:
                        if(is_double)
                            insert_corresponding_vect_call<double,Interflop::Op<double>::sub>(drcontext , bb , instr, oc);
                        else
                            insert_corresponding_vect_call<float,Interflop::Op<float>::sub>(drcontext , bb , instr, oc);

                    break;
                    case IFP_OP_MUL:
                        if(is_double)
                            insert_corresponding_vect_call<double,Interflop::Op<double>::mul>(drcontext , bb , instr, oc);
                        else
                            insert_corresponding_vect_call<float,Interflop::Op<float>::mul>(drcontext , bb , instr, oc);

                    break;
                    case IFP_OP_DIV:
                        if(is_double)
                            insert_corresponding_vect_call<double,Interflop::Op<double>::div>(drcontext , bb , instr, oc);
                        else
                            insert_corresponding_vect_call<float,Interflop::Op<float>::div>(drcontext , bb , instr, oc);
                       
                    break;
                    default:
                        ERROR("Error insert call : Operation not found !");
                }

                // ****************************************************************************
                // ****************************************************************************
                // ****************************************************************************


                if(saveYMM0) {
                    saveYMM0 = false;
                    insert_pop_pseudo_stack(drcontext , DR_REG_YMM0 , bb , instr , buffer_reg , scratch);
                }

                if(saveSRC0) {
                    saveSRC0 = false;   
                    insert_pop_pseudo_stack(drcontext , DR_REG_SRC_0 , bb , instr , buffer_reg , scratch);
                }

                if(saveYMM1) {
                    saveYMM1 = false;
                    insert_pop_pseudo_stack(drcontext , DR_REG_YMM1 , bb , instr , buffer_reg , scratch);
                }

                if(saveSRC1) {
                    saveSRC1 = false;   
                    insert_pop_pseudo_stack(drcontext , DR_REG_SRC_1 , bb , instr , buffer_reg , scratch);
                }

                // ****************************************************************************
                // Set the result in the corresponding register
                // ****************************************************************************  
                INSERT_READ_TLS(drcontext , tls_result , bb , instr , DR_REG_XDI);

                if(IS_XMM(GET_REG(DST(instr,0)))) {
                    translate_insert(INSTR_CREATE_movupd(drcontext , DST(instr,0) , OP_BASE_DISP(DR_REG_XDI , 0 , reg_get_size(GET_REG(DST(instr,0))))) , bb , instr);
                }
                else if(IS_YMM(GET_REG(DST(instr,0)))) {
                    translate_insert(INSTR_CREATE_vmovupd(drcontext , DST(instr,0) , OP_BASE_DISP(DR_REG_XDI , 0 , reg_get_size(GET_REG(DST(instr,0))))) , bb , instr);
                }

            }

            // ****************************************************************************
            // pop general purpose registers on pseudo stack 
            // All GPR except DR_BUFFER_REG and DR_SCRATCH_REG
            // ****************************************************************************
            reg_id_t topop_reg[] = {DR_REG_XBX , DR_REG_XSP , DR_REG_XBP , DR_REG_XAX , DR_REG_XSI , DR_REG_XDI};
            insert_pop_pseudo_stack_list(drcontext , topop_reg , bb , instr , buffer_reg , scratch , 6);

            // ****************************************************************************
            // Restore processor flags
            // ****************************************************************************
            dr_restore_arith_flags(drcontext , bb , instr , SPILL_SLOT_ARITH_FLAG);
           
            // Restore registers
            // ****************************************************************************
            dr_restore_reg(drcontext , bb , instr , buffer_reg , SPILL_SLOT_BUFFER_REG);
            dr_restore_reg(drcontext , bb , instr , scratch , SPILL_SLOT_SCRATCH_REG);
            
            // ****************************************************************************
            // PRINT AFTER 
            // ****************************************************************************
            //dr_insert_clean_call(drcontext , bb , instr , (void*)print , false , 0);
            // ****************************************************************************

            // ****************************************************************************
            // Remove original instruction
            // ****************************************************************************
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);

            if(display_after) {
                display_after = false;
                dr_printf("\n");
                for(instr2 = instrlist_first(bb); instr2 != NULL; instr2 = next_instr2) {
                    next_instr2 = instr_get_next(instr2);
                    dr_print_instr(drcontext , STDERR , instr2 , "INSTR 2 : ");
                }
            }            
        }       
    }
    return DR_EMIT_STORE_TRANSLATIONS;
}


//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

static dr_emit_flags_t runtime(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, void **user_data) {
    /*instr_t *instr, *next_instr;
    for(instr = instrlist_first(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next(instr);
        //dr_printf("BUFFER ADDRESS IN REGISTER : %p\tREAL BUFFER ADDRESS : %p\n",buffer_address_reg,*dbuffer_ind);
        dr_print_instr(drcontext, STDERR, instr, "RUNTIME Found : ");

    }*/
    return DR_EMIT_DEFAULT;
}
