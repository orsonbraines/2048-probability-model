#pragma once

#include <cstdint>

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
