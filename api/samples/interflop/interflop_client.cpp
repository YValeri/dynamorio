#include "interflop_client.h"

static reg_id_t XMM_REG[] = {DR_REG_XMM0, DR_REG_XMM1, DR_REG_XMM2, DR_REG_XMM3, DR_REG_XMM4, DR_REG_XMM5, DR_REG_XMM6, DR_REG_XMM7, DR_REG_XMM8, DR_REG_XMM9, DR_REG_XMM10, DR_REG_XMM11, DR_REG_XMM12, DR_REG_XMM13, DR_REG_XMM14, DR_REG_XMM15};
static reg_id_t XMM_REG_REVERSE[] = {DR_REG_XMM15, DR_REG_XMM14, DR_REG_XMM13, DR_REG_XMM12, DR_REG_XMM11, DR_REG_XMM10, DR_REG_XMM9, DR_REG_XMM8, DR_REG_XMM7, DR_REG_XMM6, DR_REG_XMM5, DR_REG_XMM4, DR_REG_XMM3, DR_REG_XMM2, DR_REG_XMM1, DR_REG_XMM0};

static reg_id_t YMM_REG[] = {DR_REG_YMM0, DR_REG_YMM1, DR_REG_YMM2, DR_REG_YMM3, DR_REG_YMM4, DR_REG_YMM5, DR_REG_YMM6, DR_REG_YMM7, DR_REG_YMM8, DR_REG_YMM9, DR_REG_YMM10, DR_REG_YMM11, DR_REG_YMM12, DR_REG_YMM13, DR_REG_YMM14, DR_REG_YMM15};
static reg_id_t YMM_REG_REVERSE[] = {DR_REG_YMM15, DR_REG_YMM14, DR_REG_YMM13, DR_REG_YMM12, DR_REG_YMM11, DR_REG_YMM10, DR_REG_YMM9, DR_REG_YMM8, DR_REG_YMM7, DR_REG_YMM6, DR_REG_YMM5, DR_REG_YMM4, DR_REG_YMM3, DR_REG_YMM2, DR_REG_YMM1, DR_REG_YMM0};

static reg_id_t ZMM_REG[] = {DR_REG_ZMM0, DR_REG_ZMM1, DR_REG_ZMM2, DR_REG_ZMM3, DR_REG_ZMM4, DR_REG_ZMM5, DR_REG_ZMM6, DR_REG_ZMM7, DR_REG_ZMM8, DR_REG_ZMM9, DR_REG_ZMM10, DR_REG_ZMM11, DR_REG_ZMM12, DR_REG_ZMM13, DR_REG_ZMM14, DR_REG_ZMM15, DR_REG_ZMM16, DR_REG_ZMM17, DR_REG_ZMM18, DR_REG_ZMM19, DR_REG_ZMM20, DR_REG_ZMM21, DR_REG_ZMM22, DR_REG_ZMM23, DR_REG_ZMM24, DR_REG_ZMM25, DR_REG_ZMM26, DR_REG_ZMM27, DR_REG_ZMM28, DR_REG_ZMM29, DR_REG_ZMM30, DR_REG_ZMM31};
static reg_id_t ZMM_REG_REVERSE[] = {DR_REG_ZMM31, DR_REG_ZMM30, DR_REG_ZMM29, DR_REG_ZMM28, DR_REG_ZMM27, DR_REG_ZMM26, DR_REG_ZMM25, DR_REG_ZMM24, DR_REG_ZMM23, DR_REG_ZMM22, DR_REG_ZMM21, DR_REG_ZMM20, DR_REG_ZMM19, DR_REG_ZMM18, DR_REG_ZMM17, DR_REG_ZMM16, DR_REG_ZMM15, DR_REG_ZMM14, DR_REG_ZMM13, DR_REG_ZMM12, DR_REG_ZMM11, DR_REG_ZMM10, DR_REG_ZMM9, DR_REG_ZMM8, DR_REG_ZMM7, DR_REG_ZMM6, DR_REG_ZMM5, DR_REG_ZMM4, DR_REG_ZMM3, DR_REG_ZMM2, DR_REG_ZMM1, DR_REG_ZMM0};

static int  tls_result /* index of thread local storage to store the result of floating point operations */, 
            tls_op_A, tls_op_B /* index of thread local storage to store the operands of vectorial floating point operations */,
            tls_op_C, /* index of thread local storage to store the third operand in FMA */
            tls_stack /* index of thread local storage to store the address of the shallow stack */;

int get_index_tls_result() {return tls_result;}
int get_index_tls_op_A() {return tls_op_A;}
int get_index_tls_op_B() {return tls_op_B;}
int get_index_tls_op_C() {return tls_op_C;}
int get_index_tls_stack() {return tls_stack;}

void set_index_tls_result(int new_tls_value) {tls_result = new_tls_value;}
void set_index_tls_op_A(int new_tls_value) {tls_op_A = new_tls_value;}
void set_index_tls_op_B(int new_tls_value) {tls_op_B = new_tls_value;}
void set_index_tls_op_C(int new_tls_value) {tls_op_C = new_tls_value;}
void set_index_tls_stack(int new_tls_value) {tls_stack = new_tls_value;}

template <typename FTYPE , FTYPE (*Backend_function)(FTYPE, FTYPE) , int SIMD_TYPE = IFP_OP_SCALAR>
struct interflop_backend {

    static void apply(FTYPE *vect_a,  FTYPE *vect_b) {
       
        static const int vect_size = (SIMD_TYPE == IFP_OP_128) ? 16 : (SIMD_TYPE == IFP_OP_256) ? 32 : (SIMD_TYPE == IFP_OP_512) ? 64 : sizeof(FTYPE);
        static const int nb_elem = vect_size/sizeof(FTYPE);

        FTYPE res;
        
        for(int i = 0 ; i < nb_elem ; i++) {
            res = Backend_function(vect_a[i],vect_b[i]);
            *(((FTYPE*)GET_TLS(dr_get_current_drcontext() , tls_result))+i) = res;
        }   
    }
};


template <typename FTYPE, FTYPE (*Backend_function)(FTYPE, FTYPE, FTYPE) , int SIMD_TYPE = IFP_OP_SCALAR>
struct interflop_backend_fused {
        
        static void apply(FTYPE *vect_a,  FTYPE *vect_b, FTYPE *vect_c) {
       
        static const int vect_size = (SIMD_TYPE == IFP_OP_128) ? 16 : (SIMD_TYPE == IFP_OP_256) ? 32 : (SIMD_TYPE == IFP_OP_512) ? 64 : sizeof(FTYPE);
        static const int nb_elem = vect_size/sizeof(FTYPE);

        FTYPE res;

        for(int i = 0 ; i < nb_elem ; i++) {
            res = Backend_function(vect_a[i] , vect_b[i] , vect_c[i]);
            *(((FTYPE*)GET_TLS(dr_get_current_drcontext() , tls_result))+i) = res;
        }   
    }

   /* static void apply_sub(FTYPE *vect_a,  FTYPE *vect_b, FTYPE *vect_c) {
       
        static const int vect_size = (SIMD_TYPE == IFP_OP_128) ? 16 : (SIMD_TYPE == IFP_OP_256) ? 32 : (SIMD_TYPE == IFP_OP_512) ? 64 : sizeof(FTYPE);
        static const int nb_elem = vect_size/sizeof(FTYPE);

        FTYPE res;

        for(int i = 0 ; i < nb_elem ; i++) {
            res = vect_a[i] * vect_b[i] - vect_c[i];
            *(((FTYPE*)GET_TLS(dr_get_current_drcontext() , tls_result))+i) = res;
        }   
    }
    */

};

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr) {   
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_pop_pseudo_stack(void *drcontext , reg_id_t reg, instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg, reg_id_t temp_buf) {
    
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    // ****************************************************************************
    // Decrement the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_sub(drcontext , OP_REG(buffer_reg) , OP_INT(REG_SIZE(reg))) , bb , instr);

    // ****************************************************************************
    // Load the value in the defined register
    // ****************************************************************************
    if(IS_GPR(reg))
        translate_insert(XINST_CREATE_load(drcontext, OP_REG(reg) , OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg))) , bb , instr); 
    else 
        translate_insert(MOVE_FLOATING_REG((IS_YMM(reg) || IS_ZMM(reg)) , drcontext , OP_REG(reg) , OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg))), bb , instr);

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_pop_pseudo_stack_list(void *drcontext , reg_id_t *reg_to_pop_list , instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg , reg_id_t temp_buf , unsigned int nb_reg) {
    
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    int offset = 0;

    for(unsigned int i = 0 ; i < nb_reg ; i++) {
        offset -= REG_SIZE(reg_to_pop_list[i]);
        if(IS_GPR(reg_to_pop_list[i]))
            translate_insert(XINST_CREATE_load(drcontext, OP_REG(reg_to_pop_list[i]) , OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_pop_list[i]))) , bb , instr); 
        else 
            translate_insert(MOVE_FLOATING_REG((IS_YMM(reg_to_pop_list[i]) || IS_ZMM(reg_to_pop_list[i])) , drcontext , OP_REG(reg_to_pop_list[i]) , OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_pop_list[i]))), bb , instr);
    }

    // ****************************************************************************
    // Decrement the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_add(drcontext , OP_REG(buffer_reg) , OP_INT(offset)) , bb , instr);

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_push_pseudo_stack(void *drcontext , reg_id_t reg_to_push, instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg, reg_id_t temp_buf) {
 
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    // ****************************************************************************
    // Store the value of the register to save
    // ****************************************************************************
    if(IS_GPR(reg_to_push))
        translate_insert(XINST_CREATE_store(drcontext , OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg_to_push)) , OP_REG(reg_to_push)) , bb , instr); 
    else 
        translate_insert(MOVE_FLOATING_REG((IS_YMM(reg_to_push) || IS_ZMM(reg_to_push)) , drcontext , OP_BASE_DISP(buffer_reg , 0 , reg_get_size(reg_to_push)) , OP_REG(reg_to_push)), bb , instr);
   
    // ****************************************************************************
    // Increment the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_add(drcontext , OP_REG(buffer_reg) , OP_INT(REG_SIZE(reg_to_push))) , bb , instr); 

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_push_pseudo_stack_list(void *drcontext , reg_id_t *reg_to_push_list , instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg , reg_id_t temp_buf , unsigned int nb_reg) {
    
    // ****************************************************************************
    // Retrieve top of the stack address in register buffer_reg
    // ****************************************************************************
    INSERT_READ_TLS(drcontext , tls_stack , bb , instr , buffer_reg);

    int offset=0;

    for(unsigned int i = 0 ; i < nb_reg ; i++) {
        if(IS_GPR(reg_to_push_list[i]))
            translate_insert(XINST_CREATE_store(drcontext , OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_push_list[i])) , OP_REG(reg_to_push_list[i])) , bb , instr); 
        else 
            translate_insert(MOVE_FLOATING_REG((IS_YMM(reg_to_push_list[i]) || IS_ZMM(reg_to_push_list[i])) , drcontext , OP_BASE_DISP(buffer_reg , offset , reg_get_size(reg_to_push_list[i])) , OP_REG(reg_to_push_list[i])), bb , instr);

        offset += REG_SIZE(reg_to_push_list[i]);
    }

    // ****************************************************************************
    // Increment the register containing the address of the top of the stack
    // ****************************************************************************
    translate_insert(INSTR_CREATE_add(drcontext , OP_REG(buffer_reg) , OP_INT(offset)) , bb , instr); 

    // ****************************************************************************
    // Update tls field with the new address
    // ****************************************************************************
    INSERT_WRITE_TLS(drcontext , tls_stack , bb , instr , buffer_reg , temp_buf);
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################


void insert_save_floating_reg(void *drcontext , instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg , reg_id_t scratch) {
    if(AVX_512_SUPPORTED) {
        insert_push_pseudo_stack_list(drcontext , ZMM_REG , bb , instr , buffer_reg , scratch , NB_ZMM_REG);
    }
    else if(AVX_2_SUPPORTED) {
        insert_push_pseudo_stack_list(drcontext , YMM_REG , bb , instr , buffer_reg , scratch , NB_YMM_REG);
    }
    else { /* SSE only */
        insert_push_pseudo_stack_list(drcontext , XMM_REG , bb , instr , buffer_reg , scratch , NB_XMM_REG);
    }
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################


void insert_restore_floating_reg(void *drcontext , instrlist_t *bb , instr_t *instr , reg_id_t buffer_reg , reg_id_t scratch) {
    if(AVX_512_SUPPORTED) {
        insert_pop_pseudo_stack_list(drcontext , ZMM_REG_REVERSE , bb , instr , buffer_reg , scratch , NB_ZMM_REG);
    }
    else if(AVX_2_SUPPORTED){
        insert_pop_pseudo_stack_list(drcontext , YMM_REG_REVERSE , bb , instr , buffer_reg , scratch , NB_YMM_REG); 
    }
    else { /* SSE only */
        insert_pop_pseudo_stack_list(drcontext , XMM_REG_REVERSE , bb , instr , buffer_reg , scratch , NB_XMM_REG); 
    }
}


//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_move_operands_to_tls_memory(void *drcontext , instrlist_t *bb , instr_t *instr , OPERATION_CATEGORY oc, bool is_double) {

    /* First set content of the destination regsiter in the tls of the result to save the current value */
    INSERT_READ_TLS(drcontext , tls_result , bb , instr , DR_REG_XDI);
    translate_insert(MOVE_FLOATING_REG((IS_YMM(GET_REG(DST(instr,0))) || IS_ZMM(GET_REG(DST(instr,0)))) , drcontext , OP_BASE_DISP(DR_REG_XDI , 0 , reg_get_size(GET_REG(DST(instr,0)))) , OP_REG(GET_REG(DST(instr,0)))) , bb, instr);

    INSERT_READ_TLS(drcontext , tls_op_A , bb , instr , DR_REG_OP_A_ADDR);
    INSERT_READ_TLS(drcontext , tls_op_B , bb , instr , DR_REG_OP_B_ADDR);

    if(oc & IFP_OP_SCALAR && !(oc & IFP_OP_FUSED)) {
        insert_move_operands_to_tls_memory_scalar(drcontext , bb , instr , oc, is_double);
    }
    else if(oc & IFP_OP_PACKED && !(oc & IFP_OP_FUSED)) {
        insert_move_operands_to_tls_memory_packed(drcontext , bb , instr , oc);
    }
    else if(oc & IFP_OP_FUSED) {
        INSERT_READ_TLS(drcontext , tls_op_C , bb , instr , DR_REG_OP_C_ADDR);
        insert_move_operands_to_tls_memory_fused(drcontext , bb , instr , oc);
    }     
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################


void insert_move_operands_to_tls_memory_scalar(void *drcontext , instrlist_t *bb , instr_t *instr, OPERATION_CATEGORY oc, bool is_double) {

    reg_id_t reg_op_addr[] = {DR_REG_OP_B_ADDR, DR_REG_OP_A_ADDR};

    for(int i = 0 ; i < 2 ; i++) {
        if(OP_IS_BASE_DISP(SRC(instr,i)) || OP_IS_ADDR(SRC(instr,i))) {
            translate_insert(MOVE_FLOATING(is_double , drcontext , OP_REG(DR_REG_XMM_BUFFER) , SRC(instr,i) , SRC(instr,i)), bb, instr);
            translate_insert(MOVE_FLOATING(is_double , drcontext , OP_BASE_DISP(reg_op_addr[i], 0, is_double ? OPSZ(DOUBLE_SIZE) : OPSZ(FLOAT_SIZE)), OP_REG(DR_REG_XMM_BUFFER) , OP_REG(DR_REG_XMM_BUFFER)), bb, instr);
        }
        else if(IS_REG(SRC(instr,i)) ) {
            translate_insert(MOVE_FLOATING(is_double , drcontext , OP_BASE_DISP(reg_op_addr[i], 0, is_double ? OPSZ(DOUBLE_SIZE) : OPSZ(FLOAT_SIZE)), SRC(instr,i) , SRC(instr,i)), bb, instr);
        }
    }
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_move_operands_to_tls_memory_packed(void *drcontext , instrlist_t *bb , instr_t *instr, OPERATION_CATEGORY oc) {

    reg_id_t reg_op_addr[] = {DR_REG_OP_B_ADDR, DR_REG_OP_A_ADDR};

    for(int i = 0 ; i < 2 ; i++) {
        if(OP_IS_ADDR(SRC(instr,i)))
            insert_opnd_addr_to_tls_memory_packed(drcontext , SRC(instr,i), reg_op_addr[i] , bb , instr, oc);
        else if(OP_IS_BASE_DISP(SRC(instr,i))) 
            insert_opnd_base_disp_to_tls_memory_packed(drcontext , SRC(instr,i) , reg_op_addr[i] , bb , instr , oc);
        else if(IS_REG(SRC(instr,i)))
            translate_insert(MOVE_FLOATING_REG((IS_YMM(GET_REG(SRC(instr,i))) || IS_ZMM(GET_REG(SRC(instr,i)))) , drcontext , OP_BASE_DISP(reg_op_addr[i], 0, reg_get_size(GET_REG(SRC(instr,i)))) , SRC(instr,i)), bb , instr);
    }
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_opnd_addr_to_tls_memory_packed(void *drcontext , opnd_t addr_src , reg_id_t base_dst , instrlist_t *bb , instr_t *instr, OPERATION_CATEGORY oc) {
    if(ifp_is_128(oc)) {
        translate_insert(INSTR_CREATE_movupd(drcontext , OP_REG(DR_REG_XMM_BUFFER) , OP_REL_ADDR(GET_ADDR(addr_src))) , bb  , instr);
        translate_insert(INSTR_CREATE_movupd(drcontext , OP_BASE_DISP(base_dst, 0, reg_get_size(DR_REG_XMM_BUFFER)) , OP_REG(DR_REG_XMM_BUFFER)) , bb  , instr);
    }
    else if(ifp_is_256(oc)) {
        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_REG(DR_REG_YMM_BUFFER) , OP_REL_ADDR(GET_ADDR(addr_src))) , bb  , instr);
        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_BASE_DISP(base_dst, 0, reg_get_size(DR_REG_YMM_BUFFER)) , OP_REG(DR_REG_YMM_BUFFER)) , bb  , instr);
    }
    else { /* 512 */
        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_REG(DR_REG_ZMM_BUFFER) , OP_REL_ADDR(GET_ADDR(addr_src))) , bb  , instr);
        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_BASE_DISP(base_dst, 0, reg_get_size(DR_REG_ZMM_BUFFER)) , OP_REG(DR_REG_ZMM_BUFFER)) , bb  , instr);
    }
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_opnd_base_disp_to_tls_memory_packed(void *drcontext , opnd_t base_disp_src , reg_id_t base_dst , instrlist_t *bb , instr_t *instr , OPERATION_CATEGORY oc) {
    if(ifp_is_128(oc)) {
        translate_insert(INSTR_CREATE_movupd(drcontext , OP_REG(DR_REG_XMM_BUFFER) , OP_BASE_DISP(opnd_get_base(base_disp_src) , opnd_get_disp(base_disp_src), reg_get_size(DR_REG_XMM_BUFFER))) , bb  , instr);
        translate_insert(INSTR_CREATE_movupd(drcontext , OP_BASE_DISP(base_dst, 0, reg_get_size(DR_REG_XMM_BUFFER)) , OP_REG(DR_REG_XMM_BUFFER)) , bb  , instr);
    }
    else if(ifp_is_256(oc)) {
        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_REG(DR_REG_YMM_BUFFER) , OP_BASE_DISP(opnd_get_base(base_disp_src) , opnd_get_disp(base_disp_src), reg_get_size(DR_REG_YMM_BUFFER))) , bb  , instr);
        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_BASE_DISP(base_dst, 0, reg_get_size(DR_REG_YMM_BUFFER)) , OP_REG(DR_REG_YMM_BUFFER)) , bb  , instr);
    }
    else if(ifp_is_512(oc)){ /* 512 */
        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_REG(DR_REG_ZMM_BUFFER) , OP_BASE_DISP(opnd_get_base(base_disp_src) , opnd_get_disp(base_disp_src), reg_get_size(DR_REG_ZMM_BUFFER))) , bb  , instr);
        translate_insert(INSTR_CREATE_vmovupd(drcontext , OP_BASE_DISP(base_dst, 0, reg_get_size(DR_REG_ZMM_BUFFER)) , OP_REG(DR_REG_ZMM_BUFFER)) , bb  , instr);
    }
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_move_operands_to_tls_memory_fused(void *drcontext , instrlist_t *bb , instr_t *instr, OPERATION_CATEGORY oc) {
    
    reg_id_t reg_op_addr[] = {DR_REG_OP_C_ADDR, DR_REG_OP_B_ADDR, DR_REG_OP_A_ADDR};
    
    int index[3];

    if(oc & IFP_OP_213) {
        index[0] = 1;
        index[1] = 0;
        index[2] = 2;
    }
    else if(oc & IFP_OP_231) {
        index[0] = 1;
        index[1] = 2;
        index[2] = 0;
    }
    else if(oc & IFP_OP_132) {
        index[0] = 0;
        index[1] = 2;
        index[2] = 1;
    }

    for(int i = 0 ; i < 3 ; i++) {
        if(OP_IS_ADDR(SRC(instr,i))) {
            insert_opnd_addr_to_tls_memory_packed(drcontext , SRC(instr,i), reg_op_addr[index[i]] , bb , instr, oc);
        }
        else if(OP_IS_BASE_DISP(SRC(instr,i))) {
            insert_opnd_base_disp_to_tls_memory_packed(drcontext , SRC(instr,i) , reg_op_addr[index[i]] , bb , instr , oc);
        }
        else if(IS_REG(SRC(instr,i))) {
            translate_insert(MOVE_FLOATING_REG((IS_YMM(GET_REG(SRC(instr,i))) || IS_ZMM(GET_REG(SRC(instr,i)))) , drcontext , OP_BASE_DISP(reg_op_addr[index[i]], 0, reg_get_size(GET_REG(SRC(instr,i)))) , SRC(instr,i)), bb , instr);
        }
    }
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

template <typename FTYPE , FTYPE (*Backend_function)(FTYPE, FTYPE)>
void insert_corresponding_vect_call(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    switch(oc & IFP_SIMD_TYPE_MASK)
    {
        case IFP_OP_128:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend<FTYPE, Backend_function, IFP_OP_128>::apply , 0);
        break;
        case IFP_OP_256:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend<FTYPE, Backend_function, IFP_OP_256>::apply , 0);
        break;
        case IFP_OP_512:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend<FTYPE, Backend_function, IFP_OP_512>::apply , 0);
        break;
        default: /*SCALAR */
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend<FTYPE, Backend_function>::apply , 0);
    }
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################


template <typename FTYPE , FTYPE (*Backend_function)(FTYPE, FTYPE, FTYPE)>
void insert_corresponding_vect_call_fused(void* drcontext, instrlist_t *bb, instr_t* instr,OPERATION_CATEGORY oc)
{
    switch(oc & IFP_SIMD_TYPE_MASK)
    {
        case IFP_OP_128:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend_fused<FTYPE, Backend_function, IFP_OP_128>::apply , 0);
        break;

        case IFP_OP_256:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend_fused<FTYPE, Backend_function,  IFP_OP_256>::apply , 0);
        break;
        case IFP_OP_512:
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend_fused<FTYPE, Backend_function,  IFP_OP_512>::apply , 0);
        break;

        default: /*SCALAR */
            dr_insert_call(drcontext , bb , instr , (void*)interflop_backend_fused<FTYPE, Backend_function>::apply , 0);
    }
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_call(void *drcontext , instrlist_t *bb , instr_t *instr , OPERATION_CATEGORY oc , bool is_double) {
    
    if(oc & IFP_OP_FUSED) {
        if(oc & IFP_OP_FMA) {
            if(is_double)
                insert_corresponding_vect_call_fused<double, Interflop::Op<double>::fmadd>(drcontext , bb , instr, oc);
            else
                insert_corresponding_vect_call_fused<float, Interflop::Op<float>::fmadd>(drcontext , bb , instr, oc);
        }
        else if(oc & IFP_OP_FMS) {
            if(is_double)
                insert_corresponding_vect_call_fused<double, Interflop::Op<double>::fmsub>(drcontext , bb , instr, oc);
            else
                insert_corresponding_vect_call_fused<float, Interflop::Op<float>::fmsub>(drcontext , bb , instr, oc);
        }
    }
    else {
        switch (oc & IFP_OP_TYPE_MASK)
        {
            case IFP_OP_ADD:
                if(is_double)
                    insert_corresponding_vect_call<double, Interflop::Op<double>::add>(drcontext , bb , instr, oc);
                else
                    insert_corresponding_vect_call<float, Interflop::Op<float>::add>(drcontext , bb , instr, oc);
            break;
            case IFP_OP_SUB:
                if(is_double)
                    insert_corresponding_vect_call<double, Interflop::Op<double>::sub>(drcontext , bb , instr, oc);
                else
                    insert_corresponding_vect_call<float, Interflop::Op<float>::sub>(drcontext , bb , instr, oc);
            break;
            case IFP_OP_MUL:
                if(is_double)
                    insert_corresponding_vect_call<double, Interflop::Op<double>::mul>(drcontext , bb , instr, oc);
                else
                    insert_corresponding_vect_call<float, Interflop::Op<float>::mul>(drcontext , bb , instr, oc);
            break;
            case IFP_OP_DIV:
                if(is_double)
                    insert_corresponding_vect_call<double, Interflop::Op<double>::div>(drcontext , bb , instr, oc);
                else
                    insert_corresponding_vect_call<float, Interflop::Op<float>::div>(drcontext , bb , instr, oc);    
            break;
            default:
                PRINT_ERROR_MESSAGE("ERROR OPERATION NOT FOUND !");
        }
    }    
}

//######################################################################################################################################################################################
//######################################################################################################################################################################################
//######################################################################################################################################################################################

void insert_set_result_in_corresponding_register(void *drcontext , instrlist_t *bb , instr_t *instr, bool is_double , bool is_scalar) {
    INSERT_READ_TLS(drcontext , tls_result , bb , instr , DR_REG_RES_ADDR);
    translate_insert(MOVE_FLOATING_REG((IS_YMM(GET_REG(DST(instr,0))) || IS_ZMM(GET_REG(DST(instr,0)))) , drcontext , DST(instr,0) , OP_BASE_DISP(DR_REG_RES_ADDR , 0 , reg_get_size(GET_REG(DST(instr,0))))), bb , instr);
}