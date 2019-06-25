#ifndef INTERFLOP_COMPUTE_HEADER
#define INTERFLOP_COMPUTE_HEADER

#include "./backend/interflop/backend.hxx"
#include "interflop_operations.hpp"
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include <string.h>


template <typename T>
void get_value_from_opnd(void *drcontext , dr_mcontext_t mcontext , opnd_t src , T *res, int size_mask) {
    if(!opnd_is_null(src)) {
        if(opnd_is_reg(src)) {
           reg_get_value_ex(opnd_get_reg(src) , &mcontext , (byte*)res);
        }
        else if(opnd_is_base_disp(src)) {
            reg_id_t base = opnd_get_base(src);
            int disp = opnd_get_disp(src);
            unsigned long *addr;

            reg_get_value_ex(base , &mcontext , (byte*)&addr);
            switch(size_mask)
            {
                case IFP_OP_512:
                    memcpy((void*)res , ((byte*)addr)+disp , 64);
                break;
                case IFP_OP_256:
                    memcpy((void*)res , ((byte*)addr)+disp , 32);
                break;
                case IFP_OP_128:
                    memcpy((void*)res , ((byte*)addr)+disp , 16);
                break;
                    memcpy((void*)res , ((byte*)addr)+disp , sizeof(T));
            }
            
        }
        else if(opnd_is_abs_addr(src) || opnd_is_rel_addr(src)) {
            switch(size_mask)
            {
                case IFP_OP_512:
                    memcpy((void*)res , opnd_get_addr(src) , 64);
                break;
                case IFP_OP_256:
                    memcpy((void*)res , opnd_get_addr(src) , 32);
                break;
                case IFP_OP_128:
                    memcpy((void*)res , opnd_get_addr(src) , 16);
                break;
                    memcpy((void*)res , opnd_get_addr(src) , sizeof(T));
            }
            //memcpy((void*)res , opnd_get_addr(src) , sizeof(T));
        }
        else if(opnd_is_immed(src)) {
            // Possible only with float
            *res = opnd_get_immed_float(src);
        }

    }
}

static inline void get_instr_and_context(app_pc pc, void* drcontext, OUT instr_t* instr, OUT dr_mcontext_t *mcontext)
{
    mcontext->size = sizeof(*mcontext);
    mcontext->flags = DR_MC_ALL;
    dr_get_mcontext(drcontext , mcontext);
    instr_init(drcontext, instr);
    decode(drcontext , pc , instr);
}

static inline void push_result_and_free(instr_t * instr, byte* value, void* drcontext, dr_mcontext_t* mcontext)
{
    reg_id_t dst = opnd_get_reg(instr_get_dst(instr,0));
    reg_set_value_ex(dst , mcontext , value);
    dr_set_mcontext(drcontext , mcontext);
    instr_free(drcontext , instr);
}

template <typename T>
void interflop_operation(app_pc apppc , int operation);

#endif 