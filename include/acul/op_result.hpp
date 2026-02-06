#pragma once
#include "scalars.hpp"

#define ACUL_OP_UNKNOWN          0xFFFF
#define ACUL_OP_DOMAIN           0x30A3
#define ACUL_OP_SUCCESS          0
#define ACUL_OP_ERROR_GENERIC    1
#define ACUL_OP_INVALID_SIZE     2
#define ACUL_OP_MAP_ERROR        3
#define ACUL_OP_COMPRESS_ERROR   4
#define ACUL_OP_DECOMPRESS_ERROR 5
#define ACUL_OP_NULLPTR          6
#define ACUL_OP_OUT_OF_BOUNDS    7
#define ACUL_OP_READ_ERROR       8
#define ACUL_OP_WRITE_ERROR      9
#define ACUL_OP_DELETE_ERROR     10
#define ACUL_OP_SEEK_ERROR       11
#define ACUL_OP_CHECKSUM_ERROR   12

// Codes specific
#define ACUL_OP_CODE_SIZE_ZERO    1
#define ACUL_OP_CODE_SIZE_UNKNOWN 2
#define ACUL_OP_CODE_SIZE_ERROR   3
#define ACUL_OP_CODE_SKIPPED      4

#define ACUL_TRY(expr)                \
    {                                 \
        auto _r = (expr);             \
        if (!_r.success()) return _r; \
    }

namespace acul
{
    struct op_result
    {
        u16 state;
        u16 domain_id;
        u32 code = 0;

        bool success() const { return state == ACUL_OP_SUCCESS; }

        operator u64() const { return ((u64)state << 48) | ((u64)domain_id << 32) | code; }

        static op_result from_u64(u64 result) { return {(u16)(result >> 48), (u16)(result >> 32), (u32)result}; }
    };

    op_result inline make_op_error(u16 state, u32 code = 0) { return {state, ACUL_OP_DOMAIN, code}; }
    op_result inline make_op_success() { return {ACUL_OP_SUCCESS}; }
} // namespace acul