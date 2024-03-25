#pragma once

#include <bitset>
#include <cstdint>

using uint32 = uint32_t;
using uint = uint32_t;

// Returns number of bits necessary to represent integers from 0 up to and including x
// x = 0 is invalid input
constexpr uint log2(uint x) {
	return 32u - __builtin_clz(x);
}

template<int N>
class GridState{
public:
	static constexpr uint BITS_PER_TILE = log2(N*N + 1);
	static constexpr uint GRID_BITS = BITS_PER_TILE * N * N;
private:
	std::bitset<GRID_BITS> m_grid;
};
