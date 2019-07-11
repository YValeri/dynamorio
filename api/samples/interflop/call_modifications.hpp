#ifndef CALL_BARRIER
#define CALL_BARRIER

inline void prepare_for_call(void* drcontext, instrlist_t *ilist, instr_t *instr, bool is_ymm)
{
    translate_insert(XINST_CREATE_sub(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_vmovaps(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, (is_ymm)?OPSZ_32:OPSZ_16), 
        opnd_create_reg((is_ymm)?DR_REG_YMM2:DR_REG_XMM2)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    translate_insert(XINST_CREATE_sub(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_vmovaps(drcontext, 
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, (is_ymm)?OPSZ_32:OPSZ_16), 
        opnd_create_reg((is_ymm)?DR_REG_YMM7:DR_REG_XMM7)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
}

inline void after_call(void* drcontext, instrlist_t *ilist, instr_t* instr, bool is_ymm)
{
    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_vmovaps(drcontext, 
        opnd_create_reg((is_ymm)?DR_REG_YMM7:DR_REG_XMM7),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, (is_ymm)?OPSZ_32:OPSZ_16)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    translate_insert(XINST_CREATE_add(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    //dr_printf("second translate\n");
    translate_insert(INSTR_CREATE_vmovaps(drcontext, 
        opnd_create_reg((is_ymm)?DR_REG_YMM2:DR_REG_XMM2),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, (is_ymm)?OPSZ_32:OPSZ_16)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");

    translate_insert(XINST_CREATE_add(drcontext, 
        opnd_create_reg(DR_REG_XSP), 
        opnd_create_immed_int(32, OPSZ_4)), ilist, instr);
    //dr_print_opnd(drcontext, STDERR, esp, "ESP : ");
}

#endif