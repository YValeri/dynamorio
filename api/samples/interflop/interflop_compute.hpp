#ifndef INTERFLOP_COMPUTE_HEADER
#define INTERFLOP_COMPUTE_HEADER

#include "./backend/interflop/backend.hxx"
#include "interflop_operations.hpp"
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include <string.h>

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

template<typename FTYPE, typename FUNC, unsigned int UNROLL>
void interflop_opt_operation(reg_id_t src0, reg_id_t src1, reg_id_t dst);

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), unsigned int UNROLL>
void interflop_opt_operation(reg_id_t base, long disp, reg_id_t src1, reg_id_t dst);

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), unsigned int UNROLL>
void interflop_opt_operation(void* addr, reg_id_t src1, reg_id_t dst);

#endif 