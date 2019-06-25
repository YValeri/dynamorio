#ifndef INTERFLOP_COMPUTE_HEADER
#define INTERFLOP_COMPUTE_HEADER

#include "./backend/interflop/backend.hxx"
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include <string.h>

template <typename T>
void get_value_from_opnd(void *drcontext , dr_mcontext_t mcontext , opnd_t src , T *res) {
    if(!opnd_is_null(src)) {
        if(opnd_is_reg(src)) {
           reg_get_value_ex(opnd_get_reg(src) , &mcontext , (byte*)res);
        }
        else if(opnd_is_base_disp(src)) {
            reg_id_t base = opnd_get_base(src);
            int disp = opnd_get_disp(src);
            unsigned long *addr;

            reg_get_value_ex(base , &mcontext , (byte*)&addr);
            memcpy((void*)res , ((byte*)addr)+disp , sizeof(T));
        }
        else if(opnd_is_abs_addr(src) || opnd_is_rel_addr(src)) {
            memcpy((void*)res , opnd_get_addr(src) , sizeof(T));
        }
        else if(opnd_is_immed(src)) {
            // Possible only with float
            *(T*)res = opnd_get_immed_float(src);
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
void interflop_mul(app_pc apppc)
{   
    instr_t instr;
    dr_mcontext_t mcontext;
    void *drcontext = dr_get_current_drcontext();
    get_instr_and_context(apppc, drcontext, &instr, &mcontext);

    T src0[64/sizeof(T)];
    T src1[64/sizeof(T)];
    get_value_from_opnd<T>(drcontext, mcontext, instr_get_src(&instr, 0), src0);
    get_value_from_opnd<T>(drcontext, mcontext, instr_get_src(&instr, 1), src1);

    T res = Interflop::Op<T>::mul(*src1, *src0);

    push_result_and_free(&instr, (byte*)&res, drcontext, &mcontext);
}

template <typename T>
void interflop_div(app_pc apppc)
{   
    instr_t instr;
    dr_mcontext_t mcontext;
    void *drcontext = dr_get_current_drcontext();
    get_instr_and_context(apppc, drcontext, &instr, &mcontext);

    T src0[64/sizeof(T)];
    T src1[64/sizeof(T)];
    get_value_from_opnd<T>(drcontext, mcontext, instr_get_src(&instr, 0), src0);
    get_value_from_opnd<T>(drcontext, mcontext, instr_get_src(&instr, 1), src1);

    T res = Interflop::Op<T>::div(*src1, *src0);

    push_result_and_free(&instr, (byte*)&res, drcontext, &mcontext);
}

template <typename T>
void interflop_sub(app_pc apppc)
{   
    instr_t instr;
    dr_mcontext_t mcontext;
    void *drcontext = dr_get_current_drcontext();
    get_instr_and_context(apppc, drcontext, &instr, &mcontext);

    T src0[64/sizeof(T)];
    T src1[64/sizeof(T)];
    get_value_from_opnd<T>(drcontext, mcontext, instr_get_src(&instr, 0), src0);
    get_value_from_opnd<T>(drcontext, mcontext, instr_get_src(&instr, 1), src1);

    T res = Interflop::Op<T>::sub(*src1, *src0);

    push_result_and_free(&instr, (byte*)&res, drcontext, &mcontext);
}

template <typename T>
void interflop_add(app_pc apppc)
{   
    instr_t instr;
    dr_mcontext_t mcontext;
    void *drcontext = dr_get_current_drcontext();
    get_instr_and_context(apppc, drcontext, &instr, &mcontext);

    T src0[64/sizeof(T)];
    T src1[64/sizeof(T)];
    get_value_from_opnd<T>(drcontext, mcontext, instr_get_src(&instr, 0), src0);
    get_value_from_opnd<T>(drcontext, mcontext, instr_get_src(&instr, 1), src1);

    T res = Interflop::Op<T>::add(*src1, *src0);

    push_result_and_free(&instr, (byte*)&res, drcontext, &mcontext);
}

//TODO
template <typename T>
void ifp_compute_add(T* operand_buffer, T* result_buffer, unsigned char nbOfPrimitive)
{
    for(unsigned char i=0; i<nbOfPrimitive; ++i)
        *(result_buffer+i) = Interflop::Op<T>::add(*(operand_buffer+nbOfPrimitive+i), *operand_buffer+i);
}

template <typename T>
void ifp_compute_sub(T* operand_buffer, T* result_buffer, unsigned char nbOfPrimitive)
{
    for(unsigned char i=0; i<nbOfPrimitive; ++i)
        *(result_buffer+i) = Interflop::Op<T>::sub(*(operand_buffer+nbOfPrimitive+i), *operand_buffer+i);
}

template <typename T>
void ifp_compute_mul(T* operand_buffer, T* result_buffer, unsigned char nbOfPrimitive)
{
    for(unsigned char i=0; i<nbOfPrimitive; ++i)
        *(result_buffer+i) = Interflop::Op<T>::mul(*(operand_buffer+nbOfPrimitive+i), *operand_buffer+i);
}

template <typename T>
void ifp_compute_div(T* operand_buffer, T* result_buffer, unsigned char nbOfPrimitive)
{
    for(unsigned char i=0; i<nbOfPrimitive; ++i)
        *(result_buffer+i) = Interflop::Op<T>::div(*(operand_buffer+nbOfPrimitive+i), *operand_buffer+i);
}

#endif 