/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "drsyms.h"
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
#define IS_XMM(reg) reg_is_xmm((reg))

#define MOVE_FLOATING(is_double, drcontext , dest , srcd , srcs) (is_double) ? INSTR_CREATE_movsd((drcontext) , (dest) , (srcd)) : INSTR_CREATE_movss((drcontext) , (dest), (srcs))

#define GET_TLS(drcontext,tls) drmgr_get_tls_field((drcontext) , (tls))
#define SET_TLS(drcontext,tls,value) drmgr_set_tls_field((drcontext) , (tls) , (void*)(value))
#define INSERT_READ_TLS(drcontext , tls , bb , instr , reg) drmgr_insert_read_tls_field((drcontext) , (tls) , (bb) , (instr) , (reg))
#define INSERT_WRITE_TLS(drcontext , tls , bb , instr , reg , temp_reg) drmgr_insert_write_tls_field((drcontext) , (tls_stack) , (bb) , (instr) , (buffer_reg) , (temp_reg));

#define SIZE_STACK 256
#define PTR_SIZE sizeof(void*)
#define FLOAT_SIZE sizeof(float)
#define DOUBLE_SIZE sizeof(double)
#define REG_SIZE(reg) opnd_size_in_bytes(reg_get_size((reg)))


typedef byte SLOT;

void *stackkkkkk;

int tls_result /* index of thread local storage to store the result of floating point operations */, 
    tls_stack /* index of thread local storage to store the address of the shallow stack */ ,
    tls_processor_flag /* index of thread local storage to store the number of bytes used on the stack */;

// Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,         //TODO
                                            bool translating);      //TODO

static dr_emit_flags_t runtime(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, void **user_data);
                                        
static void event_exit(void);

static void thread_init(void *drcontext);

static void thread_exit(void *drcontext);


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
    tls_processor_flag = drmgr_register_tls_field();

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
    drmgr_unregister_tls_field(tls_processor_flag);

    drmgr_exit();
    drreg_exit();
}

static void thread_init(void *dr_context) {
    //union {void *aaaaaaaaaaaaaaaaa ; double aaaaa;};
    //aaaaa = 30;
    SET_TLS(dr_context , tls_result ,0);
    SET_TLS(dr_context , tls_stack , dr_thread_alloc(dr_context , SIZE_STACK*sizeof(SLOT)));
    SET_TLS(dr_context , tls_processor_flag , 0);
    stackkkkkk = GET_TLS(dr_context , tls_stack);
}

static void thread_exit(void *dr_context) {
    dr_thread_free(dr_context , drmgr_get_tls_field(dr_context , tls_stack) , SIZE_STACK*sizeof(SLOT));
}

static void print() {
    void *context = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.flags = DR_MC_ALL;
    mcontext.size = sizeof(mcontext);

    dr_get_mcontext(context , &mcontext);

    byte rdi[8] , rbp[8], rsp[8],rax[8] , rsi[8], rbx[8] , rdx[8], rcx[8],
         edi[4] , ebp[4] , esp[4] , eax[4] , esi[4] , ebx[4] , edx[4] , ecx[4],
         xmm[16] , xmm1[16], xmm2[16], xmm3[16] , xmm4[16];
    
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
    
    dr_printf("TLS STACK : %p\n",GET_TLS(context,tls_stack));

    for(int i = 0 ; i < 16 ; i++) dr_printf("%02X ",*(((unsigned char *)(stackkkkkk)+i)));

    dr_printf("\n");
    dr_printf("*****************************************************************************************************************************\n\n");
}


template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline bool insert_corresponding_parameters(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(instr_num_srcs(instr) == 2)
    {
        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);
        opnd_t dst = instr_get_dst(instr, 0);
        if(opnd_is_reg(src1) && opnd_is_reg(dst))
        {
            reg_id_t reg_src1 = opnd_get_reg(src1), reg_dst = opnd_get_reg(dst);
            if(opnd_is_reg(src0))
            {
                //reg reg -> reg
                reg_id_t reg_src0 = opnd_get_reg(src0);
                
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_reg<FTYPE, FN, SIMD_TYPE>, false, 4,OPND_CREATE_INT32(reg_src0),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(DR_MC_MULTIMEDIA));

                return true;
            }else if(opnd_is_base_disp(src0))
            {
                reg_id_t base = opnd_get_base(src0);
                int flags = DR_MC_MULTIMEDIA;
                if(base == DR_REG_XSP) //Cannot find XIP
                {
                    flags |= DR_MC_CONTROL;
                }else if(reg_is_gpr(base))
                {
                    flags |= DR_MC_INTEGER;
                }
                long disp = opnd_get_disp(src0);
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_base_disp<FTYPE, FN, SIMD_TYPE>, false, 5, OPND_CREATE_INT32(base),OPND_CREATE_INT64(disp),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(flags));
                return true;

            }else if(opnd_is_rel_addr(src0) || opnd_is_abs_addr(src0))
            {
                dr_insert_clean_call(drcontext, bb, instr, (void*)interflop_operation_addr<FTYPE, FN, SIMD_TYPE>, false, 4, OPND_CREATE_INTPTR(opnd_get_addr(src0)),OPND_CREATE_INT32(reg_src1), OPND_CREATE_INT32(reg_dst), OPND_CREATE_INT32(DR_MC_MULTIMEDIA));
                return true;
            }
        }
    }
    return false;
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE)>
inline bool insert_corresponding_packed(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    switch(oc & IFP_SIMD_TYPE_MASK)
    {
        case IFP_OP_128:
            return insert_corresponding_parameters<FTYPE, FN, IFP_OP_128>(drcontext, bb, instr, oc);
        case IFP_OP_256:
            return insert_corresponding_parameters<FTYPE, FN, IFP_OP_256>(drcontext, bb, instr, oc);
        case IFP_OP_512:
            return insert_corresponding_parameters<FTYPE, FN, IFP_OP_512>(drcontext, bb, instr, oc);
        default:
            return false;
    }
}

template<typename FTYPE, bool packed>
inline bool insert_corresponding_operation(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if (packed)
    {
        switch (oc & IFP_OP_TYPE_MASK)
        {
            case IFP_OP_ADD:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::add>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_SUB:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::sub>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_MUL:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::mul>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_DIV:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::div>(drcontext, bb, instr, oc);
            break;
            default:
                return false;
            break;
        }
    }else{
        switch (oc & IFP_OP_TYPE_MASK)
        {
            case IFP_OP_ADD:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::add, 0>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_SUB:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::sub, 0>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_MUL:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::mul, 0>(drcontext, bb, instr, oc);
            break;
            case IFP_OP_DIV:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::div, 0>(drcontext, bb, instr, oc);
            break;
            default:
                return false;
            break;
        }
    }
}

inline bool insert_corresponding_call(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(ifp_is_double(oc))
    {
        if(ifp_is_packed(oc))
        {
            return insert_corresponding_operation<double,true>(drcontext, bb, instr, oc);
        }else if(ifp_is_scalar(oc))
        {
            return insert_corresponding_operation<double,false>(drcontext, bb, instr, oc);
        }
    }else if(ifp_is_float(oc))
    {
        if(ifp_is_packed(oc))
        {
            return insert_corresponding_operation<float,true>(drcontext, bb, instr, oc);
        }else if(ifp_is_scalar(oc))
        {
            return insert_corresponding_operation<float,false>(drcontext, bb, instr, oc);
        }
    }
    
    return false;
}

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

void emptyfunc() {
    //byte buf[DR_FPSTATE_BUF_SIZE];
    //proc_save_fpstate(buf);
    //dr_printf("EMPTY FUNC !!!!\n");
    //proc_restore_fpstate(buf);
}

template <typename T>
void interflop_backend(T a,  T b , void *context);


template <>
void interflop_backend<double>(double a,  double b , void *context)
{
     union {void* aaaaaa; double aa;};
    //dr_printf("Context : %p\n",context);
    //dr_printf("A : %d B : %d\n",(int)a,(int)b);
    //dr_printf("Buffer : %f\n",*buff);
    
    //interflop_verrou_add_double(a , b, &aa , context);
    aa = a+b;

    //dr_printf("Result : %d\n",int(aa));
    //dr_printf("Res in tls field : %d\n",(int)aa);*/
    SET_TLS(dr_get_current_drcontext() , tls_result , aaaaaa);
}

template <>
void interflop_backend<float>(float a,  float b, void *context)
{
    union {unsigned int aaaaaa; float aa;};
    dr_printf("A : %d B : %d\n",(int)a,(int)b);
    aa = a+b;
    //interflop_verrou_add_float(a , b, &aa , context);
    SET_TLS(dr_get_current_drcontext() , tls_result , (void*)aaaaaa);
}

static inline void insert_pop_pseudo_stack(void *drcontext , reg_id_t reg, instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg, reg_id_t temp_buf, unsigned int size_reg) {
    
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    // ****************************************************************************
    // Decrement the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_sub(drcontext , OP_REG(buffer_reg) , OP_INT(size_reg)) , bb , instr);

    // ****************************************************************************
    // Load the value in the defined register
    // ****************************************************************************
    if(IS_GPR(reg))
        translate_insert(XINST_CREATE_load(drcontext, OP_REG(reg) , OP_BASE_DISP(buffer_reg , 0 , OPSZ(size_reg))) , bb , instr); 
    else if(IS_XMM(reg))
        translate_insert(INSTR_CREATE_movupd(drcontext, OP_REG(reg), OP_BASE_DISP(buffer_reg , 0 , OPSZ(size_reg))) , bb , instr);
    else 
        dr_fprintf(STDERR, "LOAD ERROR\n");

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}

static inline void insert_push_pseudo_stack(void *drcontext , reg_id_t reg_to_push, instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg, reg_id_t temp_buf , unsigned int size_reg) {
 
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    // ****************************************************************************
    // Store the value of the register to save
    // ****************************************************************************
    if(IS_GPR(reg_to_push))
        translate_insert(XINST_CREATE_store(drcontext , OP_BASE_DISP(buffer_reg , 0 , OPSZ(size_reg)) , OP_REG(reg_to_push)) , bb , instr); 
    else if(IS_XMM(reg_to_push))
        translate_insert(INSTR_CREATE_movupd(drcontext, OP_BASE_DISP(buffer_reg , 0 , OPSZ(size_reg)), OP_REG(reg_to_push)) , bb , instr);
    else 
        dr_fprintf(STDERR, "STORE ERROR\n");

    // ****************************************************************************
    // Increment the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_add(drcontext , OP_REG(buffer_reg) , OP_INT(size_reg)) , bb , instr); 

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}


static dr_emit_flags_t event_basic_block(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    instr_t *instr2 , *next_instr2;
    OPERATION_CATEGORY oc;

    bool display = false;
    bool saveXMM0 = false, saveXMM1 = false;

    for(instr = instrlist_first(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next(instr);

        oc = ifp_get_operation_category(instr);

        if(oc)
        {
            bool is_double = ifp_is_double(oc);
            
            dr_print_instr(drcontext, STDERR, instr , "II : ");
            
            if(display) {
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
            // Reserve two registers among all registers available except RDI
            // ****************************************************************************
            reg_id_t buffer_reg  = DR_BUFFER_REG, 
                     scratch     = DR_SCRATCH_REG;

            dr_save_reg(drcontext , bb , instr , buffer_reg , SPILL_SLOT_BUFFER_REG);
            dr_save_reg(drcontext , bb , instr , scratch , SPILL_SLOT_SCRATCH_REG);
        
            // ****************************************************************************
            // save processor flags on stack
            // ****************************************************************************
            dr_save_arith_flags(drcontext , bb , instr , SPILL_SLOT_ARITH_FLAG);
            
            // ****************************************************************************
            // save rdi on pseudo stack 
            // ****************************************************************************
            insert_push_pseudo_stack(drcontext , DR_REG_XDI , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XDI));

            // ****************************************************************************
            // save rsi on pseudo stack 
            // ****************************************************************************
            insert_push_pseudo_stack(drcontext , DR_REG_XSI , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XSI));

            // ****************************************************************************
            // save rax on pseudo stack 
            // ****************************************************************************
            insert_push_pseudo_stack(drcontext , DR_REG_XAX , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XAX));

            // ****************************************************************************
            // save rbp on pseudo stack 
            // ****************************************************************************
            insert_push_pseudo_stack(drcontext , DR_REG_XBP , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XBP));

            // ****************************************************************************
            // save rsp on pseudo stack 
            // ****************************************************************************
            insert_push_pseudo_stack(drcontext , DR_REG_XSP , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XSP));

            // ****************************************************************************
            // save rbx on pseudo stack 
            // ****************************************************************************
            insert_push_pseudo_stack(drcontext , DR_REG_XBX , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XBX));
            
            // ****************************************************************************
            // pass arguments using RDI register for the context and XMM registers for floating point operands 
            // ****************************************************************************

            // ***** context in %rdi *****
            translate_insert(INSTR_CREATE_movq(drcontext , OP_REG(DR_REG_XDI) , OP_REL_ADDR(drcontext)) , bb , instr);


            // ***** First operand in XMM0 *****
            if(!IS_REG(SRC(instr,1)) || GET_REG(SRC(instr,1)) != DR_REG_SRC_0) {
                saveXMM0 = true;

                // Save current value of XMM0 in pseudo stack
                insert_push_pseudo_stack(drcontext , DR_REG_SRC_0 , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_SRC_0));

                // Set the XMM register with the operand floating point value 
                translate_insert(MOVE_FLOATING(is_double , drcontext , OP_REG(DR_REG_SRC_0) , SRC(instr,1), SRC(instr,1)) , bb , instr);
            }

            
           
            // ***** Second operand in XMM1 *****
            if(!IS_REG(SRC(instr,0)) || GET_REG(SRC(instr,0)) != DR_REG_SRC_1) {
                saveXMM1 = true;

                // Save current value of XMM1 in pseudo stack
                insert_push_pseudo_stack(drcontext , DR_REG_SRC_1 , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_SRC_1));
               
                // Set the XMM register with the operand floating point value 
                translate_insert(MOVE_FLOATING(is_double , drcontext , OP_REG(DR_REG_SRC_1) , SRC(instr,0) , SRC(instr,0)) , bb , instr);
            }
             
            // ****************************************************************************
            // CALL
            // ****************************************************************************
            translate_insert(INSTR_CREATE_sub(drcontext , OP_REG(DR_REG_XSP) , OP_INT(32)) , bb , instr);
            dr_insert_call(drcontext , bb , instr ,is_double ?  (void*)interflop_backend<double> : (void*)interflop_backend<float> , 0);
            
            // ****************************************************************************
            // ****************************************************************************

            if(saveXMM1) {
                saveXMM1 = false;
                insert_pop_pseudo_stack(drcontext , DR_REG_SRC_1 , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_SRC_1));
            }

            if(saveXMM0) {
                saveXMM0 = false;   
                insert_pop_pseudo_stack(drcontext , DR_REG_SRC_0 , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_SRC_0));
            }

            // ****************************************************************************
            // Set the result in the corresponding register
            // ****************************************************************************  
            if(IS_GPR(GET_REG(DST(instr,0))))
                INSERT_READ_TLS(drcontext , tls_result , bb , instr , GET_REG(DST(instr,0)));
            else {
                INSERT_READ_TLS(drcontext , tls_result , bb , instr , DR_REG_XDI);

                insert_push_pseudo_stack(drcontext , DR_REG_XDI , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XDI));                

                INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

                translate_insert(INSTR_CREATE_sub(drcontext , OP_REG(buffer_reg) , OP_INT(REG_SIZE(DR_REG_XDI))) , bb , instr);
                
                translate_insert(MOVE_FLOATING(is_double , drcontext , DST(instr,0) , OP_BASE_DISP(buffer_reg, 0, OPSZ(DOUBLE_SIZE)) , OP_BASE_DISP(buffer_reg,0,OPSZ(FLOAT_SIZE))), bb , instr);
                
                INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , scratch);
            }       

            // ****************************************************************************
            // Pop saved $rbx from pseudo stack
            // ****************************************************************************
            insert_pop_pseudo_stack(drcontext , DR_REG_XBX , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XBX));

            // ****************************************************************************
            // Pop saved $rsp from pseudo stack
            // ****************************************************************************
            insert_pop_pseudo_stack(drcontext , DR_REG_XSP , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XSP));

            // ****************************************************************************
            // Pop saved $rbp from pseudo stack
            // ****************************************************************************
            insert_pop_pseudo_stack(drcontext , DR_REG_XBP , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XBP));

            // ****************************************************************************
            // Pop saved $rax from pseudo stack
            // ****************************************************************************
            insert_pop_pseudo_stack(drcontext , DR_REG_XAX , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XAX));

            // ****************************************************************************
            // Pop saved $rsi from pseudo stack
            // ****************************************************************************
            insert_pop_pseudo_stack(drcontext , DR_REG_XSI , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XSI));

            // ****************************************************************************
            // Pop saved $rdi from pseudo stack
            // ****************************************************************************
            insert_pop_pseudo_stack(drcontext , DR_REG_XDI , bb , instr , buffer_reg , scratch , REG_SIZE(DR_REG_XDI));

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

            if(display) {
                //display = false;
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
