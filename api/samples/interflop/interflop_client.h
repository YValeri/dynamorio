/*
* Created on 2019-07-23
*/

#ifndef INTERFLOP_CLIENT_H
#define INTERFLOP_CLIENT_H

#include "dr_api.h"
#include "drmgr.h"
#include "backend/backend.hxx"
#include "interflop_operations.hpp"

#ifndef MAX_OPND_SIZE_BYTES
// operand size is maximum 512 bits (AVX512 instructions) = 64 bytes 
	#define MAX_OPND_SIZE_BYTES 512 
#endif

#define ERROR(message) dr_fprintf(STDERR, "%s\n", (message));


#if defined(WINDOWS)
    #define DR_REG_OP_A_ADDR DR_REG_XCX
    #define DR_REG_OP_B_ADDR DR_REG_XDX
    #define DR_REG_RES_ADDR DR_REG_XDI
#elif defined(AARCH64)
    #define DR_REG_OP_A_ADDR DR_REG_X0
    #define DR_REG_OP_B_ADDR DR_REG_X1
    #define DR_REG_RES_ADDR DR_REG_X0
#else
    #define DR_REG_OP_A_ADDR DR_REG_XDI
    #define DR_REG_OP_B_ADDR DR_REG_XSI
    #define DR_REG_RES_ADDR DR_REG_XDI
#endif

#if defined(X86)
    #define DR_REG_XMM_BUFFER DR_REG_XMM15
    #define DR_REG_YMM_BUFFER DR_REG_YMM15
    #define DR_REG_ZMM_BUFFER DR_REG_ZMM31

    #define AVX_2_SUPPORTED (proc_has_feature(FEATURE_AVX2))
    #define AVX_512_SUPPORTED (proc_has_feature(FEATURE_AVX512F))

    #define DR_BUFFER_REG DR_REG_XCX
    #define DR_SCRATCH_REG DR_REG_XDX
#elif defined(AARCH64)
    #define DR_REG_MULTIPLE DR_REG_Q31
    #define DR_REG_FLOAT DR_REG_S31
    #define DR_REG_DOUBLE DR_REG_D31

    #define DR_BUFFER_REG DR_REG_R7
    #define DR_SCRATCH_REG DR_REG_R6
#endif

#define SPILL_SLOT_BUFFER_REG SPILL_SLOT_1
#define SPILL_SLOT_SCRATCH_REG SPILL_SLOT_2
#define SPILL_SLOT_ARITH_FLAG SPILL_SLOT_3

/* INSTR */
#define SRC(instr,n) instr_get_src((instr),(n))
#define DST(instr,n) instr_get_dst((instr),(n))

#if defined(X86)
    #define MOVE_FLOATING_SCALAR(is_double, drcontext, dest, srcd, srcs) (is_double) ? 	\
	INSTR_CREATE_movsd((drcontext), (dest), (srcd)) : 			  	\
	INSTR_CREATE_movss((drcontext), (dest), (srcs))
    #define MOVE_FLOATING_PACKED(is_avx, drcontext, dest, src) (is_avx) ? 		\
	INSTR_CREATE_vmovupd((drcontext), (dest), (src)) : 				\
	INSTR_CREATE_movupd((drcontext), (dest), (src))
#endif

/* CREATE OPND */
#define OP_REG(reg_id) opnd_create_reg((reg_id))
#define OP_INT(val) opnd_create_immed_int((val), OPSZ_4)

#if defined(X86)
	#define OP_REL_ADDR(addr) \
		opnd_create_rel_addr((addr), OPSZ_8)
	#define OP_BASE_DISP(base,disp,size) \
		opnd_create_base_disp((base), DR_REG_NULL, 0, (disp), (size))
#elif defined(AARCH64)
	#define OP_REL_ADDR(addr) \
		opnd_create_rel_addr((addr), OPSZ_2)
	#define OP_BASE_DISP(base, disp, size) \
		opnd_create_base_disp_aarch64((base), DR_REG_NULL, 0, false, (disp), 0, (size))
#endif

/* TESTS OPND */
#define IS_REG(opnd) opnd_is_reg((opnd))
#define OP_IS_BASE_DISP(opnd) opnd_is_base_disp((opnd))
#define OP_IS_REL_ADDR(opnd) opnd_is_rel_addr((opnd))
#define OP_IS_ABS_ADDR(opnd) opnd_is_abs_addr((opnd))
#define OP_IS_ADDR(opnd) (OP_IS_REL_ADDR((opnd)) || OP_IS_ABS_ADDR((opnd)))

/* TESTS REG */
#define IS_GPR(reg) reg_is_gpr((reg))

#if defined(X86)
	#define IS_XMM(reg) reg_is_strictly_xmm((reg))
	#define IS_YMM(reg) reg_is_strictly_ymm((reg))
	#define IS_ZMM(reg) reg_is_strictly_zmm((reg))
#elif defined(AARCH64)
	#define IS_SIMD(reg) reg_is_simd((reg))
#endif

/* OPND GET VALUE */
#define GET_REG(opnd) opnd_get_reg((opnd)) /* if opnd is a register */
#define GET_ADDR(opnd) opnd_get_addr((opnd)) /*if opnd is an address */

/* TLS OPERATIONS */
#define GET_TLS(drcontext,tls) drmgr_get_tls_field((drcontext), (tls))
#define SET_TLS(drcontext,tls,value) drmgr_set_tls_field((drcontext), (tls), (void*)(value))
#define INSERT_READ_TLS(drcontext, tls, bb, instr, reg) \
		drmgr_insert_read_tls_field((drcontext), (tls), (bb), (instr), (reg))
#define INSERT_WRITE_TLS(drcontext, tls, bb, instr, reg, temp_reg) \
		drmgr_insert_write_tls_field((drcontext), (tls_stack), (bb), (instr), (buffer_reg), (temp_reg));

/* SIZES */
#define SIZE_STACK 4096
#define PTR_SIZE sizeof(void*)
#define FLOAT_SIZE sizeof(float)
#define DOUBLE_SIZE sizeof(double)
#define REG_SIZE(reg) opnd_size_in_bytes(reg_get_size((reg)))
#define OPSZ(bytes) opnd_size_from_bytes((bytes))

typedef byte SLOT;

#if defined(X86)
	#define NB_XMM_REG 16
	#define NB_YMM_REG 16
	#define NB_ZMM_REG 32
#elif defined(AARCH64)
	#define NB_Q_REG 32
#endif

int get_index_tls_result();
int get_index_tls_op_A();
int get_index_tls_op_B();
int get_index_tls_stack();

void set_index_tls_result(int new_tls_value);
void set_index_tls_op_A(int new_tls_value);
void set_index_tls_op_B(int new_tls_value);
void set_index_tls_stack(int new_tls_value);

void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr);

void insert_pop_pseudo_stack(void *drcontext, reg_id_t reg, instrlist_t *bb, instr_t *instr, reg_id_t buffer_reg, reg_id_t temp_buf);
void insert_pop_pseudo_stack_list(void *drcontext, reg_id_t *reg_to_pop_list, instrlist_t *bb, instr_t *instr, reg_id_t buffer_reg, reg_id_t temp_buf, unsigned int nb_reg);

void insert_push_pseudo_stack(void *drcontext, reg_id_t reg_to_push, instrlist_t *bb, instr_t *instr, reg_id_t buffer_reg, reg_id_t temp_buf);
void insert_push_pseudo_stack_list(void *drcontext, reg_id_t *reg_to_push_list, instrlist_t *bb, instr_t *instr, reg_id_t buffer_reg, reg_id_t temp_buf, unsigned int nb_reg);

void insert_move_operands_to_tls_memory_scalar(void *drcontext , instrlist_t *bb , instr_t *instr, OPERATION_CATEGORY oc, bool is_double);
void insert_move_operands_to_tls_memory_packed(void *drcontext , instrlist_t *bb , instr_t *instr, OPERATION_CATEGORY oc);
void insert_move_operands_to_tls_memory(void *drcontext , instrlist_t *bb , instr_t *instr , OPERATION_CATEGORY oc, bool is_double);

void insert_save_floating_reg(void *drcontext, instrlist_t *bb, instr_t *instr, reg_id_t buffer_reg, reg_id_t scratch);
void insert_restore_floating_reg(void *drcontext, instrlist_t *bb, instr_t *instr, reg_id_t buffer_reg, reg_id_t scratch);

void insert_call(void *drcontext, instrlist_t *bb, instr_t *instr, OPERATION_CATEGORY oc, bool is_double);

void insert_set_result_in_corresponding_register(void *drcontext, instrlist_t *bb, instr_t *instr, bool is_double, bool is_scalar);

void insert_opnd_base_disp_to_tls_memory_packed(void *drcontext , opnd_t opnd_src , reg_id_t base_dst , instrlist_t *bb , instr_t *instr , OPERATION_CATEGORY oc);
void insert_opnd_addr_to_tls_memory_packed(void *drcontext , opnd_t addr_src , reg_id_t base_dst , instrlist_t *bb , instr_t *instr, OPERATION_CATEGORY oc);

#endif