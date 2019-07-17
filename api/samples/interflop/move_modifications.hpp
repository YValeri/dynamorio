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

    static inline void move_operands(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_vmovupd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM13:DR_REG_XMM13),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_vmovupd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM14:DR_REG_XMM14),
            src1), ilist, instr);
    }

    static inline void move_operands_fma(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, opnd_t src2, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_vmovupd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM13:DR_REG_XMM13),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_vmovupd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM14:DR_REG_XMM14),
            src1), ilist, instr);

        translate_insert(INSTR_CREATE_vmovupd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM15:DR_REG_XMM15),
            src2), ilist, instr);
    }

};

template <int SIMD_TYPE>
struct mover<float, SIMD_TYPE> {

    static inline void move_operands(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_vmovups(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM13:DR_REG_XMM13),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_vmovups(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM14:DR_REG_XMM14),
            src1), ilist, instr);
    }

    static inline void move_operands_fma(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, opnd_t src2, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_vmovups(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM13:DR_REG_XMM13),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_vmovups(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM14:DR_REG_XMM14),
            src1), ilist, instr);

        translate_insert(INSTR_CREATE_vmovups(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM15:DR_REG_XMM15),
            src2), ilist, instr);
    }

};

template <>
struct mover<double, 0> {

    static inline void move_operands(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, bool is_ymm)
    {
        dr_printf("truc\n");
        translate_insert(INSTR_CREATE_movsd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM13:DR_REG_XMM13),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_movsd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM14:DR_REG_XMM14),
            src1), ilist, instr);
    }

    static inline void move_operands_fma(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, opnd_t src2, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_movsd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM13:DR_REG_XMM13),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_movsd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM14:DR_REG_XMM14),
            src1), ilist, instr);

        translate_insert(INSTR_CREATE_movsd(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM15:DR_REG_XMM15),
            src2), ilist, instr);
    }

};

template <>
struct mover<float, 0> {

    static inline void move_operands(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM13:DR_REG_XMM13),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM14:DR_REG_XMM14),
            src1), ilist, instr);
    }

    static inline void move_operands_fma(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, opnd_t src2, bool is_ymm)
    {
        translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM13:DR_REG_XMM13),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM14:DR_REG_XMM14),
            src1), ilist, instr);

        translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg((is_ymm)?DR_REG_YMM15:DR_REG_XMM15),
            src2), ilist, instr);
    }
};

template<typename FTYPE, int SIMD_TYPE>
inline void move_operands(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, bool is_ymm)
{
    mover<FTYPE, SIMD_TYPE>::move_operands(drcontext, ilist, instr, 
        src0, src1, is_ymm);
}

template<typename FTYPE, int SIMD_TYPE>
inline void move_operands_fma_temp(void *drcontext, instrlist_t *ilist, instr_t* instr, 
        opnd_t src0, opnd_t src1, opnd_t src2, bool is_ymm)
{
    mover<FTYPE, SIMD_TYPE>::move_operands_fma(drcontext, ilist, instr, 
        src0, src1, src2, is_ymm);
}

inline void move_operands_fma(void *drcontext, 
        opnd_t src0, opnd_t src1, opnd_t src2, 
        instrlist_t *ilist, instr_t* instr)
{
    move_operands_fma_temp<float, 0>(drcontext, ilist, instr, src0, src1, src2, false);
        /*translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg(DR_REG_XMM7),
            src0), ilist, instr);

        translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg(DR_REG_XMM8),
            src1), ilist, instr);

        translate_insert(INSTR_CREATE_movss(drcontext, 
            opnd_create_reg(DR_REG_XMM15),
            src2), ilist, instr);*/
}

template<typename FTYPE>
inline void move_back(void* drcontext, instrlist_t *ilist, instr_t* instr, 
    opnd_t dst, bool is_ymm);

template<>
inline void move_back<double>(void* drcontext, instrlist_t *ilist, instr_t* instr, 
    opnd_t dst, bool is_ymm)
{
    translate_insert(INSTR_CREATE_vmovupd(drcontext, dst, 
        opnd_create_reg((is_ymm)?DR_REG_YMM15:DR_REG_XMM15)), ilist, instr);
}

template<>
inline void move_back<float>(void* drcontext, instrlist_t *ilist, instr_t* instr, 
    opnd_t dst, bool is_ymm)
{
    translate_insert(INSTR_CREATE_vmovups(drcontext, dst, 
        opnd_create_reg((is_ymm)?DR_REG_YMM15:DR_REG_XMM15)), ilist, instr);
}

inline void move_back_fma(void *drcontext, opnd_t dst, 
        instrlist_t *ilist, instr_t* instr)
{
    move_back<float>(drcontext, ilist, instr, dst, false);
    //translate_insert(INSTR_CREATE_vmovups(drcontext, dst, 
    //    opnd_create_reg(DR_REG_XMM15)), ilist, instr);
}

#endif