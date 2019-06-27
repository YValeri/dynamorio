#include "interflop_compute.hpp"
#include "interflop_operations.hpp"
#include "./backend/interflop/backend.hxx"
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include <string.h>

/*
template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
static inline void execute(FTYPE* src0, FTYPE* src1, FTYPE* res)
{
    constexpr unsigned int maxiter = SIMD_TYPE == 0 ? 1 : (SIMD_TYPE == IFP_OP_128 ? 4 : SIMD_TYPE == IFP_OP_256 ? 8 : 16)/((sizeof(FTYPE))/sizeof(float));
    for(int i=0; i<maxiter; i++)
    {
        res[i] = FN(src1[i], src0[i]);
    }
}

void initContext(dr_mcontext_flags_t flags, void* drcontext, OUT dr_mcontext_t* mcontext)
{
    mcontext->size = sizeof(*mcontext);
    mcontext->flags = flags;
    dr_get_mcontext(drcontext , mcontext);
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
void interflop_operation_reg(const reg_id_t reg_src0, const reg_id_t reg_src1, const reg_id_t reg_dst, const dr_mcontext_flags_t flags)
{
    dr_mcontext_t mcontext;
    void* drcontext = dr_get_current_drcontext();
    initContext(flags, drcontext, &mcontext);

    constexpr size_t size = sizeof(dr_zmm_t)/sizeof(FTYPE);
    FTYPE src0[size], src1[size], res[size];
    reg_get_value_ex(reg_src0, &mcontext, (byte*)src0);
    reg_get_value_ex(reg_src1, &mcontext, (byte*)src1);

    execute<FTYPE, FN, SIMD_TYPE>(src0, src1, res);

    reg_set_value_ex(reg_dst, &mcontext, res);
    dr_set_mcontext(drcontext , &mcontext);
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
void interflop_operation_base_disp(reg_id_t base, long disp, reg_id_t reg_src1, reg_id_t reg_dst, const dr_mcontext_flags_t flags)
{
    dr_mcontext_t mcontext;
    void* drcontext = dr_get_current_drcontext();
    initContext(flags, drcontext, &mcontext);

    constexpr size_t size = sizeof(dr_zmm_t)/sizeof(FTYPE);
    FTYPE src0[size], src1[size], res[size];
    byte *addr;
    reg_get_value_ex(base , &mcontext , addr);
    constexpr size_t copysize = SIMD_TYPE == 0 ? sizeof(FTYPE) : SIMD_TYPE == IFP_OP_128 ? 16 : SIMD_TYPE == IFP_OP_256 ? 32 : 64;
    memcpy(src0, addr+disp, copysize);
    reg_get_value_ex(reg_src1, &mcontext, (byte*)src1);

    execute<FTYPE, FN, SIMD_TYPE>(src0, src1, res);

    reg_set_value_ex(reg_dst, &mcontext, res);
    dr_set_mcontext(drcontext , &mcontext);
}

template<typename FTYPE, FTYPE (*FN)(FTYPE, FTYPE), int SIMD_TYPE>
void interflop_operation_addr(void* addr, reg_id_t reg_src1, reg_id_t reg_dst, const dr_mcontext_flags_t flags)
{
    dr_mcontext_t mcontext;
    void* drcontext = dr_get_current_drcontext();
    initContext(flags, drcontext, &mcontext);

    constexpr size_t size = sizeof(dr_zmm_t)/sizeof(FTYPE);
    FTYPE src0[size], src1[size], res[size];
    constexpr size_t copysize = SIMD_TYPE == 0 ? sizeof(FTYPE) : SIMD_TYPE == IFP_OP_128 ? 16 : SIMD_TYPE == IFP_OP_256 ? 32 : 64;
    memcpy(src0, addr, copysize);
    reg_get_value_ex(reg_src1, &mcontext, (byte*)src1);

    execute<FTYPE, FN, SIMD_TYPE>(src0, src1, res);

    reg_set_value_ex(reg_dst, &mcontext, res);
    dr_set_mcontext(drcontext , &mcontext);
}
*/