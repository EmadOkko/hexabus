#ifndef SHARED_SM__TYPES_H_FBE69B5E7D88EBB1
#define SHARED_SM__TYPES_H_FBE69B5E7D88EBB1

enum hxb_sm_opcode {
	HSO_LD_SOURCE_IP,
	HSO_LD_SOURCE_EID,
	HSO_LD_SOURCE_VAL,
	HSO_LD_CURSTATE,
	HSO_LD_CURSTATETIME,
	HSO_LD_FALSE,
	HSO_LD_TRUE,
	HSO_LD_U8,
	HSO_LD_U16,
	HSO_LD_U32,
	HSO_LD_FLOAT,
	HSO_LD_DT,
	HSO_LD_SYSTIME,

	HSO_LD_REG,
	HSO_ST_REG,

	HSO_OP_MUL,
	HSO_OP_DIV,
	HSO_OP_MOD,
	HSO_OP_ADD,
	HSO_OP_SUB,
	HSO_OP_DT_DIFF,
	HSO_OP_AND,
	HSO_OP_OR,
	HSO_OP_XOR,
	HSO_OP_NOT,
	HSO_OP_SHL,
	HSO_OP_SHR,
	HSO_OP_DUP,
	HSO_OP_DUP_I,
	HSO_OP_ROT,
	HSO_OP_ROT_I,
	HSO_OP_DT_DECOMPOSE,
	HSO_OP_GETTYPE,
	HSO_OP_SWITCH_8,
	HSO_OP_SWITCH_16,
	HSO_OP_SWITCH_32,

	HSO_CMP_BLOCK,
	HSO_CMP_IP_LO,
	HSO_CMP_LT,
	HSO_CMP_LE,
	HSO_CMP_GT,
	HSO_CMP_GE,
	HSO_CMP_EQ,
	HSO_CMP_NEQ,
	HSO_CMP_DT_LT,
	HSO_CMP_DT_GE,

	HSO_CONV_B,
	HSO_CONV_U8,
	HSO_CONV_U32,
	HSO_CONV_F,

	HSO_JNZ,
	HSO_JZ,
	HSO_JUMP,

	HSO_WRITE,

	HSO_POP,

	HSO_RET_CHANGE,
	HSO_RET_STAY,
};

enum hxb_sm_dtmask {
	HSDM_SECOND  = 1,
	HSDM_MINUTE  = 2,
	HSDM_HOUR    = 4,
	HSDM_DAY     = 8,
	HSDM_MONTH   = 16,
	HSDM_YEAR    = 32,
	HSDM_WEEKDAY = 64,
};

struct hxb_sm_instruction {
	enum hxb_sm_opcode opcode;

	hxb_sm_value_t immed;
	uint8_t dt_mask;
	uint16_t jump_skip;
	struct {
		uint8_t first;
		uint8_t last;
		char data[16];
	} block;
};

enum hxb_sm_error {
	HSE_SUCCESS = 0,

	HSE_OOB_READ,
	HSE_INVALID_OPCODE,
	HSE_INVALID_TYPES,
	HSE_DIV_BY_ZERO,
	HSE_INVALID_HEADER,
	HSE_INVALID_OPERATION,
	HSE_STACK_ERROR,
};

enum hxb_sm_vector_offsets {
	HSVO_INIT     = 0x0000,
	HSVO_PACKET   = 0x0002,
	HSVO_PERIODIC = 0x0004,
};

#endif
