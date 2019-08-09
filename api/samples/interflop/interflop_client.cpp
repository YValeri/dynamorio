#include <cstdint>
#include "interflop_client.h"


#if defined(X86)
    #define INSTR_IS_ANY_SSE(instr) (instr_is_sse(instr) || instr_is_sse2(instr) || instr_is_sse3(instr) || instr_is_sse41(instr) || instr_is_sse42(instr) || instr_is_sse4A(instr))
	static reg_id_t XMM_REG[] = {
		DR_REG_XMM0, DR_REG_XMM1, DR_REG_XMM2, DR_REG_XMM3, DR_REG_XMM4, 
		DR_REG_XMM5, DR_REG_XMM6, DR_REG_XMM7, DR_REG_XMM8, DR_REG_XMM9, 
		DR_REG_XMM10, DR_REG_XMM11, DR_REG_XMM12, DR_REG_XMM13, DR_REG_XMM14, 
		DR_REG_XMM15
	};
	static reg_id_t XMM_REG_REVERSE[] = {
		DR_REG_XMM15, DR_REG_XMM14, DR_REG_XMM13, DR_REG_XMM12, DR_REG_XMM11, 
		DR_REG_XMM10, DR_REG_XMM9, DR_REG_XMM8, DR_REG_XMM7, DR_REG_XMM6, 
		DR_REG_XMM5, DR_REG_XMM4, DR_REG_XMM3, DR_REG_XMM2, DR_REG_XMM1, 
		DR_REG_XMM0
	};

	static reg_id_t YMM_REG[] = {
		DR_REG_YMM0, DR_REG_YMM1, DR_REG_YMM2, DR_REG_YMM3, DR_REG_YMM4, 
		DR_REG_YMM5, DR_REG_YMM6, DR_REG_YMM7, DR_REG_YMM8, DR_REG_YMM9, 
		DR_REG_YMM10, DR_REG_YMM11, DR_REG_YMM12, DR_REG_YMM13, DR_REG_YMM14, 
		DR_REG_YMM15
	};
	static reg_id_t YMM_REG_REVERSE[] = {
		DR_REG_YMM15, DR_REG_YMM14, DR_REG_YMM13, DR_REG_YMM12, DR_REG_YMM11, 
		DR_REG_YMM10, DR_REG_YMM9, DR_REG_YMM8, DR_REG_YMM7, DR_REG_YMM6, 
		DR_REG_YMM5, DR_REG_YMM4, DR_REG_YMM3, DR_REG_YMM2, DR_REG_YMM1, 
		DR_REG_YMM0
	};

	static reg_id_t ZMM_REG[] = {
		DR_REG_ZMM0, DR_REG_ZMM1, DR_REG_ZMM2, DR_REG_ZMM3, DR_REG_ZMM4, 
		DR_REG_ZMM5, DR_REG_ZMM6, DR_REG_ZMM7, DR_REG_ZMM8, DR_REG_ZMM9, 
		DR_REG_ZMM10, DR_REG_ZMM11, DR_REG_ZMM12, DR_REG_ZMM13, DR_REG_ZMM14, 
		DR_REG_ZMM15, DR_REG_ZMM16, DR_REG_ZMM17, DR_REG_ZMM18, DR_REG_ZMM19, 
		DR_REG_ZMM20, DR_REG_ZMM21, DR_REG_ZMM22, DR_REG_ZMM23, DR_REG_ZMM24, 
		DR_REG_ZMM25, DR_REG_ZMM26, DR_REG_ZMM27, DR_REG_ZMM28, DR_REG_ZMM29, 
		DR_REG_ZMM30, DR_REG_ZMM31
	};
	static reg_id_t ZMM_REG_REVERSE[] = {
		DR_REG_ZMM31, DR_REG_ZMM30, DR_REG_ZMM29, DR_REG_ZMM28, DR_REG_ZMM27, 
		DR_REG_ZMM26, DR_REG_ZMM25, DR_REG_ZMM24, DR_REG_ZMM23, DR_REG_ZMM22, 
		DR_REG_ZMM21, DR_REG_ZMM20, DR_REG_ZMM19, DR_REG_ZMM18, DR_REG_ZMM17, 
		DR_REG_ZMM16, DR_REG_ZMM15, DR_REG_ZMM14, DR_REG_ZMM13, DR_REG_ZMM12, 
		DR_REG_ZMM11, DR_REG_ZMM10, DR_REG_ZMM9, DR_REG_ZMM8, DR_REG_ZMM7, 
		DR_REG_ZMM6, DR_REG_ZMM5, DR_REG_ZMM4, DR_REG_ZMM3, DR_REG_ZMM2, 
		DR_REG_ZMM1, DR_REG_ZMM0
	};
#elif defined(AARCH64)
	static reg_id_t Q_REG[] = {
		DR_REG_Q0, DR_REG_Q1, DR_REG_Q2, DR_REG_Q3, DR_REG_Q4, 
		DR_REG_Q5, DR_REG_Q6, DR_REG_Q7, DR_REG_Q8, DR_REG_Q9, 
		DR_REG_Q10, DR_REG_Q11, DR_REG_Q12, DR_REG_Q13, DR_REG_Q14, 
		DR_REG_Q15, DR_REG_Q16, DR_REG_Q17, DR_REG_Q18, DR_REG_Q19, 
		DR_REG_Q20, DR_REG_Q21, DR_REG_Q22, DR_REG_Q23, DR_REG_Q24, 
		DR_REG_Q25, DR_REG_Q26, DR_REG_Q27, DR_REG_Q28, DR_REG_Q29, 
		DR_REG_Q30, DR_REG_Q31
	};
	static reg_id_t Q_REG_REVERSE[] = {
		DR_REG_Q31, DR_REG_Q30, DR_REG_Q29, DR_REG_Q28, DR_REG_Q27, 
		DR_REG_Q26, DR_REG_Q25, DR_REG_Q24, DR_REG_Q23, DR_REG_Q22, 
		DR_REG_Q21, DR_REG_Q20, DR_REG_Q19, DR_REG_Q18, DR_REG_Q17, 
		DR_REG_Q16, DR_REG_Q15, DR_REG_Q14, DR_REG_Q13, DR_REG_Q12, 
		DR_REG_Q11, DR_REG_Q10, DR_REG_Q9, DR_REG_Q8, DR_REG_Q7, 
		DR_REG_Q6, DR_REG_Q5, DR_REG_Q4, DR_REG_Q3, DR_REG_Q2, 
		DR_REG_Q1, DR_REG_Q0
	};
#endif

#define R(name) DR_REG_##name
#if defined(X86) && defined(X64)

    static const reg_id_t GPR_ORDER[] = {R(NULL), R(XAX), R(XCX), R(XDX), R(XBX), R(XSP), R(XBP), R(XSI), R(XDI), R(R8), R(R9), R(R10), R(R11), R(R12), R(R13), R(R14), R(R15)};
    #define NUM_GPR_SLOTS 17
#elif defined(AArch64)
    //TODO Add GPR for Aarch64
#endif
#undef R


static int  tls_result, /* index of thread local storage to store the result of floating point operations */
			tls_op_A, tls_op_B, /* index of thread local storage to store the operands of vectorial floating point operations */
			tls_op_C, /* index of thread local storage to store the third operand in FMA */
			tls_stack, /* index of thread local storage to store the address of the shallow stack */
			tls_saved_reg; /* index of the tls where we save the arithmetic flags, rax and the scratch register */

static int tls_gpr, tls_float;

int get_index_tls_gpr(){return tls_gpr;}
int get_index_tls_float(){return tls_float;}

void set_index_tls_gpr(int new_tls_value) {tls_gpr = new_tls_value;}
void set_index_tls_float(int new_tls_value) {tls_float = new_tls_value;}


int get_index_tls_result() {return tls_result;}
int get_index_tls_op_A() {return tls_op_A;}
int get_index_tls_op_B() {return tls_op_B;}
int get_index_tls_op_C() {return tls_op_C;}
int get_index_tls_stack() {return tls_stack;}
int get_index_tls_saved_reg(){return tls_saved_reg;}

void set_index_tls_result(int new_tls_value) {tls_result = new_tls_value;}
void set_index_tls_op_A(int new_tls_value) {tls_op_A = new_tls_value;}
void set_index_tls_op_B(int new_tls_value) {tls_op_B = new_tls_value;}
void set_index_tls_op_C(int new_tls_value) {tls_op_C = new_tls_value;}
void set_index_tls_stack(int new_tls_value) {tls_stack = new_tls_value;}
void set_index_tls_saved_reg(int new_tls_value) {tls_saved_reg = new_tls_value;}

/**
 * \brief Returns the offset, in bytes, of the gpr stored in the tls
 * 
 * \param gpr 
 * \return int 
 */
inline int offset_of_gpr(reg_id_t gpr)
{
    //Assuming the gpr parameter is a valid gpr
    return (((int)gpr-DR_REG_START_GPR)+1)<<3;
}

/**
 * \brief Returns the offset, in bytes, of the simd register in the tls
 * 
 * \param simd 
 * \return int 
 */
inline int offset_of_simd(reg_id_t simd)
{
    //Assuming the simd parameter is a valid simd register
    static const int OFFSET = AVX_512_SUPPORTED ? 6 : AVX_SUPPORTED ? 5 : 4 /*128 bits = 16 bytes = 2^4 */;
    const reg_id_t START = reg_is_strictly_zmm(simd) ? DR_REG_START_ZMM : reg_is_strictly_ymm(simd) ? DR_REG_START_YMM : DR_REG_START_XMM;
    return ((uint)simd-START)<<OFFSET;
}

/*
 * \brief Definition of intermediary functions between front-end and back-end
 * \brief It is suited for two sources instructions 
 * 
 * FTYPE : float or double depending on the precision of the instruction 
 * Backend_function : the function corresponding to the overlaoded operation (add, sub, fmadd ...) : implementation in Backend.hxx
 * INSTR_CATEGORY : Make the difference SSE and AVX instructions. For AVX instructions targeting low parts of XMM/YMM/ZMM registers, the high part has to be set to 0
 * 		- possible values are IFP_OP_SSE and IFP_OP_AVX
 * SIMD_TYPE : Define the length of the elements of the operation. 
 * 		- default is IFP_OP_SCALAR 
 * 		- possible values are IFP_OP_128, IFP_OP_256 and IFP_OP_512
 * Note that INSTR_CATEGORY and SIMD_TYPE are both mandatory since AVX instructions can be applied on 128 sized registers (XMM)
 */
template <typename FTYPE , FTYPE (*Backend_function)(FTYPE, FTYPE) , int INSTR_CATEGORY , int SIMD_TYPE = IFP_OP_SCALAR>
struct interflop_backend {

	static void apply(FTYPE *vect_a,  FTYPE *vect_b) {

		static const int operation_size = (SIMD_TYPE == IFP_OP_128) ? 16 : (SIMD_TYPE == IFP_OP_256) ? 32 : (SIMD_TYPE == IFP_OP_512) ? 64 : sizeof(FTYPE);
		static const int nb_elem = operation_size/sizeof(FTYPE);

		static const int max_operation_size = (INSTR_CATEGORY == IFP_OP_SSE) ? 16 : (INSTR_CATEGORY == IFP_OP_AVX) ? 32 : sizeof(FTYPE);
		static const int max_nb_elem = max_operation_size/sizeof(FTYPE);
	    
		FTYPE res;

        FTYPE* tls = *(FTYPE**)GET_TLS(dr_get_current_drcontext(), tls_result);
        
        for(int i = 0 ; i < nb_elem ; i++) {
			#if defined(X86)
				res = Backend_function(vect_a[i], vect_b[i]);
			#elif defined(AARCH64)
				res = Backend_function(vect_b[i], vect_a[i]);
			#endif
				*(tls+i) = res;
        }   	


		/* If this is an AVX instruction, set the high part with 0 */
		if(INSTR_CATEGORY == IFP_OP_AVX) {
			for(int i = nb_elem ; i < max_nb_elem ; i++) {
				*(tls+i) = 0;
			}
		}
		

    }
};

/*
 * \bried Identical to interflop_backend for three sources instructions 
 */
template <typename FTYPE, FTYPE (*Backend_function)(FTYPE, FTYPE, FTYPE), int INSTR_CATEGORY , int SIMD_TYPE = IFP_OP_SCALAR>
struct interflop_backend_fused {
        
        static void apply(FTYPE *vect_a,  FTYPE *vect_b, FTYPE *vect_c) {

       
        static const int vect_size = (SIMD_TYPE == IFP_OP_128) ? 16 : (SIMD_TYPE == IFP_OP_256) ? 32 : (SIMD_TYPE == IFP_OP_512) ? 64 : sizeof(FTYPE);
        static const int nb_elem = vect_size/sizeof(FTYPE);

		static const int max_operation_size = (INSTR_CATEGORY == IFP_OP_SSE) ? 16 : (INSTR_CATEGORY == IFP_OP_AVX) ? 32 : sizeof(FTYPE);
		static const int max_nb_elem = max_operation_size/sizeof(FTYPE);

        FTYPE res;
        FTYPE* tls = *(FTYPE**)(GET_TLS(dr_get_current_drcontext(), tls_result));

		for(int i = 0 ; i < nb_elem ; i++) {
			#if defined(X86)
					res = Backend_function(vect_a[i], vect_b[i], vect_c[i]);
				#elif defined(AARCH64)
					res = Backend_function(vect_b[i], vect_a[i], vect_c[i]);
				#endif
            		*(tls+i) = res;
        }   
		
		/* If this is an AVX instruction, set the high part with 0 */
		if(INSTR_CATEGORY == IFP_OP_AVX) {
			for(int i = nb_elem ; i < max_nb_elem ; i++) {
				*(tls+i) = 0;
			}
		}
    }   
};

void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr) {
	instr_set_translation(newinstr, instr_get_app_pc(instr));
	instr_set_app(newinstr);
	instrlist_preinsert(ilist, instr, newinstr);
}


void insert_save_floating_reg(void *drcontext, instrlist_t *bb, instr_t *instr, reg_id_t buffer_reg, reg_id_t scratch) {
#if defined(X86)
    if(AVX_512_SUPPORTED) {
        insert_push_pseudo_stack_list(drcontext, ZMM_REG, bb, instr, buffer_reg, scratch, NB_ZMM_REG);
    }
    else if(AVX_SUPPORTED) {
        insert_push_pseudo_stack_list(drcontext, YMM_REG, bb, instr, buffer_reg, scratch, NB_YMM_REG);
    }
    else { /* SSE only */
        insert_push_pseudo_stack_list(drcontext, XMM_REG, bb, instr, buffer_reg, scratch, NB_XMM_REG);
    }
#elif defined(AARCH64)
	insert_push_pseudo_stack_list(drcontext, Q_REG, bb, instr, buffer_reg, scratch, NB_Q_REG);
#endif
}

void insert_restore_floating_reg(void *drcontext, instrlist_t *bb, instr_t *instr, reg_id_t buffer_reg, reg_id_t scratch) {
#if defined(X86)
    if(AVX_512_SUPPORTED) {
        insert_pop_pseudo_stack_list(drcontext, ZMM_REG_REVERSE, bb, instr, buffer_reg, scratch, NB_ZMM_REG);
    }
    else if(AVX_SUPPORTED){
        insert_pop_pseudo_stack_list(drcontext, YMM_REG_REVERSE, bb, instr, buffer_reg, scratch, NB_YMM_REG); 
    }
    else { /* SSE only */
        insert_pop_pseudo_stack_list(drcontext, XMM_REG_REVERSE, bb, instr, buffer_reg, scratch, NB_XMM_REG); 
    }
#elif defined(AARCH64)
	insert_pop_pseudo_stack_list(drcontext, Q_REG_REVERSE, bb, instr, buffer_reg, scratch, NB_Q_REG); 
#endif
}

/*
 * \brief Subset of insert_call for two sources instructions
 */
template <typename FTYPE, FTYPE (*Backend_function)(FTYPE, FTYPE)>
void insert_corresponding_vect_call(void* drcontext, instrlist_t *bb, instr_t* instr, OPERATION_CATEGORY oc)
{
	switch(oc & IFP_SIMD_TYPE_MASK)
	{
		case IFP_OP_128:
			if(oc & IFP_OP_SSE)
				dr_insert_call(drcontext, bb, instr, (void*)interflop_backend<FTYPE, Backend_function, IFP_OP_SSE , IFP_OP_128>::apply, 0);
			else
				dr_insert_call(drcontext, bb, instr, (void*)interflop_backend<FTYPE, Backend_function, IFP_OP_AVX , IFP_OP_128>::apply, 0);
		break;
		case IFP_OP_256:
			dr_insert_call(drcontext, bb, instr, (void*)interflop_backend<FTYPE, Backend_function, IFP_AVX  ,IFP_OP_256>::apply, 0);
		break;
		case IFP_OP_512:
			dr_insert_call(drcontext, bb, instr, (void*)interflop_backend<FTYPE, Backend_function, IFP_AVX , IFP_OP_512>::apply, 0);
		break;
		default: /*SCALAR */
			if(oc & IFP_OP_SSE)
				dr_insert_call(drcontext, bb, instr, (void*)interflop_backend<FTYPE, Backend_function, IFP_OP_SSE>::apply, 0);
			else
				dr_insert_call(drcontext, bb, instr, (void*)interflop_backend<FTYPE, Backend_function, IFP_OP_AVX >::apply, 0);
	}
}

/*
 * \brief Subset of insert_call for three sources instructions
 */
template <typename FTYPE, FTYPE (*Backend_function)(FTYPE, FTYPE, FTYPE)>
void insert_corresponding_vect_call_fused(void* drcontext, instrlist_t *bb, instr_t* instr, OPERATION_CATEGORY oc)
{
	switch(oc & IFP_SIMD_TYPE_MASK)
	{
		case IFP_OP_128:
			if(oc & IFP_OP_SSE)
				dr_insert_call(drcontext, bb, instr, (void*)interflop_backend_fused<FTYPE, Backend_function, IFP_OP_SSE , IFP_OP_128>::apply, 0);
			else
				dr_insert_call(drcontext, bb, instr, (void*)interflop_backend_fused<FTYPE, Backend_function, IFP_OP_AVX , IFP_OP_128>::apply, 0);
		break;
		case IFP_OP_256:
			dr_insert_call(drcontext, bb, instr, (void*)interflop_backend_fused<FTYPE, Backend_function, IFP_OP_AVX , IFP_OP_256>::apply, 0);
		break;
		case IFP_OP_512:
			dr_insert_call(drcontext, bb, instr, (void*)interflop_backend_fused<FTYPE, Backend_function, IFP_OP_AVX ,  IFP_OP_512>::apply, 0);
		break;

		default: /*SCALAR */
			if(oc & IFP_OP_SSE)
				dr_insert_call(drcontext, bb, instr, (void*)interflop_backend_fused<FTYPE, Backend_function, IFP_OP_SSE>::apply, 0);
			else
				dr_insert_call(drcontext, bb, instr, (void*)interflop_backend_fused<FTYPE, Backend_function, IFP_OP_AVX >::apply, 0);
	}
}


/*
 * \brief Insert the corresponding call into the application depending on the properties of the instruction overloaded.  
 */
void insert_call(void *drcontext, instrlist_t *bb, instr_t *instr, OPERATION_CATEGORY oc, bool is_double) {
	if(oc & IFP_OP_FUSED) {
		if(oc & IFP_OP_FMA) {
			if(!(oc & IFP_OP_NEG)) {
				if(is_double)
					insert_corresponding_vect_call_fused<double, Interflop::Op<double>::fmadd>(drcontext, bb, instr, oc);
				else
					insert_corresponding_vect_call_fused<float, Interflop::Op<float>::fmadd>(drcontext, bb, instr, oc);
			}
			else {
				if(is_double)
					insert_corresponding_vect_call_fused<double, Interflop::Op<double>::nfmadd>(drcontext, bb, instr, oc);
				else
					insert_corresponding_vect_call_fused<float, Interflop::Op<float>::nfmadd>(drcontext, bb, instr, oc);
			}
		}
		else if(oc & IFP_OP_FMS) {
			if(!(oc & IFP_OP_NEG)) {
				if(is_double)
					insert_corresponding_vect_call_fused<double, Interflop::Op<double>::fmsub>(drcontext, bb, instr, oc);
				else
					insert_corresponding_vect_call_fused<float, Interflop::Op<float>::fmsub>(drcontext, bb, instr, oc);
			}
			else {
				if(is_double)
					insert_corresponding_vect_call_fused<double, Interflop::Op<double>::nfmsub>(drcontext, bb, instr, oc);
				else
					insert_corresponding_vect_call_fused<float, Interflop::Op<float>::nfmsub>(drcontext, bb, instr, oc);
			}
		}
	}
	else {
		switch (oc & IFP_OP_TYPE_MASK)
		{
			case IFP_OP_ADD:
				if(is_double)
					insert_corresponding_vect_call<double, Interflop::Op<double>::add>(drcontext, bb, instr, oc);
				else
					insert_corresponding_vect_call<float, Interflop::Op<float>::add>(drcontext, bb, instr, oc);
			break;
			case IFP_OP_SUB:
				if(is_double)
					insert_corresponding_vect_call<double, Interflop::Op<double>::sub>(drcontext, bb, instr, oc);
				else
					insert_corresponding_vect_call<float, Interflop::Op<float>::sub>(drcontext, bb, instr, oc);
			break;
			case IFP_OP_MUL:
				if(is_double)
					insert_corresponding_vect_call<double, Interflop::Op<double>::mul>(drcontext, bb, instr, oc);
				else
					insert_corresponding_vect_call<float, Interflop::Op<float>::mul>(drcontext, bb, instr, oc);
			break;
			case IFP_OP_DIV:
				if(is_double)
					insert_corresponding_vect_call<double, Interflop::Op<double>::div>(drcontext, bb, instr, oc);
				else
					insert_corresponding_vect_call<float, Interflop::Op<float>::div>(drcontext, bb, instr, oc);	
			break;
			default:
				PRINT_ERROR_MESSAGE("ERROR OPERATION NOT FOUND !");
		}
	}	
}


#define MINSERT(bb, where, instr) instrlist_meta_preinsert(bb, where, instr)
#define AINSERT(bb, where, instr) translate_insert(instr, bb, where)

/**
 * \brief Inserts prior to \p where meta-instructions to save the arithmetic flags and the gpr registers
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_save_gpr_and_flags(void *drcontext, instrlist_t *bb, instr_t *where)
{
#if defined(X86) && defined(X64)
    //save rcx to spill slot
    dr_save_reg(drcontext, bb, where, DR_REG_RCX, SPILL_SLOT_SCRATCH_REG); 
    //read tls
    INSERT_READ_TLS(drcontext, get_index_tls_gpr(), bb, where, DR_REG_RCX); 
    //store rax in second position
    MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_RCX, 8, OPSZ_8),OP_REG(DR_REG_XAX))); 
    //store arith flags to rax
    MINSERT(bb, where, INSTR_CREATE_lahf(drcontext)); 
    //store arith flags in first position
    MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_RCX, 0, OPSZ_8),OP_REG(DR_REG_XAX))); 
    //restore rcx into rax
    dr_restore_reg(drcontext, bb, where, DR_REG_XAX, SPILL_SLOT_SCRATCH_REG); 
    //store rcx in third position
    MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_RCX, 16, OPSZ_8),OP_REG(DR_REG_XAX))); 
    //save all the other GPR
    for(size_t i=3; i<NUM_GPR_SLOTS; i++)
    {
        MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_RCX, offset_of_gpr(GPR_ORDER[i]), OPSZ_8), OP_REG(GPR_ORDER[i])));
    }

#else //AArch64
DR_ASSERT_MSG(false, "insert_save_gpr_and_flags not implemented for this architecture");
#endif
}

void insert_restore_gpr_and_flags(void *drcontext, instrlist_t *bb, instr_t *where)
{
#if defined(X86) && defined(X64)
    //read tls into rcx
    INSERT_READ_TLS(drcontext, get_index_tls_gpr(), bb, where, DR_REG_RCX);
    //load saved arith flags to rax
    MINSERT(bb, where, XINST_CREATE_load(drcontext, OP_REG(DR_REG_RAX), OP_BASE_DISP(DR_REG_RCX, 0, OPSZ_8)));
	//load arith flags
	MINSERT(bb, where, INSTR_CREATE_sahf(drcontext));
	//load saved rax into rax
    MINSERT(bb, where, XINST_CREATE_load(drcontext, OP_REG(DR_REG_RAX), OP_BASE_DISP(DR_REG_RCX, 8, OPSZ_8)));
	//load back all GPR in reverse, overwrite RCX in the end
	for(size_t i=NUM_GPR_SLOTS-1; i>=2 /*RCX*/; --i)
	{
		MINSERT(bb, where, XINST_CREATE_load(drcontext, OP_REG(GPR_ORDER[i]), OP_BASE_DISP(DR_REG_RCX, offset_of_gpr(GPR_ORDER[i]), OPSZ_8)));
	}
    
#else //AArch64
DR_ASSERT_MSG(false, "insert_restore_gpr_and_flags not implemented for this architecture");
#endif
}

/**
 * \brief Prepares the address in the buffer of the tls register to point to the destination register in memory
 * Assumes the gpr have been saved beforehand !
 * \param drcontext 
 * \param bb 
 * \param where 
 * \param destination 
 */
void insert_set_destination_tls(void *drcontext, instrlist_t *bb, instr_t *where, reg_id_t destination)
{
#if defined(X86) && defined(X64)
	//Result tls adress in OP_A register
	INSERT_READ_TLS(drcontext, get_index_tls_result(), bb, where, DR_REG_OP_A_ADDR);
	//Floating registers tls adress in OP_B register
	INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, DR_REG_OP_B_ADDR);
	//Loads the adress of the destination register who is in the saved array, in OP_C register
	MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(DR_REG_OP_C_ADDR), OP_BASE_DISP(DR_REG_OP_B_ADDR, offset_of_simd(destination), OPSZ_lea)));
	//Stores the adress in the buffer of the result tls
	MINSERT(bb, where, XINST_CREATE_store(drcontext, OP_BASE_DISP(DR_REG_OP_A_ADDR,0, OPSZ_8), OP_REG(DR_REG_OP_C_ADDR)));
#else //AArch64
DR_ASSERT_MSG(false, "insert_set_destination_tls not implemented for this architecture");
#endif
}

/**
 * \brief Inserts prior to \p where meta-instructions to save the floating point registers (xmm-ymm-zmm)
 * Assumes the gpr have been saved beforehand !
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_save_simd_registers(void *drcontext, instrlist_t *bb, instr_t *where)
{
#if defined(X86) && defined(X64)
	//Loads the adress of the simd registers TLS
	INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, DR_SCRATCH_REG);
	//By default, SSE
	reg_id_t start = DR_REG_START_XMM, stop = DR_REG_XMM15;
	bool is_avx=false;
	opnd_size_t size = OPSZ_16;
	if(AVX_512_SUPPORTED)
	{
		is_avx = true;
		start = DR_REG_START_ZMM;
		stop = DR_REG_STOP_ZMM;
		size=OPSZ_64;
	}else if(AVX_SUPPORTED)
	{
		is_avx=true;
		start = DR_REG_START_YMM;
		stop = DR_REG_YMM15;
		size = OPSZ_32;
	}
	//Save all the SIMD registers
	for(reg_id_t i=start; i<=stop; i++)
	{
		MINSERT(bb, where, MOVE_FLOATING_PACKED(is_avx, drcontext, OP_BASE_DISP(DR_SCRATCH_REG, offset_of_simd(i),size), OP_REG(i)));
	}
#else //AArch64
DR_ASSERT_MSG(false, "insert_save_simd_registers not implemented for this architecture");
#endif
}

/**
 * \brief Inserts prior to \p where meta-instructions to restore the floating point registers (xmm-ymm-zmm)
 * Assumes the gpr have been saved beforehand !
 * \param drcontext DynamoRIO context
 * \param bb Current basic bloc
 * \param where instruction prior to whom we insert the meta-instructions 
 */
void insert_restore_simd_registers(void *drcontext, instrlist_t *bb, instr_t *where)
{
#if defined(X86) && defined(X64)
	//Loads the adress of the simd registers TLS
	INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, DR_SCRATCH_REG);
	//By default, SSE
	reg_id_t start = DR_REG_START_XMM, stop = DR_REG_XMM15;
	bool is_avx=false;
	opnd_size_t size = OPSZ_16;
	if(AVX_512_SUPPORTED)
	{
		is_avx = true;
		start = DR_REG_START_ZMM;
		stop = DR_REG_STOP_ZMM;
		size=OPSZ_64;
	}else if(AVX_SUPPORTED)
	{
		is_avx=true;
		start = DR_REG_START_YMM;
		stop = DR_REG_YMM15;
		size = OPSZ_32;
	}
	//Restore all the SIMD registers
	for(reg_id_t i=start; i<=stop; i++)
	{
		MINSERT(bb, where, MOVE_FLOATING_PACKED(is_avx, drcontext, OP_REG(i), OP_BASE_DISP(DR_SCRATCH_REG, offset_of_simd(i),size)));
	}
#else //AArch64
DR_ASSERT_MSG(false, "insert_save_simd_registers not implemented for this architecture");
#endif
}

/**
 * \brief 
 * 
 * \param drcontext 
 * \param bb 
 * \param where 
 * \param src0 
 * \param src1 
 * \param src2 
 */
static void insert_set_operands_all_registers(void* drcontext, instrlist_t* bb, instr_t *where, reg_id_t src0, reg_id_t src1, reg_id_t src2, reg_id_t out_reg[])
{
#if defined(X86) && defined(X64)
	if(src2 == DR_REG_NULL)
	{
		//2 registers
        INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, out_reg[1]);
		MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[0]), OP_BASE_DISP(out_reg[1], offset_of_simd(src0), OPSZ_lea)));
		MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[1]), OP_BASE_DISP(out_reg[1], offset_of_simd(src1), OPSZ_lea)));
	}else
	{
		//3 registers
        INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, out_reg[2]);
		MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[0]), OP_BASE_DISP(out_reg[2], offset_of_simd(src0), OPSZ_lea)));
		MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[1]), OP_BASE_DISP(out_reg[2], offset_of_simd(src1), OPSZ_lea)));
		MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(out_reg[2]), OP_BASE_DISP(out_reg[2], offset_of_simd(src2), OPSZ_lea)));
	}
#else //AArch64
DR_ASSERT_MSG(false, "insert_set_operands_all_registers not implemented for this architecture");
#endif
}

static void insert_set_operands_base_disp(void* drcontext, instrlist_t* bb, instr_t *where, opnd_t addr, reg_id_t destination, reg_id_t tempindex)
{
	INSERT_READ_TLS(drcontext, get_index_tls_gpr(), bb, where, destination);
	reg_id_t base = opnd_get_base(addr), index = opnd_get_index(addr);
    if(index != DR_REG_NULL)
	{
		MINSERT(bb, where, XINST_CREATE_load(drcontext, OP_REG(tempindex), OP_BASE_DISP(destination, offset_of_gpr(index), OPSZ_8)));
		index = tempindex;
	}
	if(base != DR_REG_NULL)
	{
		MINSERT(bb, where, XINST_CREATE_load(drcontext, OP_REG(destination), OP_BASE_DISP(destination, offset_of_gpr(base), OPSZ_8)));
		base = destination;
	}
	MINSERT(bb, where, INSTR_CREATE_lea(drcontext, OP_REG(destination), opnd_create_base_disp(base, index, opnd_get_scale(addr), opnd_get_disp(addr), OPSZ_lea)));
}

static void insert_set_operands_addr(void* drcontext, instrlist_t* bb, instr_t *where, void* addr, reg_id_t destination, reg_id_t scratch)
{
    MINSERT(bb, where, XINST_CREATE_load_int(drcontext, OP_REG(destination), OPND_CREATE_INTPTR(addr)));
    /*return;
    //We can't move an immediate of 64 bit directly (for some reason), so we need to use a trick
    ptr_int_t high = (ptr_uint_t)((uint64_t)addr >> 32);
    ptr_int_t low = (ptr_uint_t)((uint64_t)addr & 0xFFFFFFFF);
    //Move 32bits immediates to 2 registers
    reg_id_t scratch32 = reg_64_to_32(scratch);
    reg_id_t dest32 = reg_64_to_32(destination);
    AINSERT(bb, where, XINST_CREATE_load_int(drcontext, OP_REG(scratch32), OPND_CREATE_INT32(high)));
    AINSERT(bb, where, XINST_CREATE_load_int(drcontext, OP_REG(dest32), OPND_CREATE_INT32(low)));
    //We shift the low bits by 32 into the upper parts of the register
    AINSERT(bb, where, INSTR_CREATE_shl(drcontext, OP_REG(destination), opnd_create_immed_int(32, OPSZ_1)));
    //We shift the destination register back 32 bits to the right while taking the bits of the scratch register
    AINSERT(bb, where, INSTR_CREATE_shrd(drcontext, OP_REG(destination), OP_REG(scratch), opnd_create_immed_int(32, OPSZ_1)));*/
}

/**
 * \brief 
 * 
 * \param drcontext 
 * \param bb 
 * \param where 
 * \param instr 
 * \param fused 
 */
static void insert_set_operands_mem_reference(void* drcontext, instrlist_t *bb, instr_t *where, instr_t *instr, bool fused, int mem_src_idx, reg_id_t out_regs[])
{
    DR_ASSERT_MSG((mem_src_idx >= 0) && (instr_num_srcs(instr) > mem_src_idx), "MEMORY SOURCE INDEX IS INVALID");
    opnd_t mem_src = SRC(instr, mem_src_idx);
    if(OP_IS_BASE_DISP(mem_src))
    {
        insert_set_operands_base_disp(drcontext, bb, where, mem_src, out_regs[mem_src_idx], out_regs[mem_src_idx == 0 ? 1 : 0]);
    }else if(OP_IS_ADDR(mem_src))
    {
        void* addr = opnd_get_addr(mem_src);
        insert_set_operands_addr(drcontext, bb, where, addr, out_regs[mem_src_idx], out_regs[mem_src_idx == 0 ? 1 : 0]);
    }else
    {
        DR_ASSERT_MSG(false, "OPERAND IS NOT A MEMORY REFERENCE");
    }
    //Index in which we'll store the tls, we want it to be the last so that way we override it only at the end
    int last_reg_idx = fused ? 2 : 1;
    //We don't want to override the address we put before
    if(mem_src_idx == last_reg_idx) --last_reg_idx;
    INSERT_READ_TLS(drcontext, get_index_tls_float(), bb, where, out_regs[last_reg_idx]);

    for(int i=0; i< (fused ? 3:2); i++)
    {
        if(i != mem_src_idx)
        {
            reg_id_t reg = GET_REG(SRC(instr, i));
            MINSERT(bb, where, INSTR_CREATE_lea(drcontext, 
                                                OP_REG(out_regs[i]), 
                                                OP_BASE_DISP(out_regs[last_reg_idx], offset_of_simd(reg), OPSZ_lea)));
        }
    }

    
}

/**
 * \brief 
 * 
 * \param drcontext 
 * \param bb 
 * \param where 
 * \param instr 
 * \param oc 
 * \param fused 
 */
void insert_set_operands(void* drcontext, instrlist_t *bb, instr_t *where, instr_t *instr, OPERATION_CATEGORY oc)
{
    reg_id_t reg_op_addr[3];
    const bool fused = ifp_is_fused(oc);
    if(fused)
    {
        if(oc & IFP_OP_213) {
            reg_op_addr[0] = DR_REG_OP_B_ADDR;
            reg_op_addr[1] = DR_REG_OP_C_ADDR;
            reg_op_addr[2] = DR_REG_OP_A_ADDR;
        }
        else if(oc & IFP_OP_231) {
            reg_op_addr[0] = DR_REG_OP_B_ADDR;
            reg_op_addr[1] = DR_REG_OP_A_ADDR;
            reg_op_addr[2] = DR_REG_OP_C_ADDR;
        }
        else if(oc & IFP_OP_132) {
            reg_op_addr[0] = DR_REG_OP_C_ADDR;
            reg_op_addr[1] = DR_REG_OP_A_ADDR;
            reg_op_addr[2] = DR_REG_OP_B_ADDR;
        }else
        {
            DR_ASSERT_MSG(false, "ORDER OF FUSED UNKNOWN");
        }
    }else
    {
        if(get_need_sse_inverse() && INSTR_IS_ANY_SSE(instr))
        {
            reg_op_addr[0] = DR_REG_OP_B_ADDR;
            reg_op_addr[1] = DR_REG_OP_A_ADDR;
        }else
        {
            reg_op_addr[0] = DR_REG_OP_A_ADDR;
            reg_op_addr[1] = DR_REG_OP_B_ADDR;
        }
        reg_op_addr[2] = DR_REG_NULL;
    }
    
    
    
	int mem_src=-1;
	//Get the index of the memory operand, if there is one
	for (size_t i = 0; i < (fused ? 3 : 2); i++)
	{
		opnd_t src = SRC(instr, i);
		if(OP_IS_ADDR(src) || OP_IS_BASE_DISP(src))
		{
			mem_src=i;
            break;
		}
	}
	if(mem_src==-1)
	{
		//No memory references, only registers
		insert_set_operands_all_registers(drcontext, bb, where, GET_REG(SRC(instr, 0)), GET_REG(SRC(instr, 1)), fused ? GET_REG(SRC(instr, 2)) : DR_REG_NULL, reg_op_addr);
	}else
	{
		//Memory reference
        insert_set_operands_mem_reference(drcontext, bb, where, instr, fused, mem_src, reg_op_addr);
	}
}

void insert_restore_rsp(void *drcontext, instrlist_t *bb, instr_t *where)
{
    INSERT_READ_TLS(drcontext, get_index_tls_gpr(), bb, where, DR_REG_RSP);
    MINSERT(bb, where, XINST_CREATE_load(drcontext, OP_REG(DR_REG_RSP), OP_BASE_DISP(DR_REG_RSP, offset_of_gpr(DR_REG_RSP), OPSZ_8)));
}