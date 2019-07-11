#ifndef MOVE_BARRIER
#define MOVE_BARRIER

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

template<typename FTYPE, int SIMD_TYPE>
struct mover;

template <int SIMD_TYPE>
struct mover<double, SIMD_TYPE> {

    static inline void move_operands(void *drcontext, opnd_t src0, 
        opnd_t src1, instrlist_t *ilist, instr_t* instr, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_vmovapd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM2:DR_REG_XMM2),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_vmovapd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM7:DR_REG_XMM7),
            src1), ilist, instr);
    }

};

template <int SIMD_TYPE>
struct mover<float, SIMD_TYPE> {

    static inline void move_operands(void *drcontext, opnd_t src0, 
        opnd_t src1, instrlist_t *ilist, instr_t* instr, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_vmovaps(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM2:DR_REG_XMM2),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_vmovaps(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM7:DR_REG_XMM7),
            src1), ilist, instr);
    }

};

template <>
struct mover<double, 0> {

    static inline void move_operands(void *drcontext, opnd_t src0, 
        opnd_t src1, instrlist_t *ilist, instr_t* instr, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_movsd(drcontext, 
            opnd_create_reg(DR_REG_XMM2),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_movsd(drcontext, 
            opnd_create_reg(DR_REG_XMM7),
            src1), ilist, instr);
    }

};

template <>
struct mover<float, 0> {

    static inline void move_operands(void *drcontext, opnd_t src0, 
        opnd_t src1, instrlist_t *ilist, instr_t* instr, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg(DR_REG_XMM2),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg(DR_REG_XMM7),
            src1), ilist, instr);
    }
};

template<typename FTYPE, int SIMD_TYPE>
inline void move_operands(void *drcontext, opnd_t src0, 
        opnd_t src1, instrlist_t *ilist, instr_t* instr, bool is_ymm)
{
    mover<FTYPE, SIMD_TYPE>::move_operands(drcontext, src0, 
        src1, ilist, instr, is_ymm);
}

template<typename FTYPE>
inline void move_back(void* drcontext, opnd_t dst, 
    instrlist_t *ilist, instr_t* instr, bool is_ymm);

template<>
inline void move_back<double>(void* drcontext, opnd_t dst, 
    instrlist_t *ilist, instr_t* instr, bool is_ymm)
{
    translate_insert(INSTR_CREATE_vmovapd(drcontext, dst, 
        opnd_create_reg((is_ymm)?DR_REG_YMM2:DR_REG_XMM2)), ilist, instr);
}

template<>
inline void move_back<float>(void* drcontext, opnd_t dst, 
    instrlist_t *ilist, instr_t* instr, bool is_ymm)
{
    translate_insert(INSTR_CREATE_vmovaps(drcontext, dst, 
        opnd_create_reg((is_ymm)?DR_REG_YMM2:DR_REG_XMM2)), ilist, instr);
}

#endif