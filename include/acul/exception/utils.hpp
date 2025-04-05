#pragma once

#include "../fwd/sstream.hpp"
#include "../vector.hpp"
#include "exception.hpp"


namespace acul
{
    APPLIB_API void write_frame_registers(acul::stringstream &stream, const CONTEXT &context);
    APPLIB_API void write_exception_info(EXCEPTION_RECORD record, acul::stringstream &stream);
    APPLIB_API void write_stack_trace(acul::stringstream &stream, const except_info &except_info);
    APPLIB_API bool create_mini_dump(HANDLE hProcess, HANDLE hThread, EXCEPTION_RECORD &exceptionRecord,
                                     CONTEXT &context, acul::vector<char> &buffer);
} // namespace acul