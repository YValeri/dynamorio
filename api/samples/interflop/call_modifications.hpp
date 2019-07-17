#ifndef CALL_BARRIER
#define CALL_BARRIER

static int tls_index_first;
static int tls_index_second;
static int tls_index_third;

inline void prepare_for_call(void* drcontext, instrlist_t *ilist, instr_t *instr, 
    bool is_ymm)
{
    translate_insert(INSTR_CREATE_push(drcontext, opnd_create_reg(DR_REG_RAX)),
        ilist, instr);

    drmgr_insert_read_tls_field(drcontext, tls_index_first, ilist, instr,
        DR_REG_RAX);

    translate_insert(INSTR_CREATE_vmovupd(drcontext, 
        opnd_create_base_disp(DR_REG_RAX, DR_REG_NULL, 0, 0, OPSZ_32), 
        opnd_create_reg(DR_REG_YMM13)), ilist, instr);

    drmgr_insert_read_tls_field(drcontext, tls_index_second, ilist, instr,
        DR_REG_RAX);

    translate_insert(INSTR_CREATE_vmovupd(drcontext, 
        opnd_create_base_disp(DR_REG_RAX, DR_REG_NULL, 0, 0, OPSZ_32), 
        opnd_create_reg(DR_REG_YMM14)), ilist, instr);
    
    drmgr_insert_read_tls_field(drcontext, tls_index_third, ilist, instr,
        DR_REG_RAX);

    translate_insert(INSTR_CREATE_vmovupd(drcontext, 
        opnd_create_base_disp(DR_REG_RAX, DR_REG_NULL, 0, 0, OPSZ_32), 
        opnd_create_reg(DR_REG_YMM15)), ilist, instr);

    translate_insert(INSTR_CREATE_pop(drcontext, opnd_create_reg(DR_REG_RAX)),
        ilist, instr);
}   

inline void after_call(void* drcontext, instrlist_t *ilist, instr_t* instr, 
    bool is_ymm)
{
    translate_insert(INSTR_CREATE_push(drcontext, opnd_create_reg(DR_REG_RAX)),
        ilist, instr);
    
    drmgr_insert_read_tls_field(drcontext, tls_index_first, ilist, instr,
        DR_REG_RAX);

    translate_insert(INSTR_CREATE_vmovupd(drcontext,  
        opnd_create_reg(DR_REG_YMM13),
        opnd_create_base_disp(DR_REG_RAX, DR_REG_NULL, 0, 0, OPSZ_32)), ilist, instr);

    drmgr_insert_read_tls_field(drcontext, tls_index_second, ilist, instr,
        DR_REG_RAX);

    translate_insert(INSTR_CREATE_vmovupd(drcontext, 
        opnd_create_reg(DR_REG_YMM14),
        opnd_create_base_disp(DR_REG_RAX, DR_REG_NULL, 0, 0, OPSZ_32)), ilist, instr);

    drmgr_insert_read_tls_field(drcontext, tls_index_third, ilist, instr,
        DR_REG_RAX);

    translate_insert(INSTR_CREATE_vmovupd(drcontext,  
        opnd_create_reg(DR_REG_YMM15),
        opnd_create_base_disp(DR_REG_RAX, DR_REG_NULL, 0, 0, OPSZ_32)), ilist, instr);

    translate_insert(INSTR_CREATE_pop(drcontext, opnd_create_reg(DR_REG_RAX)),
        ilist, instr);
}

#endif