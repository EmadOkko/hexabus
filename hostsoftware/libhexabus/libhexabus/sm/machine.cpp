#include "sm/machine.hpp"

#include "../../../shared/hexabus_types.h"

#include <endian.h>
#include <string.h>

#include <iostream>

namespace hexabus {
namespace sm {

int Machine::sm_get_block(uint16_t at, uint8_t size, void* block)
{
	if (at + size > _program.size())
		return -1;

	memcpy(block, &_program[at], size);

	return size;
}

int Machine::sm_get_u8(uint16_t at, uint32_t* u)
{
	uint8_t buf;

	if (sm_get_block(at, 1, &buf) < 0)
		return -1;

	*u = buf;
	return 1;
}

int Machine::sm_get_u16(uint16_t at, uint32_t* u)
{
	uint16_t buf;

	if (sm_get_block(at, 2, &buf) < 0)
		return -1;

	*u = be16toh(buf);
	return 2;
}

int Machine::sm_get_u32(uint16_t at, uint32_t* u)
{
	uint32_t buf;

	if (sm_get_block(at, 4, &buf) < 0)
		return -1;

	*u = be32toh(buf);
	return 4;
}

int Machine::sm_get_float(uint16_t at, float* f)
{
	uint32_t u;

	if (sm_get_u32(at, &u) < 0)
		return -1;

	memcpy(f, &u, 4);
	return 4;
}

int Machine::sm_get_instruction(uint16_t at, struct hxb_sm_instruction* op)
{
	uint32_t offset = 0;

#define LOAD(type) ({ \
	uint32_t tmp; \
	int rc = sm_get_##type(at + offset, &tmp); \
	if (rc < 0) return -1; \
	offset += rc; \
	tmp; })

	op->opcode = (enum hxb_sm_opcode) LOAD(u8);

	switch (op->opcode) {
	case HSO_LD_SOURCE_IP:
	case HSO_LD_SOURCE_EID:
	case HSO_LD_SOURCE_VAL:
	case HSO_LD_CURSTATE:
	case HSO_LD_CURSTATETIME:
	case HSO_LD_SYSTIME:
	case HSO_OP_MUL:
	case HSO_OP_DIV:
	case HSO_OP_MOD:
	case HSO_OP_ADD:
	case HSO_OP_SUB:
	case HSO_OP_DT_DIFF:
	case HSO_OP_AND:
	case HSO_OP_OR:
	case HSO_OP_XOR:
	case HSO_OP_NOT:
	case HSO_OP_SHL:
	case HSO_OP_SHR:
	case HSO_OP_GETTYPE:
	case HSO_CMP_IP_UNDEF:
	case HSO_CMP_IP_LO:
	case HSO_CMP_LT:
	case HSO_CMP_LE:
	case HSO_CMP_GT:
	case HSO_CMP_GE:
	case HSO_CMP_EQ:
	case HSO_CMP_NEQ:
	case HSO_CONV_B:
	case HSO_CONV_U8:
	case HSO_CONV_U32:
	case HSO_CONV_F:
	case HSO_WRITE:
	case HSO_POP:
	case HSO_RET_CHANGE:
	case HSO_RET_STAY:
		break;

	case HSO_OP_DUP:
		op->immed.v_uint = 0;
		break;

	case HSO_OP_ROT:
		op->immed.v_uint = 1;
		break;

	case HSO_LD_FALSE:
		op->immed.type = HXB_DTYPE_BOOL;
		op->immed.v_uint = 0;
		break;

	case HSO_LD_TRUE:
		op->immed.type = HXB_DTYPE_BOOL;
		op->immed.v_uint = 1;
		break;

	case HSO_LD_REG:
	case HSO_ST_REG:
		op->immed.v_uint = LOAD(u8);
		break;

	case HSO_LD_U8:
		op->immed.type = HXB_DTYPE_UINT8;
		op->immed.v_uint = LOAD(u8);
		break;

	case HSO_LD_U16:
		op->immed.type = HXB_DTYPE_UINT32;
		op->immed.v_uint = LOAD(u16);
		break;

	case HSO_LD_U32:
		op->immed.type = HXB_DTYPE_UINT32;
		op->immed.v_uint = LOAD(u32);
		break;

	case HSO_LD_FLOAT: {
		uint32_t f = LOAD(u32);
		op->immed.type = HXB_DTYPE_FLOAT;
		memcpy(&op->immed.v_float, &f, sizeof(float));
		break;
	}

	case HSO_LD_DT: {
		uint32_t mask = LOAD(u8);
		uint32_t offset = 1;
		op->immed.type = HXB_DTYPE_DATETIME;
		uint8_t second = 0;
		uint8_t minute = 0;
		uint8_t hour = 0;
		uint8_t day = 0;
		uint8_t month = 0;
		uint16_t year = 0;
		uint8_t weekday = 0;

		if (mask & HSDM_SECOND) second = LOAD(u8);
		if (mask & HSDM_MINUTE) minute = LOAD(u8);
		if (mask & HSDM_HOUR) hour = LOAD(u8);
		if (mask & HSDM_DAY) day = LOAD(u8);
		if (mask & HSDM_MONTH) month = LOAD(u8);
		if (mask & HSDM_YEAR) year = LOAD(u16);
		if (mask & HSDM_WEEKDAY) weekday = LOAD(u8);
		hxbdt_set(&op->immed.v_datetime,
			second, minute, hour,
			day, month, year,
			weekday);
		break;
	}

	case HSO_OP_DUP_I:
	case HSO_OP_ROT_I:
	case HSO_OP_SWITCH_8:
	case HSO_OP_SWITCH_16:
	case HSO_OP_SWITCH_32:
		op->immed.v_uint = LOAD(u8);
		break;

	case HSO_OP_DT_DECOMPOSE:
	case HSO_CMP_DT_LT:
	case HSO_CMP_DT_GE:
		op->dt_mask = LOAD(u8);
		break;

	case HSO_CMP_BLOCK: {
		uint32_t tmp = LOAD(u8);
		op->block.first = tmp >> 4;
		op->block.last = tmp & 0xF;
		if (op->block.first > op->block.last)
			return -1;
		tmp = op->block.last - op->block.first + 1;
		if (sm_get_block(at, tmp, op->block.data) < 0)
			return -1;
		offset += tmp;
		break;
	}

	case HSO_JNZ:
	case HSO_JZ:
	case HSO_JUMP:
		op->jump_skip = LOAD(u16);
		break;

	case HSO_JNZ_S:
	case HSO_JZ_S:
	case HSO_JUMP_S:
		op->jump_skip = LOAD(u8);
		break;
	}

	return offset;
}

uint8_t Machine::sm_endpoint_write(uint32_t eid, const hxb_sm_value_t* val)
{
	boost::variant<bool, uint8_t, uint32_t, float> value;
	const char* type;

	switch (val->type) {
	case HXB_DTYPE_BOOL:
		value = (bool) val->v_uint;
		type = "bool";
		break;

	case HXB_DTYPE_UINT8:
		value = val->v_uint;
		type = "uint8";
		break;

	case HXB_DTYPE_UINT32:
		value = val->v_uint;
		type = "uint32";
		break;

	case HXB_DTYPE_FLOAT:
		value = val->v_float;
		type = "float";
		break;

	default:
		return 1;
	}

	std::cout << "WRITE\n"
		<< "	EP " << eid << "\n"
		<< "	Type " << type << "\n"
		<< "	Value " << value << std::endl;

	return 0;
}

uint32_t Machine::sm_get_timestamp()
{
	return (sm_get_systime() - _created_at).total_seconds();
}

hxb_datetime_t Machine::sm_get_systime()
{
	return boost::posix_time::second_clock::local_time();
}



static bool is_int(const hxb_sm_value_t* v)
{
	return v->type == HXB_DTYPE_BOOL ||
		v->type == HXB_DTYPE_UINT8 ||
		v->type == HXB_DTYPE_UINT32;
}

static bool is_arithmetic(const hxb_sm_value_t* v)
{
	return is_int(v) || v->type == HXB_DTYPE_FLOAT;
}

static void datetime_span(const hxb_datetime_t* dt, uint32_t* days, uint32_t* seconds)
{
	/* we want julian days, see https://en.wikipedia.org/wiki/Julian_day
	 * negative years obviously aren't interesting. */
	uint8_t a = hxbdt_month(dt) <= 2 ? 1 : 0;
	uint32_t y = hxbdt_year(dt) + 4800 - a;
	uint8_t m = hxbdt_month(dt) + 12 * a - 3;

	*days = hxbdt_day(dt) + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100
			+ y / 400 - 32045;

	*seconds = hxbdt_second(dt) + 60 * hxbdt_minute(dt)
		+ 3600 * hxbdt_hour(dt);
}

static bool sm_dt_op(hxb_sm_value_t* v1, const hxb_sm_value_t* v2, int op, uint8_t mask)
{
	if (v1->type != HXB_DTYPE_DATETIME || v2->type != HXB_DTYPE_DATETIME)
		return false;

	switch (op) {
	case HSO_OP_DT_DIFF: {
		uint32_t d1, s1, d2, s2;

		datetime_span(&v1->v_datetime, &d1, &s1);
		datetime_span(&v2->v_datetime, &d2, &s2);

		v1->type = HXB_DTYPE_UINT32;
		v1->v_uint = 86400 * (d1 - d2) + (s1 - s2);
		break;
	}

	case HSO_CMP_DT_GE:
	case HSO_CMP_DT_LT: {
		bool result = true;
		bool ge_inv = op != HSO_CMP_DT_GE;
		const hxb_datetime_t* d1 = &v1->v_datetime;
		const hxb_datetime_t* d2 = &v2->v_datetime;

		if (mask & HSDM_SECOND) result &= ge_inv ^ (hxbdt_second(d1) >= hxbdt_second(d2));
		if (mask & HSDM_MINUTE) result &= ge_inv ^ (hxbdt_minute(d1) >= hxbdt_minute(d2));
		if (mask & HSDM_HOUR) result &= ge_inv ^ (hxbdt_hour(d1) >= hxbdt_hour(d2));
		if (mask & HSDM_DAY) result &= ge_inv ^ (hxbdt_day(d1) >= hxbdt_day(d2));
		if (mask & HSDM_MONTH) result &= ge_inv ^ (hxbdt_month(d1) >= hxbdt_month(d2));
		if (mask & HSDM_YEAR) result &= ge_inv ^ (hxbdt_year(d1) >= hxbdt_year(d2));
		if (mask & HSDM_WEEKDAY) result &= ge_inv ^ (hxbdt_weekday(d1) >= hxbdt_weekday(d2));

		v1->type = HXB_DTYPE_BOOL;
		v1->v_uint = result;
		break;
	}

	default:
		return false;
	}

	return true;
}

static bool sm_arith_op2(hxb_sm_value_t* v1, const hxb_sm_value_t* v2, int op)
{
	if (!is_arithmetic(v1) || !is_arithmetic(v2))
		return false;

	/* widen v1 if v2 is of greater rank than v1, this covers integer
	 * conversion ranks and int<float for all ints */
	uint8_t common_type = v1->type;
	if (common_type < v2->type)
		common_type = v2->type;

	bool is_float = common_type == HXB_DTYPE_FLOAT;

	union {
		uint32_t u;
		float f;
	} val1, val2;

	if (v1->type == HXB_DTYPE_FLOAT)
		val1.f = v1->v_float;
	else if (common_type == HXB_DTYPE_FLOAT)
		val1.f = v1->v_uint;
	else
		val1.u = v1->v_uint;

	if (v2->type == HXB_DTYPE_FLOAT)
		val2.f = v2->v_float;
	else if (common_type == HXB_DTYPE_FLOAT)
		val2.f = v2->v_uint;
	else
		val2.u = v2->v_uint;

	v1->type = common_type;

	switch (op) {
#define ONE_OP(OP) \
		if (is_float) \
			v1->v_float = val1.f OP val2.f; \
		else \
			v1->v_uint = val1.u OP val2.u; \
		break;

	case HSO_OP_MUL: ONE_OP(*)
	case HSO_OP_DIV:
		if (!is_float && val2.u == 0)
			return false;
		ONE_OP(/)
	case HSO_OP_MOD:
		if (is_float)
			v1->v_float = 0.f;
		else if (v2->v_uint == 0)
			return false;
		else
			v1->v_uint = val1.u % val2.u;
		break;
	case HSO_OP_ADD: ONE_OP(+)
	case HSO_OP_SUB: ONE_OP(-)

#undef ONE_OP
#define ONE_OP(OP) \
		v1->v_uint = is_float \
			? val1.f OP val2.f \
			: val1.u OP val2.u; \
		v1->type = HXB_DTYPE_BOOL; \
		break;

	case HSO_CMP_LT: ONE_OP(<)
	case HSO_CMP_LE: ONE_OP(>=)
	case HSO_CMP_GT: ONE_OP(>)
	case HSO_CMP_GE: ONE_OP(>=)
	case HSO_CMP_EQ: ONE_OP(==)
	case HSO_CMP_NEQ: ONE_OP(!=)

#undef ONE_OP

	default: return false;
	}

	return true;
}

static bool sm_int_op2(hxb_sm_value_t* v1, const hxb_sm_value_t* v2, int op)
{
	if (!is_int(v1) || !is_int(v2))
		return false;

	/* widen v1 to if v2 is of greater rank than v1 */
	if (v1->type < v2->type) {
		v1->type = v2->type;
	}

	switch (op) {
	case HSO_OP_AND: v1->v_uint &=  v2->v_uint; break;
	case HSO_OP_OR:  v1->v_uint |=  v2->v_uint; break;
	case HSO_OP_XOR: v1->v_uint ^=  v2->v_uint; break;
	case HSO_OP_SHL: v1->v_uint <<= v2->v_uint; break;
	case HSO_OP_SHR: v1->v_uint >>= v2->v_uint; break;

	default: return false;
	}

	return true;
}

static bool sm_cmp_block(hxb_sm_value_t* val, int op,
	uint8_t first, uint8_t last, const char* cmp)
{
	if (val->type != HXB_DTYPE_16BYTES)
		return false;

	val->type = HXB_DTYPE_BOOL;

	if (op == HSO_CMP_BLOCK) {
		val->v_uint = memcmp(val->v_binary + first, cmp, last - first + 1) == 0;
		return true;
	}

	bool nonzero = false;
	for (uint8_t i = 0; i < 15; i++)
		nonzero |= val->v_binary[i] != 0;

	switch (op) {
	case HSO_CMP_IP_UNDEF: val->v_uint = !nonzero && val->v_binary[15] == 0; break;
	case HSO_CMP_IP_LO:    val->v_uint = !nonzero && val->v_binary[15] == 1; break;

	default: return false;
	}

	return true;
}

static bool sm_convert(hxb_sm_value_t* val, uint8_t to_type)
{
	if (to_type != HXB_DTYPE_BOOL &&
		to_type != HXB_DTYPE_UINT8 &&
		to_type != HXB_DTYPE_UINT32 &&
		to_type != HXB_DTYPE_FLOAT)
		return false;

	if (val->type == to_type)
		return true;

	switch (val->type) {
	case HXB_DTYPE_BOOL:
	case HXB_DTYPE_UINT8:
	case HXB_DTYPE_UINT32:
		switch (to_type) {
		case HXB_DTYPE_FLOAT:
			val->v_float = val->v_uint;
			break;

		case HXB_DTYPE_BOOL:
			val->v_uint = !!val->v_uint;
			break;

		case HXB_DTYPE_UINT8:
			val->v_uint &= 0xFF;
			break;

		default:
			break;
		}
		val->type = to_type;
		break;

	case HXB_DTYPE_FLOAT:
		switch (to_type) {
		case HXB_DTYPE_BOOL:
			val->v_uint = val->v_float != 0;
			break;

		case HXB_DTYPE_UINT8:
			val->v_uint = ((uint32_t) val->v_float) & 0xFF;
			break;

		case HXB_DTYPE_UINT32:
			val->v_uint = (uint32_t) val->v_float;
			break;

		default:
			break;
		}
		val->type = to_type;
		break;

	default: return false;
	}

	return true;
}

void Machine::run_sm(const char* src_ip, uint32_t eid, const hxb_sm_value_t* val)
{
	uint16_t addr = 0;
	int8_t top = -1;
	hxb_sm_value_t stack[SM_STACK_SIZE];
	struct hxb_sm_instruction insn;

#define CHECK_PUSH() do { if (top >= SM_STACK_SIZE) goto fail; } while (0)
#define CHECK_POP(n) do { if (top + 1 < n) goto fail; } while (0)
#define PUSH(dtype, member, value) \
	do { \
		CHECK_PUSH(); \
		top++; \
		TOP.type = dtype; \
		TOP.member = value; \
	} while (0)
#define PUSH_V(val) \
	do { \
		CHECK_PUSH(); \
		top++; \
		TOP = val; \
	} while (0)
#define TOP_N(n) (stack[top - n])
#define TOP TOP_N(0)

	{
		uint32_t val;
		int rc;

		if (sm_get_u8(0, &val) < 0 || val != 0)
			goto fail;

		if (src_ip)
			rc = sm_get_u16(1 + 0x0000, &val);
		else
			rc = sm_get_u16(1 + 0x0002, &val);

		if (rc < 0)
			goto fail;

		addr = val;
	}

	for (;;) {
		int insn_length = sm_get_instruction(addr, &insn);
		uint16_t jump_skip = 0;

		if (insn_length < 0)
			goto fail;

		switch (insn.opcode) {
		case HSO_LD_SOURCE_IP:
			if (!src_ip)
				goto fail;
			PUSH(HXB_DTYPE_16BYTES, v_binary, src_ip);
			break;

		case HSO_LD_SOURCE_EID:
			if (!src_ip)
				goto fail;
			PUSH(HXB_DTYPE_UINT32, v_uint, eid);
			break;

		case HSO_LD_SOURCE_VAL:
			if (!src_ip)
				goto fail;
			PUSH_V(*val);
			break;

		case HSO_LD_CURSTATE:
			PUSH(HXB_DTYPE_UINT32, v_uint, sm_curstate);
			break;

		case HSO_LD_CURSTATETIME:
			PUSH(HXB_DTYPE_UINT32, v_uint, sm_get_timestamp() - sm_in_state_since);
			break;

		case HSO_LD_FALSE:
		case HSO_LD_TRUE:
		case HSO_LD_U8:
		case HSO_LD_U16:
		case HSO_LD_U32:
		case HSO_LD_FLOAT:
		case HSO_LD_DT:
			PUSH_V(insn.immed);
			break;

		case HSO_LD_SYSTIME: {
			PUSH(HXB_DTYPE_DATETIME, v_datetime, sm_get_systime());
			break;
		}

		case HSO_LD_REG:
			if (insn.immed.v_uint >= SM_REG_COUNT)
				goto fail;
			PUSH_V(sm_registers[insn.immed.v_uint]);
			break;

		case HSO_ST_REG:
			if (insn.immed.v_uint >= SM_REG_COUNT)
				goto fail;
			CHECK_POP(1);
			sm_registers[insn.immed.v_uint] = TOP;
			top--;
			break;

		case HSO_OP_MUL:
		case HSO_OP_DIV:
		case HSO_OP_MOD:
		case HSO_OP_ADD:
		case HSO_OP_SUB:
		case HSO_CMP_LT:
		case HSO_CMP_LE:
		case HSO_CMP_GT:
		case HSO_CMP_GE:
		case HSO_CMP_EQ:
		case HSO_CMP_NEQ:
			CHECK_POP(2);
			if (!sm_arith_op2(&TOP_N(1), &TOP_N(0), insn.opcode))
				goto fail;
			top--;
			break;

		case HSO_OP_DT_DIFF:
		case HSO_CMP_DT_LT:
		case HSO_CMP_DT_GE:
			CHECK_POP(2);
			if (!sm_dt_op(&TOP_N(1), &TOP_N(0), insn.opcode, insn.dt_mask))
				goto fail;
			top--;
			break;

		case HSO_OP_AND:
		case HSO_OP_OR:
		case HSO_OP_XOR:
		case HSO_OP_SHL:
		case HSO_OP_SHR:
			CHECK_POP(2);
			if (!sm_int_op2(&TOP_N(1), &TOP_N(0), insn.opcode))
				goto fail;
			top--;
			break;

		case HSO_OP_NOT:
			CHECK_POP(1);
			switch (TOP.type) {
			case HXB_DTYPE_BOOL:
				TOP.v_uint = !TOP.v_uint;
				break;
			case HXB_DTYPE_UINT8:
				TOP.v_uint = ~TOP.v_uint & 0xFF;
				break;
			case HXB_DTYPE_UINT32:
				TOP.v_uint = ~TOP.v_uint;
				break;
			default:
				goto fail;
			}
			break;

		case HSO_OP_DUP:
		case HSO_OP_DUP_I: {
			CHECK_PUSH();

			uint8_t offset = insn.immed.v_uint;

			if (top < offset)
				goto fail;

			top++;
			TOP = TOP_N(offset + 1);
			break;
		}

		case HSO_OP_ROT:
		case HSO_OP_ROT_I: {
			uint8_t offset = insn.immed.v_uint;

			if (top < offset)
				goto fail;

			while (offset) {
				hxb_sm_value_t tmp = TOP_N(offset);
				TOP_N(offset) = TOP_N(offset - 1);
				TOP_N(offset - 1) = tmp;
				offset--;
			}
			break;
		}

		case HSO_OP_DT_DECOMPOSE: {
			CHECK_POP(1);

			if (TOP.type != HXB_DTYPE_DATETIME)
				goto fail;

			const hxb_datetime_t* dt = &TOP.v_datetime;

			top--;

			if (insn.dt_mask & HSDM_WEEKDAY)
				PUSH(HXB_DTYPE_UINT32, v_uint, hxbdt_weekday(dt));
			if (insn.dt_mask & HSDM_YEAR)
				PUSH(HXB_DTYPE_UINT32, v_uint, hxbdt_year(dt));
			if (insn.dt_mask & HSDM_MONTH)
				PUSH(HXB_DTYPE_UINT32, v_uint, hxbdt_month(dt));
			if (insn.dt_mask & HSDM_DAY)
				PUSH(HXB_DTYPE_UINT32, v_uint, hxbdt_day(dt));
			if (insn.dt_mask & HSDM_HOUR)
				PUSH(HXB_DTYPE_UINT32, v_uint, hxbdt_hour(dt));
			if (insn.dt_mask & HSDM_MINUTE)
				PUSH(HXB_DTYPE_UINT32, v_uint, hxbdt_minute(dt));
			if (insn.dt_mask & HSDM_SECOND)
				PUSH(HXB_DTYPE_UINT32, v_uint, hxbdt_second(dt));

			break;
		}

		case HSO_OP_GETTYPE:
			CHECK_POP(1);
			TOP.v_uint = TOP.type;
			TOP.type = HXB_DTYPE_UINT8;
			break;

		case HSO_OP_SWITCH_8:
		case HSO_OP_SWITCH_16:
		case HSO_OP_SWITCH_32: {
			CHECK_POP(1);

			if (!is_int(&TOP))
				goto fail;

			uint16_t offset = 1;

			if (insn.opcode == HSO_OP_SWITCH_8) {
				insn_length = insn.immed.v_uint * (1 + 2);
			} else if (insn.opcode == HSO_OP_SWITCH_16) {
				insn_length = insn.immed.v_uint * (2 + 2);
			} else {
				insn_length = insn.immed.v_uint * (4 + 2);
			}

			for (uint16_t i = 0; i < insn.immed.v_uint; i++) {
				uint32_t val, jump_val;

				int rc = -1;
				if (insn.opcode == HSO_OP_SWITCH_8) {
					rc = sm_get_u8(addr + offset, &val);
				} else if (insn.opcode == HSO_OP_SWITCH_16) {
					rc = sm_get_u16(addr + offset, &val);
				} else {
					rc = sm_get_u32(addr + offset, &val);
				}
				if (rc < 0)
					goto fail;
				offset += rc;

				rc = sm_get_u16(addr + offset, &jump_val);
				if (rc < 0)
					goto fail;
				offset += rc;

				if (TOP.v_uint == val) {
					jump_skip = jump_val;
					break;
				}
			}

			break;
		}

		case HSO_CMP_BLOCK:
		case HSO_CMP_IP_UNDEF:
		case HSO_CMP_IP_LO:
			CHECK_POP(1);
			if (!sm_cmp_block(&TOP, insn.opcode, insn.block.first,
					insn.block.last, insn.block.data))
				goto fail;
			break;

		case HSO_JNZ:
		case HSO_JZ:
		case HSO_JNZ_S:
		case HSO_JZ_S: {
			CHECK_POP(1);

			bool is_zero = false;

			switch (TOP.type) {
			case HXB_DTYPE_BOOL:
			case HXB_DTYPE_UINT8:
			case HXB_DTYPE_UINT32:
				is_zero = TOP.v_uint == 0;
				break;

			case HXB_DTYPE_FLOAT:
				is_zero = TOP.v_float == 0;
				break;

			default:
				goto fail;
			}

			uint16_t jz = insn.opcode == HSO_JZ || insn.opcode == HSO_JZ_S;

			if (is_zero == jz)
				jump_skip = insn.jump_skip;
			break;
		}

		case HSO_JUMP:
		case HSO_JUMP_S:
			jump_skip = insn.jump_skip;
			break;

		case HSO_WRITE: {
			CHECK_POP(2);

			if (!is_int(&TOP_N(1)))
				goto fail;

			hxb_sm_value_t write_val = TOP_N(0);
			uint32_t write_eid = TOP_N(1).v_uint;
			top--;

			TOP.v_uint = sm_endpoint_write(write_eid, &write_val);
			TOP.type = HXB_DTYPE_UINT8;
			break;
		}

		case HSO_CONV_B:
		case HSO_CONV_U8:
		case HSO_CONV_U32:
		case HSO_CONV_F: {
			CHECK_POP(1);

			uint8_t to = 0;
			switch (insn.opcode) {
			case HSO_CONV_B: to = HXB_DTYPE_BOOL; break;
			case HSO_CONV_U8: to = HXB_DTYPE_UINT8; break;
			case HSO_CONV_U32: to = HXB_DTYPE_UINT32; break;
			case HSO_CONV_F: to = HXB_DTYPE_FLOAT; break;
			default: goto fail;
			}
			if (!sm_convert(&TOP, to))
				goto fail;
			break;
		}

		case HSO_POP:
			CHECK_POP(1);
			top--;
			break;

		case HSO_RET_CHANGE:
			CHECK_POP(1);
			if (!is_int(&TOP))
				goto fail;
			sm_curstate = TOP.v_uint;
			sm_in_state_since = sm_get_timestamp();
			goto end_program;

		case HSO_RET_STAY:
			goto end_program;
		}

		if (addr + insn_length + jump_skip < addr)
			goto fail;
		addr += insn_length + jump_skip;
	}

end_program:
	std::cout << "sm_run done, in state " << sm_curstate << " since " << sm_in_state_since << std::endl;
	return;
fail:
	std::cerr << "invalid state machine program" << std::endl;
}

}
}
