/**
 * DynamoRIO client developped in the scope of INTERFLOP project 
 */

#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drreg.h"
#include "drmgr.h"
#include "stdint.h"
#include "interflop/move_modifications.hpp"
#include "interflop/call_modifications.hpp"
#include "interflop/interflop_operations.hpp"
#include "interflop/interflop_compute.hpp"
#include "interflop/backend/interflop.h"

static void event_exit(void);

//static void *testcontext;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *ilist,        // Linked list of the instructions 
                                            bool for_trace,        
                                            bool translating);      


static void thread_init_handler(void* drcontext)
{
    void* addr_ymm1 = dr_thread_alloc(drcontext, 1024);
    drmgr_set_tls_field(drcontext, tls_index_first, addr_ymm1);

    void* addr_ymm2 = dr_thread_alloc(drcontext, 1024);
    drmgr_set_tls_field(drcontext, tls_index_second, addr_ymm2);

    void* addr_ymm3 = dr_thread_alloc(drcontext, 1024);
    drmgr_set_tls_field(drcontext, tls_index_third, addr_ymm3);
}

static void thread_exit_handler(void* drcontext)
{
    void* addr_ymm1 = drmgr_get_tls_field(drcontext, tls_index_first);
    dr_thread_free(drcontext, addr_ymm1, 1024);

    void* addr_ymm2 = drmgr_get_tls_field(drcontext, tls_index_second);
    dr_thread_free(drcontext, addr_ymm2, 1024);

    void* addr_ymm3 = drmgr_get_tls_field(drcontext, tls_index_third);
    dr_thread_free(drcontext, addr_ymm3, 1024);
}

// Main function to setup the dynamoRIO client
DR_EXPORT void dr_client_main(  client_id_t id, // client ID
                                int argc,   
                                const char *argv[])
{
    // Init DynamoRIO MGR extension ()
    drmgr_init();

    tls_index_first = drmgr_register_tls_field();
    tls_index_second = drmgr_register_tls_field();
    tls_index_third = drmgr_register_tls_field();
    
    // Define the functions to be called before exiting this client program
    dr_register_exit_event(event_exit);

    // Define the function to executed to treat each instructions block
    drmgr_register_bb_app2app_event(event_basic_block, NULL);

    drmgr_register_thread_init_event(thread_init_handler);
    drmgr_register_thread_exit_event(thread_exit_handler);

}

static void event_exit(void)
{
    drmgr_unregister_tls_field(tls_index_third);
    drmgr_unregister_tls_field(tls_index_second);
    drmgr_unregister_tls_field(tls_index_first);

    drmgr_exit();
    drreg_exit();
}


template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
struct machin;

template <int SIMD_TYPE, float (*FN)(float, float)>
struct machin<float, FN, SIMD_TYPE> {

    static void interface_interflop()
    {
        DR_ASSERT(false);
    } 

};

template <int SIMD_TYPE, double (*FN)(double, double)>
struct machin<double, FN, SIMD_TYPE> {

    static void interface_interflop()
    {
        DR_ASSERT(false);
    } 

};

template <float (*FN)(float, float)>
struct machin<float, FN, IFP_OP_256> {

    static void interface_interflop()
    {
        //dr_printf("float, 256\n");

        constexpr size_t size = sizeof(dr_zmm_t)/sizeof(float);
        float src0[size], src1[size], res[size];

        asm volatile("\tvmovups %%ymm13, %0\n" : "=m" (src0));
        //dr_printf("src0[0] = %.30f\n", src0[0]);

        asm volatile("\tvmovups %%ymm14, %0\n" : "=m" (src1));
        //dr_printf("src1[0] = %.30f\n", src1[0]);

        res[7] = FN(src1[7], src0[7]);
        res[6] = FN(src1[6], src0[6]);
        res[5] = FN(src1[5], src0[5]);
        res[4] = FN(src1[4], src0[4]);
        res[3] = FN(src1[3], src0[3]);
        res[2] = FN(src1[2], src0[2]);
        res[1] = FN(src1[1], src0[1]);
        res[0] = FN(src1[0], src0[0]);

        //dr_printf("res[0] = %.30f\n", res[0]);

        asm volatile("\tvmovups %0, %%ymm15\n" : : "m" (res));
    } 

};

template <double (*FN)(double, double)>
struct machin<double, FN, IFP_OP_256> {

    static void interface_interflop()
    {
        //dr_printf("double, 256\n");

        constexpr size_t size = sizeof(dr_zmm_t)/sizeof(double);
        double src0[size], src1[size], res[size];

        asm volatile("\tvmovupd %%ymm13, %0\n" : "=m" (src0));
        //dr_printf("src0[0] = %.30f\n", src0[0]);

        asm volatile("\tvmovupd %%ymm14, %0\n" : "=m" (src1));
        //dr_printf("src1[0] = %.30f\n", src1[0]);

        res[7] = FN(src1[7], src0[7]);
        res[6] = FN(src1[6], src0[6]);
        res[5] = FN(src1[5], src0[5]);
        res[4] = FN(src1[4], src0[4]);
        res[3] = FN(src1[3], src0[3]);
        res[2] = FN(src1[2], src0[2]);
        res[1] = FN(src1[1], src0[1]);
        res[0] = FN(src1[0], src0[0]);

        //dr_printf("res[0] = %.30f\n", res[0]);

        asm volatile("\tvmovupd %0, %%ymm15\n" : : "m" (res));
    } 

};

template <float (*FN)(float, float)>
struct machin<float, FN, IFP_OP_128> {

    static void interface_interflop()
    {
        //dr_printf("float, 158\n");

        constexpr size_t size = sizeof(dr_zmm_t)/sizeof(float);
        float src0[size], src1[size], res[size];

        asm volatile("\tvmovups %%xmm13, %0\n" : "=m" (src0));
        //dr_printf("src0[0] = %.30f\n", src0[0]);

        asm volatile("\tvmovups %%xmm14, %0\n" : "=m" (src1));
        //dr_printf("src1[0] = %.30f\n", src1[0]);
        
        res[3] = FN(src1[3], src0[3]);
        res[2] = FN(src1[2], src0[2]);
        res[1] = FN(src1[1], src0[1]);
        res[0] = FN(src1[0], src0[0]);

        //dr_printf("res[0] = %.30f\n", res[0]);

        asm volatile("\tvmovups %0, %%xmm15\n" : : "m" (res));
    } 

};

template <double (*FN)(double, double)>
struct machin<double, FN, IFP_OP_128> {

    static void interface_interflop()
    {
        //dr_printf("double, 128\n");

        constexpr size_t size = sizeof(dr_zmm_t)/sizeof(double);
        double src0[size], src1[size], res[size];

        asm volatile("\tvmovupd %%xmm13, %0\n" : "=m" (src0));
        //dr_printf("src0[0] = %.30f\n", src0[0]);

        asm volatile("\tvmovupd %%xmm14, %0\n" : "=m" (src1));
        //dr_printf("src1[0] = %.30f\n", src1[0]);

        res[3] = FN(src1[3], src0[3]);
        res[2] = FN(src1[2], src0[2]);
        res[1] = FN(src1[1], src0[1]);
        res[0] = FN(src1[0], src0[0]);

        //dr_printf("res[0] = %.30f\n", res[0]);

        asm volatile("\tvmovupd %0, %%xmm15\n" : : "m" (res));
    } 

};

template <float (*FN)(float, float)>
struct machin<float, FN, 0> {

    static void interface_interflop()
    {
        //dr_printf("float, 0\n");
        float temp_A;
        float temp_B;

        asm volatile("\tmovss %%xmm13, %0\n" : "=m" (temp_A));
        //dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovss %%xmm14, %0\n" : "=m" (temp_B));
        //dr_printf("temp_B = %.30f\n", temp_B);

        float temp_C = FN(temp_B, temp_A);
        //dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovss %0, %%xmm15\n" : : "m" (temp_C));
    } 

};

template <double (*FN)(double, double)>
struct machin<double, FN, 0> {

    static void interface_interflop()
    {
        //dr_printf("double, 0\n");
        double temp_A;
        double temp_B;

        asm volatile("\tmovsd %%xmm13, %0\n" : "=m" (temp_A));
        //dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovsd %%xmm14, %0\n" : "=m" (temp_B));
        //dr_printf("temp_B = %.30f\n", temp_B);

        double temp_C = temp_B + temp_A;
        //double temp_C = FN(temp_B, temp_A);
        //dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovsd %0, %%xmm15\n" : : "m" (temp_C));
    } 

};

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline void interface_interflop()
{
    machin<FTYPE, FN, SIMD_TYPE>::interface_interflop();
}

static void interface_interflop_fma()
    {
        float temp_A;
        float temp_B;
        float temp_C;

        asm volatile("\tmovss %%xmm13, %0\n" : "=m" (temp_A));
        dr_printf("temp_A = %f\n", temp_A);

        asm volatile("\tmovss %%xmm14, %0\n" : "=m" (temp_B));
        dr_printf("temp_B = %f\n", temp_B);

        asm volatile("\tmovss %%xmm15, %0\n" : "=m" (temp_C));
        dr_printf("temp_C = %f\n", temp_C);

        //float temp_D = Interflop::Op<float>::fma(temp_A, temp_C, temp_B);
        float temp_D = temp_C * temp_B + temp_A;
        dr_printf("temp_D = %f\n", temp_D);

        //double temp_C = FN(temp_B, temp_A);
        //dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovss %0, %%xmm15\n" : : "m" (temp_D));
    }

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline bool insert_corresponding_parameters(void* drcontext, instrlist_t *ilist, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(instr_num_srcs(instr) == 2)
    {

        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);
        opnd_t dst = instr_get_dst(instr, 0);

        bool is_ymm = false;

        if(opnd_is_reg(src0))
            is_ymm = reg_is_ymm(opnd_get_reg(src0));

        //if(opnd_is_reg(src1) && opnd_is_reg(dst))
        //{
            prepare_for_call(drcontext, ilist, instr, is_ymm);

            move_operands<FTYPE, SIMD_TYPE>(drcontext, ilist, instr, src0, src1, is_ymm);

            dr_insert_call(drcontext, ilist, instr, (void*)interface_interflop<FTYPE, FN, SIMD_TYPE>, 0);

            move_back<FTYPE>(drcontext, ilist, instr, dst, is_ymm);

            after_call(drcontext, ilist, instr, is_ymm);

            return true;
        //}
    }else if(oc == IFP_A132SS)
    {
        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);
        opnd_t src2 = instr_get_src(instr, 2);
        opnd_t dst = instr_get_dst(instr, 0);

            prepare_for_call(drcontext, ilist, instr, false);

            move_operands_fma(drcontext, src0, src1, src2, ilist, instr);

            dr_insert_call(drcontext, ilist, instr, (void*)interface_interflop_fma, 0);

            move_back_fma(drcontext, dst, ilist, instr);

            after_call(drcontext, ilist, instr, false);

            return true;
    }
    return false;
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE)>
inline bool insert_corresponding_packed(void* drcontext, instrlist_t *ilist, instr_t* instr,OPERATION_CATEGORY oc)
{
    switch(oc & IFP_SIMD_TYPE_MASK)
    {
        case IFP_OP_128:
            return insert_corresponding_parameters<FTYPE, FN, IFP_OP_128>(drcontext, ilist, instr, oc);
        case IFP_OP_256:
            return insert_corresponding_parameters<FTYPE, FN, IFP_OP_256>(drcontext, ilist, instr, oc);
        case IFP_OP_512:
            return insert_corresponding_parameters<FTYPE, FN, IFP_OP_512>(drcontext, ilist, instr, oc);
        default:
            return false;
    }
}

template<typename FTYPE, bool packed>
inline bool insert_corresponding_operation(void* drcontext, instrlist_t *ilist, instr_t* instr,OPERATION_CATEGORY oc)
{
    if (packed)
    {
        switch (oc & IFP_OP_TYPE_MASK)
        {
            case IFP_OP_ADD:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::add>(drcontext, ilist, instr, oc);
            break;
            case IFP_OP_SUB:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::sub>(drcontext, ilist, instr, oc);
            break;
            case IFP_OP_MUL:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::mul>(drcontext, ilist, instr, oc);
            break;
            case IFP_OP_DIV:
                return insert_corresponding_packed<FTYPE, Interflop::Op<FTYPE>::div>(drcontext, ilist, instr, oc);
            break;
            default:
                return false;
            break;
        }
    }else{
        switch (oc & IFP_OP_TYPE_MASK)
        {
            case IFP_OP_ADD:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::add, 0>(drcontext, ilist, instr, oc);
            break;
            case IFP_OP_SUB:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::sub, 0>(drcontext, ilist, instr, oc);
            break;
            case IFP_OP_MUL:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::mul, 0>(drcontext, ilist, instr, oc);
            break;
            case IFP_OP_DIV:
                return insert_corresponding_parameters<FTYPE, Interflop::Op<FTYPE>::div, 0>(drcontext, ilist, instr, oc);
            break;
            default:
                return false;
            break;
        }
    }
}

inline bool insert_corresponding_call(void* drcontext, instrlist_t *ilist, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(oc == IFP_A132SS){
        return insert_corresponding_parameters<float, Interflop::Op<float>::add, 0>(drcontext, ilist, instr, oc);
    }
    if(ifp_is_double(oc))
    {
        if(ifp_is_packed(oc))
        {
            return insert_corresponding_operation<double,true>(drcontext, ilist, instr, oc);
        }else if(ifp_is_scalar(oc))
        {
            return insert_corresponding_operation<double,false>(drcontext, ilist, instr, oc);
        }
    }else if(ifp_is_float(oc))
    {
        if(ifp_is_packed(oc))
        {
            return insert_corresponding_operation<float,true>(drcontext, ilist, instr, oc);
        }else if(ifp_is_scalar(oc))
        {
            return insert_corresponding_operation<float,false>(drcontext, ilist, instr, oc);
        }
    }
    
    return false;
}


static dr_emit_flags_t event_basic_block(void *drcontext, void* tag, instrlist_t *ilist, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    OPERATION_CATEGORY oc;

    //bool display = false;

    for(instr = instrlist_first(ilist); instr != NULL; instr = next_instr)
    {
        //dr_print_instr(drcontext, STDERR, instr, "");
        next_instr = instr_get_next(instr);
        oc = ifp_get_operation_category(instr);
        if(oc)
        {

            //dr_printf("\n\n\n BEFORE \n\n\n");
            //display = true;
            dr_print_instr(drcontext, STDERR, instr, "Found : ");

            if(insert_corresponding_call(drcontext, ilist, instr, oc))
            {
                instrlist_remove(ilist, instr);
                instr_destroy(drcontext, instr);
            }else
            {
                dr_printf("Bad oc\n");
            }
            
            //dr_insert_clean_call(drcontext, ilist, instr, ifp_is_double(oc) ? (void*)interflop_operation<double> : (void*)interflop_operation<float>, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)) , opnd_create_immed_int(oc & IFP_OP_MASK , OPSZ_4));
            
        }

    }

    /*if(display){
        dr_printf("\n\n\n AFTER \n\n\n");

        for(instr = instrlist_first(ilist); instr != NULL; instr = next_instr)
        {
            next_instr = instr_get_next(instr);
            dr_print_instr(drcontext, STDERR, instr, "");

        }
    }*/
    return DR_EMIT_DEFAULT;
}