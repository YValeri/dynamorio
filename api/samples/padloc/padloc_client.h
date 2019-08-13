/*
* Created on 2019-07-23
*/

#ifndef PADLOC_CLIENT_H
#define PADLOC_CLIENT_H

#include "dr_api.h"
#include "drmgr.h"
#include "backend/backend.hxx"
#include "padloc_operations.hpp"

#ifndef MAX_OPND_SIZE_BYTES
// operand size is maximum 512 bits (AVX512 instructions) = 64 bytes 
	#define MAX_OPND_SIZE_BYTES 64 
#endif

#define PRINT_ERROR_MESSAGE(message) dr_fprintf(STDERR, "%s\n", (message));

#ifdef WINDOWS
    #define DR_REG_OP_A_ADDR DR_REG_XCX
    #define DR_REG_OP_B_ADDR DR_REG_XDX
    #define DR_REG_OP_C_ADDR DR_REG_R8
    #define DR_REG_BASE DR_REG_XAX
#elif defined(AARCH64)
	#define DR_REG_OP_A_ADDR DR_REG_X0
	#define DR_REG_OP_B_ADDR DR_REG_X1
	#define DR_REG_OP_C_ADDR DR_REG_X2
	#define DR_REG_RES_ADDR DR_REG_X3
#else
    #define DR_REG_OP_A_ADDR DR_REG_XDI
    #define DR_REG_OP_B_ADDR DR_REG_XSI
    #define DR_REG_OP_C_ADDR DR_REG_XDX
    #define DR_REG_RES_ADDR DR_REG_XDI
    #define DR_REG_BASE DR_REG_XAX
#endif

#if defined(X86)
    #define DR_SCRATCH_REG DR_REG_XCX

    #define AVX_SUPPORTED (proc_has_feature(FEATURE_AVX))
    #define AVX_512_SUPPORTED (proc_has_feature(FEATURE_AVX512F))

#elif defined(AARCH64)
	#define DR_REG_MULTIPLE DR_REG_Q31
	#define DR_REG_FLOAT DR_REG_S31
	#define DR_REG_DOUBLE DR_REG_D31

	#define DR_BUFFER_REG DR_REG_X6
	#define DR_SCRATCH_REG DR_REG_X5
#endif

#define SPILL_SLOT_SCRATCH_REG SPILL_SLOT_1

/* INSTR */
#define SRC(instr, n) instr_get_src((instr), (n))
#define DST(instr, n) instr_get_dst((instr), (n))

#if defined(X86)
	#define MOVE_FLOATING_SCALAR(is_double, drcontext, dest, srcd, srcs) (is_double) ? 	\
		INSTR_CREATE_movsd((drcontext), (dest), (srcd)) : 			  					\
		INSTR_CREATE_movss((drcontext), (dest), (srcs))
	
	#define MOVE_FLOATING_PACKED(is_avx, drcontext, dest, src) (is_avx) ? 				\
		INSTR_CREATE_vmovupd((drcontext), (dest), (src)) : 								\
		INSTR_CREATE_movupd((drcontext), (dest), (src))
#endif

/* CREATE OPND */
#define OP_REG(reg_id) opnd_create_reg((reg_id))
#define OP_INT(val) opnd_create_immed_int((val), OPSZ_4)
#define OP_INT64(val) opnd_create_immed_int64((val), OPSZ_8)
	
#if defined(X86)
	#define OP_REL_ADDR(addr) \
		opnd_create_rel_addr((addr), OPSZ_8)
	#define OP_BASE_DISP(base, disp, size) \
		opnd_create_base_disp((base), DR_REG_NULL, 0, (disp), (size))
#elif defined(AARCH64)
	#define OP_REL_ADDR(addr) \
		opnd_create_rel_addr((addr), OPSZ_2)
	#define OP_BASE_DISP(base, disp, size) \
		opnd_create_base_disp((base), DR_REG_NULL, 0, (disp), (size))
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
#define GET_TLS(drcontext, tls) drmgr_get_tls_field((drcontext), (tls))
#define SET_TLS(drcontext, tls, value) drmgr_set_tls_field((drcontext), (tls), (void*)(value))
#define INSERT_READ_TLS(drcontext, tls, bb, instr, reg) \
		drmgr_insert_read_tls_field((drcontext), (tls), (bb), (instr), (reg))
#define INSERT_WRITE_TLS(drcontext, tls, bb, instr, reg, temp_reg) \
		drmgr_insert_write_tls_field((drcontext), (tls_stack), (bb), (instr), (buffer_reg), (temp_reg));

/* SIZES */
#define REG_SIZE(reg) opnd_size_in_bytes(reg_get_size((reg)))
#define OPSZ(bytes) opnd_size_from_bytes((bytes))

typedef byte SLOT;

#if defined(X86)
	#define NB_XMM_REG 16
	#define NB_YMM_REG 16
	#define NB_ZMM_REG 32
#elif defined(AARCH64)
	#define NB_Q_REG 0
#endif

/**
 * \brief Returns the index of the floating point registers tls
 */
int get_index_tls_float();
/**
 * \brief Returns the index of the gpr tls
 */
int get_index_tls_gpr();
/**
 * \brief Returns the index of the result tls
 */
int get_index_tls_result();

/**
 * \brief Sets the index of the gpr tls
 */
void set_index_tls_gpr(int new_tls_value);
/**
 * \brief Sets the index of the floating point registers tls
 */
void set_index_tls_float(int new_tls_value);
/**
 * \brief Sets the index of the result tls
 */
void set_index_tls_result(int new_tls_value);

/**
 * \brief Inserts \param newinstr in \param ilist prior to \param instr and set it as an application instruction
 */
void translate_insert(instr_t *newinstr, instrlist_t *ilist, instr_t *instr);


/**
 * \brief Insert the corresponding call into the application depending on the properties of the instruction overloaded.  
 * 
 * \param drcontext DynamoRIO's context
 * \param bb Current basic block
 * \param instr Instrumented instruction
 * \param oc Operation category of the instruction
 * \param is_double True if the baseline precision is double
 */
void insert_call(void *drcontext, instrlist_t *bb, instr_t *instr, OPERATION_CATEGORY oc, bool is_double);

/**
 * \brief Inserts prior to \p where meta-instructions to set the calling convention registers to the right adresses
 * Assumes the GPR have been saved !
 * \param drcontext DynamoRIO's context
 * \param bb Current Basic Block
 * \param where instruction prior to whom we insert the meta-instructions 
 * \param instr Instrumented instruction
 * \param oc Operation category of the instrumented instruction
 */
void insert_set_operands(void* drcontext, instrlist_t *bb, instr_t *where, instr_t *instr, OPERATION_CATEGORY oc);

/**
 * \brief Inserts prior to \p where meta-instructions to restore the floating point registers (xmm-ymm-zmm)
 * Assumes the gpr have been saved beforehand !
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_restore_simd_registers(void *drcontext, instrlist_t *bb, instr_t *where);

/**
 * \brief Inserts prior to \p where meta-instructions to save the floating point registers (xmm-ymm-zmm)
 * Assumes the gpr have been saved beforehand !
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_save_simd_registers(void *drcontext, instrlist_t *bb, instr_t *where);

/**
 * \brief Prepares the address in the buffer of the tls register to point to the destination register in memory
 * Assumes the gpr have been saved beforehand !
 * \param drcontext DynamoRIO's context
 * \param bb Current basic block
 * \param where instruction prior to whom we insert the meta-instructions 
 * \param destination SIMD registers that should have received the result
 */
void insert_set_destination_tls(void *drcontext, instrlist_t *bb, instr_t *where, reg_id_t destination);

/**
 * \brief Inserts prior to \param where meta-instructions to restore the arithmetic flags and the gpr registers
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_restore_gpr_and_flags(void *drcontext, instrlist_t *bb, instr_t *where);

/**
 * \brief Inserts prior to \param where meta-instructions to save the arithmetic flags and the gpr registers
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_save_gpr_and_flags(void *drcontext, instrlist_t *bb, instr_t *where);

/**
 * \brief Inserts prior to \p where meta-instructions to restore RSP from its saved value
 * 
 * \param drcontext DynamoRIO's context
 * \param bb Current Basic Block
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_restore_rsp(void *drcontext, instrlist_t *bb, instr_t *where);

#endif
