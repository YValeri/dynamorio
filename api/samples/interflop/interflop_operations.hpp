#ifndef INTERFLOP_OPERATION_HEADER
#define INTERFLOP_OPERATION_HEADER

#include "dr_api.h"
#include "dr_ir_opcodes.h"

/*
 * The following is a list of flags caracterizing the operations we want to detect.
 * The flags allow us to detect the type of the instruction, the size, the operation, 
 * the order of operations if we detect a FMA/FMS, and the size of the SIMD instruction.
 * The other flags are masks to easily detect the operation, the type and the SIMD size.
*/

#define IFP_OP_OTHER		0		// 0b00000000000000000		  //Other operations, those that don't concern us
#define IFP_OP_DOUBLE		1		// 0b00000000000000001		  //Operation between doubles
#define IFP_OP_FLOAT		2		// 0b00000000000000010		  //Operation between floats
#define IFP_OP_PACKED		4		// 0b00000000000000100		  //Operation between vectors
#define IFP_OP_SCALAR		8		// 0b00000000000001000		  //Operation between scalars
#define IFP_OP_ADD			16		// 0b00000000000010000		  //Addition
#define IFP_OP_SUB			32		// 0b00000000000100000		  //Substraction
#define IFP_OP_MUL			64		// 0b00000000001000000		  //Multiplication
#define IFP_OP_DIV			128		// 0b00000000010000000		  //Division

#define IFP_OP_FMA			256		// 0b00000000100000000		  //Fused Multiply Add : a * b + c 
#define IFP_OP_FMS			512		// 0b00000001000000000		  //Fused Multiply Sub : a * b - c
#define IFP_OP_132			1024	// 0b00000010000000000		  //Order of FMA/FMS : a * c + b
#define IFP_OP_213			2048	// 0b00000100000000000		  //Order of FMA/FMS : b * a + c
#define IFP_OP_231			4096	// 0b00001000000000000		  //Order of FMA/FMS : b * c + a
#define IFP_OP_NEG			8192	// 0b00010000000000000		  //Negated FMA/FMS

#define IFP_OP_128			16384   // 0b00100000000000000		  //Operation between vectors of length 128
#define IFP_OP_256			32768   // 0b01000000000000000		  //Operation between vectors of length 256
#define IFP_OP_512			65536   // 0b10000000000000000		  //Operation between vectors of length 512

#define IFP_OP_MASK			131056  // 0b11111111111110000		  //Mask to get the Operation
#define IFP_OP_TYPE_MASK	16368   // 0b00011111111110000		  //Mask to get the type of the operation
#define IFP_SIMD_TYPE_MASK  114688  // 0b11100000000000000		  //Mask to get the type of SIMD
#define IFP_OP_FUSED		768		// 0b00000001100000000

// For operations that are unsupported for the moment
#define IFP_OP_UNSUPPORTED IFP_OP_OTHER

/*
 * This enum gives the length of operation we are modifying.
*/
enum SIMD_CATEGORY{
	IFP_SCALAR, 
	IFP_128, 
	IFP_256, 
	IFP_512
};

/*
 * This enum gathers all the possible operations we want to detect, as a combination of flags.
*/

enum OPERATION_CATEGORY{
	IFP_OTHER = IFP_OP_OTHER, 
	IFP_UNSUPPORTED = IFP_OP_UNSUPPORTED, 
	//Float
	IFP_ADDS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_ADD, 
	IFP_SUBS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_SUB, 
	IFP_MULS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_MUL, 
	IFP_DIVS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_DIV, 
	//Double 
	IFP_ADDD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_ADD, 
	IFP_SUBD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_SUB, 
	IFP_MULD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_MUL, 
	IFP_DIVD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_DIV, 
	//Packed float
	IFP_PADDS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_ADD, 
	IFP_PSUBS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_SUB, 
	IFP_PMULS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_MUL, 
	IFP_PDIVS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_DIV, 
	//Packed double
	IFP_PADDD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_ADD, 
	IFP_PSUBD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_SUB, 
	IFP_PMULD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_MUL, 
	IFP_PDIVD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_DIV, 
	
	//Packed float 128 bits
	IFP_PADDS_128 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_ADD | IFP_OP_128, 
	IFP_PSUBS_128 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_SUB | IFP_OP_128, 
	IFP_PMULS_128 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_MUL | IFP_OP_128, 
	IFP_PDIVS_128 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_DIV | IFP_OP_128, 
	
	//Packed double 128 bits
	IFP_PADDD_128 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_ADD | IFP_OP_128, 
	IFP_PSUBD_128 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_SUB | IFP_OP_128, 
	IFP_PMULD_128 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_MUL | IFP_OP_128, 
	IFP_PDIVD_128 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_DIV | IFP_OP_128, 
	//Packed float 256 bits
	IFP_PADDS_256 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_ADD | IFP_OP_256, 
	IFP_PSUBS_256 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_SUB | IFP_OP_256, 
	IFP_PMULS_256 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_MUL | IFP_OP_256, 
	IFP_PDIVS_256 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_DIV | IFP_OP_256, 
	//Packed double 256 bits
	IFP_PADDD_256 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_ADD | IFP_OP_256, 
	IFP_PSUBD_256 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_SUB | IFP_OP_256, 
	IFP_PMULD_256 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_MUL | IFP_OP_256, 
	IFP_PDIVD_256 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_DIV | IFP_OP_256, 
	//Packed float 512 bits
	IFP_PADDS_512 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_ADD | IFP_OP_512, 
	IFP_PSUBS_512 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_SUB | IFP_OP_512, 
	IFP_PMULS_512 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_MUL | IFP_OP_512, 
	IFP_PDIVS_512 = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_DIV | IFP_OP_512, 
	//Packed double 512 bits
	IFP_PADDD_512 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_ADD | IFP_OP_512, 
	IFP_PSUBD_512 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_SUB | IFP_OP_512, 
	IFP_PMULD_512 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_MUL | IFP_OP_512, 
	IFP_PDIVD_512 = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_DIV | IFP_OP_512, 



	//Scalar FMA and FMS Float
	IFP_A132SS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_FMA | IFP_OP_132, 
	IFP_A213SS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_FMA | IFP_OP_213, 
	IFP_A231SS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_FMA | IFP_OP_231, 
	IFP_S132SS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_FMS | IFP_OP_132, 
	IFP_S213SS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_FMS | IFP_OP_213, 
	IFP_S231SS = IFP_OP_FLOAT | IFP_OP_SCALAR | IFP_OP_FMS | IFP_OP_231, 

	//Scalar FMA and FMS Double
	IFP_A132SD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_FMA | IFP_OP_132, 
	IFP_A213SD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_FMA | IFP_OP_213, 
	IFP_A231SD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_FMA | IFP_OP_231, 
	IFP_S132SD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_FMS | IFP_OP_132, 
	IFP_S213SD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_FMS | IFP_OP_213, 
	IFP_S231SD = IFP_OP_DOUBLE | IFP_OP_SCALAR | IFP_OP_FMS | IFP_OP_231, 

	//Packed FMA and FMS Float
	IFP_A132PS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_FMA | IFP_OP_132, 
	IFP_A213PS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_FMA | IFP_OP_213, 
	IFP_A231PS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_FMA | IFP_OP_231, 
	IFP_S132PS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_FMS | IFP_OP_132, 
	IFP_S213PS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_FMS | IFP_OP_213, 
	IFP_S231PS = IFP_OP_FLOAT | IFP_OP_PACKED | IFP_OP_FMS | IFP_OP_231, 

	//Packed FMA and FMS Double
	IFP_A132PD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_FMA | IFP_OP_132, 
	IFP_A213PD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_FMA | IFP_OP_213, 
	IFP_A231PD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_FMA | IFP_OP_231, 
	IFP_S132PD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_FMS | IFP_OP_132, 
	IFP_S213PD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_FMS | IFP_OP_213, 
	IFP_S231PD = IFP_OP_DOUBLE | IFP_OP_PACKED | IFP_OP_FMS | IFP_OP_231, 

	//Negated Scalar FMA and FMS Float
	IFP_NA132SS = IFP_A132SS | IFP_OP_NEG, 
	IFP_NA213SS = IFP_A213SS | IFP_OP_NEG, 
	IFP_NA231SS = IFP_A231SS | IFP_OP_NEG, 
	IFP_NS132SS = IFP_S132SS | IFP_OP_NEG, 
	IFP_NS213SS = IFP_S213SS | IFP_OP_NEG, 
	IFP_NS231SS = IFP_S231SS | IFP_OP_NEG, 

	//Negated Scalar FMA and FMS Double
	IFP_NA132SD = IFP_A132SD | IFP_OP_NEG, 
	IFP_NA213SD = IFP_A213SD | IFP_OP_NEG, 
	IFP_NA231SD = IFP_A231SD | IFP_OP_NEG, 
	IFP_NS132SD = IFP_S132SD | IFP_OP_NEG, 
	IFP_NS213SD = IFP_S213SD | IFP_OP_NEG, 
	IFP_NS231SD = IFP_S231SD | IFP_OP_NEG, 

	//Negated Packed FMA and FMS Float
	IFP_NA132PS = IFP_A132PS | IFP_OP_NEG, 
	IFP_NA213PS = IFP_A213PS | IFP_OP_NEG, 
	IFP_NA231PS = IFP_A231PS | IFP_OP_NEG, 
	IFP_NS132PS = IFP_S132PS | IFP_OP_NEG, 
	IFP_NS213PS = IFP_S213PS | IFP_OP_NEG, 
	IFP_NS231PS = IFP_S231PS | IFP_OP_NEG, 

	//Negated Packed FMA and FMS Double
	IFP_NA132PD = IFP_A132PD | IFP_OP_NEG, 
	IFP_NA213PD = IFP_A213PD | IFP_OP_NEG, 
	IFP_NA231PD = IFP_A231PD | IFP_OP_NEG, 
	IFP_NS132PD = IFP_S132PD | IFP_OP_NEG, 
	IFP_NS213PD = IFP_S213PD | IFP_OP_NEG, 
	IFP_NS231PD = IFP_S231PD | IFP_OP_NEG
};

enum OPERATION_CATEGORY ifp_get_operation_category(instr_t* instr);

/*
 * The following functions detect, given an OPERATION_CATEGORY, if a certain flag is set.
*/
inline bool ifp_is_double(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_DOUBLE) != 0;
}
inline bool ifp_is_float(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_FLOAT) != 0;
}
inline bool ifp_is_packed(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_PACKED) != 0;
}
inline bool ifp_is_scalar(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_SCALAR) != 0;
}
inline bool ifp_is_add(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_ADD) != 0;
}
inline bool ifp_is_sub(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_SUB) != 0;
}
inline bool ifp_is_mul(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_MUL) != 0;
}
inline bool ifp_is_div(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_DIV) != 0;
}
inline bool ifp_is_fma(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_FMA) != 0;
}
inline bool ifp_is_fms(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_FMS) != 0;
}
inline bool ifp_is_fma132(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_132) != 0;
}
inline bool ifp_is_fma213(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_213) != 0;
}
inline bool ifp_is_fma231(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_231) != 0;
}
inline bool ifp_is_fms132(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_132) != 0;
}
inline bool ifp_is_fms213(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_213) != 0;
}
inline bool ifp_is_fms231(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_231) != 0;
}
inline bool ifp_is_negated(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_NEG) != 0;
}
inline bool ifp_is_128(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_128) != 0;
}
inline bool ifp_is_256(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_256) != 0;
}
inline bool ifp_is_512(enum OPERATION_CATEGORY oc)
{
	return (oc & IFP_OP_512) != 0;
}
inline bool ifp_is_fused(enum OPERATION_CATEGORY oc)
{
    return (oc & IFP_OP_FUSED) != 0;
}
inline bool ifp_is_instrumented(enum OPERATION_CATEGORY oc)
{
    return (oc != IFP_OP_OTHER && oc != IFP_OP_UNSUPPORTED);
}

#endif // INTERFLOP_OPERATION_HEADER
