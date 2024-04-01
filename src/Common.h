#pragma once

#include <cstdint>
#include <sstream>
#include <stdexcept>

using uint32 = uint32_t;
using uint64 = uint64_t;
using uint = uint32_t;

#define NOP_MACRO(x) do {} while (0)
#if _DEBUG
#define DEBUG_LOG(x) do { std::cerr << x; } while (0)
#define DEBUG_EXEC(x) do { x; } while (0)
#define DEBUG_ASSERT(x) do { assert(x); } while (0)
#else
#define DEBUG_LOG(x) NOP_MACRO(x)
#define DEBUG_EXEC(x) NOP_MACRO(x)
#define DEBUG_ASSERT(x) NOP_MACRO(x)
#endif

#define CHECK_RETURN_CODE(stmt, expected) { \
	int code_ = stmt; \
	if (code_ != expected) { \
		std::ostringstream err_msg_; \
		err_msg_ << __FILE__ << ":" << __LINE__ << " failed assertion: expected: " << expected << " actual: " << code_; \
		throw std::runtime_error(err_msg_.str()); \
	} \
}
