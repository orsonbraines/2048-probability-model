#pragma once

#include <bitset>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>

using uint32 = uint32_t;
using uint = uint32_t;

extern std::mt19937 s_random;

// Returns number of bits necessary to represent integers from 0 up to and including x
// x = 0 is invalid input
constexpr uint tileLog2(uint x) {
	return 32u - __builtin_clz(x);
}

template<int N>
class GridState{
public:
	static constexpr uint BITS_PER_TILE = tileLog2(N*N + 1);
	static constexpr uint GRID_BITS = BITS_PER_TILE * N * N;
	static constexpr std::string tileToStr(uint tile) { return (tile == 0) ? "-" : std::to_string(1u << tile); }

	uint readTile(uint row, uint col) const;
	void writeTile(uint row, uint col, uint tile);
	GridState<N> swipe(int dirRow, int dirCol) const;
	GridState<N> genRand(double fourRatio) const;
private:
	void swap(uint row1, uint col1, uint row2, uint col2);
	uint slideCol(uint col, bool dir);
	uint slideRow(uint row, bool dir);
	std::bitset<GRID_BITS> m_grid;
};

template<int N>
void swap(uint row1, uint col1, uint row2, uint col2) {
	//TODO
}

template<int N>
uint GridState<N>::slideCol(uint col, bool dir) {
	assert(col >= 0 && col < N);
	uint sumOfNewTiles = 0;
	bool lastSlideFused = false;
	if(dir) { /*TODO*/ }
	else {
		for(uint ri = 1; ri < N; ++ri) {
			for(uint rf = ri - 1; rf >= 0; --rf) {
				// TODO
			}
		}
	}
	return sumOfNewTiles;
}

template<int N>
uint GridState<N>::slideRow(uint row, bool dir) {
	assert(row >= 0 && row < N);
	uint sumOfNewTiles = 0;
	// TODO
	return sumOfNewTiles;
}

template<int N>
uint GridState<N>::readTile(uint row, uint col) const {
	uint tile = 0;
	for(uint i = 0; i < GRID_BITS; ++i) {
		tile |= (m_grid[(row * N + col) * GRID_BITS + i] << i);
	}
	return tile;
}

template<int N>
void GridState<N>::writeTile(uint row, uint col, uint tile) {
	for(uint i = 0; i < GRID_BITS; ++i) {
		m_grid[(row * N + col) * GRID_BITS + i] = (tile & 1);
		tile >>= 1;
	}
}

template<int N>
GridState<N> GridState<N>::swipe(int dirRow, int dirCol) const {
	assert(((dirRow == -1 || dirRow == 1) && dirCol == 0) || ((dirCol == -1 || dirCol == 1) && dirCol == 0));
	GridState nextState = *this;
	return nextState;
}

template<int N>
GridState<N> GridState<N>::genRand(double fourRatio) const {
	GridState nextState = *this;
	return nextState;
}

template<int N>
std::ostream& operator<<(std::ostream& o, const GridState<N>& grid) {
	for(uint r = 0; r < N; ++r) {
		for(uint c = 0; c < N; c++) {
			o.setf(std::ios::right);
  			o.width(8);
			o << GridState<N>::tileToStr(grid.readTile(r,c));
		}
		o << std::endl;
	}
	return o;
}
