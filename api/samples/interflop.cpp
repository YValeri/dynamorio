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

static int tls_index;

//static void *testcontext;

//Function to treat each block of instructions 
static dr_emit_flags_t event_basic_block(   void *drcontext,        //Context
                                            void *tag,              // Unique identifier of the block
                                            instrlist_t *bb,        // Linked list of the instructions 
                                            bool for_trace,        
                                            bool translating);      

// Main function to setup the dynamoRIO client
DR_EXPORT void dr_client_main(  client_id_t id, // client ID
                                int argc,   
                                const char *argv[])
{
    // Init DynamoRIO MGR extension ()
    drmgr_init();

    tls_index = drmgr_register_tls_field();
    
    // Define the functions to be called before exiting this client program
    dr_register_exit_event(event_exit);

    // Define the function to executed to treat each instructions block
    drmgr_register_bb_app2app_event(event_basic_block, NULL);

}

static void event_exit(void)
{
    drmgr_unregister_tls_field(tls_index);

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
        dr_printf("float, 256\n");

        constexpr size_t size = sizeof(dr_zmm_t)/sizeof(float);
        float src0[size], src1[size], res[size];

        asm volatile("\tvmovaps %%ymm2, %0\n" : "=m" (src0));
        dr_printf("src0[0] = %.30f\n", src0[0]);

        asm volatile("\tvmovaps %%ymm7, %0\n" : "=m" (src1));
        dr_printf("src1[0] = %.30f\n", src1[0]);

                res[7] = Interflop::Op<float>::add(src1[7], src0[7]);
                res[6] = Interflop::Op<float>::add(src1[6], src0[6]);
                res[5] = Interflop::Op<float>::add(src1[5], src0[5]);
                res[4] = Interflop::Op<float>::add(src1[4], src0[4]);
                res[3] = Interflop::Op<float>::add(src1[3], src0[3]);
                res[2] = Interflop::Op<float>::add(src1[2], src0[2]);
                res[1] = Interflop::Op<float>::add(src1[1], src0[1]);
                res[0] = Interflop::Op<float>::add(src1[0], src0[0]);

        dr_printf("res[0] = %.30f\n", res[0]);

        asm volatile("\tvmovaps %0, %%ymm2\n" : : "m" (res));
    } 

};

template <double (*FN)(double, double)>
struct machin<double, FN, IFP_OP_256> {

    static void interface_interflop()
    {
        dr_printf("double, 256\n");

        constexpr size_t size = sizeof(dr_zmm_t)/sizeof(double);
        double src0[size], src1[size], res[size];

        asm volatile("\tvmovapd %%ymm2, %0\n" : "=m" (src0));
        dr_printf("src0[0] = %.30f\n", src0[0]);

        asm volatile("\tvmovapd %%ymm7, %0\n" : "=m" (src1));
        dr_printf("src1[0] = %.30f\n", src1[0]);

                res[7] = Interflop::Op<double>::add(src1[7], src0[7]);
                res[6] = Interflop::Op<double>::add(src1[6], src0[6]);
                res[5] = Interflop::Op<double>::add(src1[5], src0[5]);
                res[4] = Interflop::Op<double>::add(src1[4], src0[4]);
                res[3] = Interflop::Op<double>::add(src1[3], src0[3]);
                res[2] = Interflop::Op<double>::add(src1[2], src0[2]);
                res[1] = Interflop::Op<double>::add(src1[1], src0[1]);
                res[0] = Interflop::Op<double>::add(src1[0], src0[0]);

        dr_printf("res[0] = %.30f\n", res[0]);

        asm volatile("\tvmovapd %0, %%ymm2\n" : : "m" (res));
    } 

};

template <float (*FN)(float, float)>
struct machin<float, FN, IFP_OP_128> {

    static void interface_interflop()
    {
        dr_printf("float, 128\n");

        constexpr size_t size = sizeof(dr_zmm_t)/sizeof(float);
        float src0[size], src1[size], res[size];

        asm volatile("\tvmovaps %%xmm2, %0\n" : "=m" (src0));
        dr_printf("src0[0] = %.30f\n", src0[0]);

        asm volatile("\tvmovaps %%xmm7, %0\n" : "=m" (src1));
        dr_printf("src1[0] = %.30f\n", src1[0]);
        
                res[3] = Interflop::Op<float>::add(src1[3], src0[3]);
                res[2] = Interflop::Op<float>::add(src1[2], src0[2]);
                res[1] = Interflop::Op<float>::add(src1[1], src0[1]);
                res[0] = Interflop::Op<float>::add(src1[0], src0[0]);

        dr_printf("res[0] = %.30f\n", res[0]);

        asm volatile("\tvmovaps %0, %%xmm2\n" : : "m" (res));
    } 

};

template <double (*FN)(double, double)>
struct machin<double, FN, IFP_OP_128> {

    static void interface_interflop()
    {
        dr_printf("double, 128\n");

        constexpr size_t size = sizeof(dr_zmm_t)/sizeof(double);
        double src0[size], src1[size], res[size];

        asm volatile("\tvmovapd %%xmm2, %0\n" : "=m" (src0));
        dr_printf("src0[0] = %.30f\n", src0[0]);

        asm volatile("\tvmovapd %%xmm7, %0\n" : "=m" (src1));
        dr_printf("src1[0] = %.30f\n", src1[0]);

                res[3] = Interflop::Op<double>::add(src1[3], src0[3]);
                res[2] = Interflop::Op<double>::add(src1[2], src0[2]);
                res[1] = Interflop::Op<double>::add(src1[1], src0[1]);
                res[0] = Interflop::Op<double>::add(src1[0], src0[0]);

        dr_printf("res[0] = %.30f\n", res[0]);

        asm volatile("\tvmovapd %0, %%xmm2\n" : : "m" (res));
    } 

};

template <float (*FN)(float, float)>
struct machin<float, FN, 0> {

    static void interface_interflop()
    {
        dr_printf("float, 0\n");
        float temp_A;
        float temp_B;

        asm volatile("\tmovss %%xmm2, %0\n" : "=m" (temp_A));
        dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovss %%xmm7, %0\n" : "=m" (temp_B));
        dr_printf("temp_B = %.30f\n", temp_B);

        //float temp_C = temp_A + temp_B;

        float temp_C = FN(temp_B, temp_A);

        dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovss %0, %%xmm2\n" : : "m" (temp_C));
    } 

};

template <double (*FN)(double, double)>
struct machin<double, FN, 0> {

    static void interface_interflop()
    {
        //dr_printf("double, 0\n");
        double temp_A;
        double temp_B;

        asm volatile("\tmovsd %%xmm2, %0\n" : "=m" (temp_A));
        dr_printf("temp_A = %.30f\n", temp_A);

        asm volatile("\tmovsd %%xmm7, %0\n" : "=m" (temp_B));
        dr_printf("temp_B = %.30f\n", temp_B);

        //double temp_C = temp_A + temp_B;

        double temp_C = FN(temp_B, temp_A);
        dr_printf("temp_C = %.30f\n", temp_C);

        asm volatile("\tmovsd %0, %%xmm2\n" : : "m" (temp_C));
    } 

};

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline void interface_interflop()
{
    machin<FTYPE, FN, SIMD_TYPE>::interface_interflop();
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
inline bool insert_corresponding_parameters(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    if(instr_num_srcs(instr) == 2)
    {

        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);
        opnd_t dst = instr_get_dst(instr, 0);

        bool is_ymm = false;

        if(opnd_is_reg(src0))
            is_ymm = reg_is_ymm(opnd_get_reg(src0));
            
        /*if(is_ymm)
            dr_printf("reg ymm\n");
        else
            dr_printf("reg not ymm\n");*/

        //if(opnd_is_reg(src1) && opnd_is_reg(dst))
        //{
            prepare_for_call(drcontext, bb, instr, is_ymm);

            move_operands<FTYPE, SIMD_TYPE>(drcontext, src0, src1, bb, instr, is_ymm);

            dr_insert_call(drcontext, bb, instr, (void*)interface_interflop<FTYPE, FN, SIMD_TYPE>, 0);

            move_back<FTYPE>(drcontext, dst, bb, instr, is_ymm);

            after_call(drcontext, bb, instr, is_ymm);

            return true;
        //}
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

            if(insert_corresponding_call(drcontext, bb, instr, oc))
            {
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
            }else
            {
                //dr_printf("Bad oc\n");
            }
            
            //dr_insert_clean_call(drcontext, bb, instr, ifp_is_double(oc) ? (void*)interflop_operation<double> : (void*)interflop_operation<float>, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)) , opnd_create_immed_int(oc & IFP_OP_MASK , OPSZ_4));
            
        }

    }
    return DR_EMIT_DEFAULT;
}