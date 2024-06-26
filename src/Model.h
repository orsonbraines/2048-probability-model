#pragma once

#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <string>

#include "ankerl/unordered_dense.h"
#include "Common.h"

extern std::mt19937 s_random;

// Returns number of bits necessary to represent integers from 0 up to and including x
// x = 0 is invalid input
constexpr uint tileLog2(uint x) {
#if _MSC_VER
	uint numBits = 0;
	while (x > 0) {
		++numBits;
		x >>= 1;
	}

	return numBits;
#else
	return 32u - __builtin_clz(x);
#endif
}

template<uint N>
class packed_bitset{
public:
	packed_bitset() { memset(m_data, 0, sizeof(m_data)); }
	bool test(uint n) const {
		assert(n < N);
		return (m_data[n >> 3] & (1u << (n & 0x7))) != 0;
	}
	void set(uint n, bool v = true) {
		assert(n < N);
		if (v) m_data[n >> 3] |= (1u << (n & 0x7));
		else m_data[n >> 3] &= ~(1u << (n & 0x7));
	}
	bool operator==(const packed_bitset<N> &that) const {
		return memcmp(this->m_data, that.m_data, (N+7)/8) == 0;
	}
	bool operator!=(const packed_bitset<N> &that) const {
		return !(*this == that);
	}
	uint64 hash() const {
		return ankerl::unordered_dense::ANKERL_UNORDERED_DENSE_NAMESPACE::detail::wyhash::hash(m_data, sizeof(m_data));
	}
	void* data() { return m_data; }
	const void* data()  const{ return m_data; }
private:
	uint8_t m_data[(N+7)/8];
};

template<uint N, class BITSET_T=packed_bitset<tileLog2(N*N + 1) * N * N>>
class GridState{
public:
	static constexpr uint BITS_PER_TILE = tileLog2(N*N + 1);
	static constexpr uint GRID_BITS = BITS_PER_TILE * N * N;
	static constexpr std::string tileToStr(uint tile) { return (tile == 0) ? "-" : std::to_string(1u << tile); }

	GridState() : m_grid() {}

	const BITSET_T& getGrid() const { return m_grid; }
	void* gridData() { return m_grid.data(); }
	const void* gridData() const { return m_grid.data(); }

	uint readTile(uint row, uint col) const;
	void writeTile(uint row, uint col, uint tile);
	bool isEmpty(uint row, uint col) const;
	uint swipe(int dirRow, int dirCol);
	void genRand(float fourRatio);
	bool hasMoves() const;
	bool hasTile(uint tile) const;
	uint numEmptyTiles() const;

	void printCompact(std::ostream &o) const;

	bool operator==(const GridState<N, BITSET_T> &that) const  {
		return this->m_grid == that.m_grid;
	}
	bool operator!=(const GridState<N, BITSET_T> &that) const  {
		return this->m_grid != that.m_grid;
	}
private:
	void swap(uint row1, uint col1, uint row2, uint col2);
	uint slideCol(uint col, bool dir);
	uint slideRow(uint row, bool dir);
	BITSET_T m_grid;
};

template<uint N, class BITSET_T>
void GridState<N, BITSET_T>::swap(uint row1, uint col1, uint row2, uint col2) {
	uint tile1 = readTile(row1, col1);
	uint tile2 = readTile(row2, col2);
	writeTile(row1, col1, tile2);
	writeTile(row2, col2, tile1);
}

template<uint N, class BITSET_T>
uint GridState<N, BITSET_T>::slideCol(uint col, bool dir) {
	assert(col < N);
	uint sumOfNewTiles = 0;
	bool lastSlideFused = false;
	if(dir) {
		for(int ri = N - 1; ri >= 0; --ri) {
			for(int rf = ri + 1; rf < (int)N; ++rf) {
				if(readTile(rf, col) == 0) {
					// move tile up to empty space
					swap(rf, col, rf - 1, col);
					if(rf == 0) {
						// set last slide fused to false if we are about to exit loop
						lastSlideFused = false;
					}
				}
				else if(readTile(rf, col) == readTile(rf - 1, col) && !lastSlideFused) {
					// fuse with identical tile
					lastSlideFused = true;
					uint fusedTile = readTile(rf, col) + 1;
					sumOfNewTiles += (1u << fusedTile);
					writeTile(rf, col, fusedTile);
					writeTile(rf - 1, col, 0);
					break;
				}
				else {
					// we collided with tile to which we cannot fuse
					lastSlideFused = false;
					break;
				}
			}
		}
	}
	else {
		for(int ri = 1; ri < (int)N; ++ri) {
			for(int rf = ri - 1; rf >= 0; --rf) {
				if(readTile(rf, col) == 0) {
					// move tile up to empty space
					swap(rf, col, rf + 1, col);
					if(rf == 0) {
						// set last slide fused to false if we are about to exit loop
						lastSlideFused = false;
					}
				}
				else if(readTile(rf, col) == readTile(rf + 1, col) && !lastSlideFused) {
					// fuse with identical tile
					lastSlideFused = true;
					uint fusedTile = readTile(rf, col) + 1;
					sumOfNewTiles += (1u << fusedTile);
					writeTile(rf, col, fusedTile);
					writeTile(rf + 1, col, 0);
					break;
				}
				else {
					// we collided with tile to which we cannot fuse
					lastSlideFused = false;
					break;
				}
			}
		}
	}
	return sumOfNewTiles;
}

template<uint N, class BITSET_T>
uint GridState<N, BITSET_T>::slideRow(uint row, bool dir) {
	assert(row < N);
	uint sumOfNewTiles = 0;
	bool lastSlideFused = false;
	if(dir) {
		for(int ci = N - 1; ci >= 0; --ci) {
			for(int cf = ci + 1; cf < (int)N; ++cf) {
				if(readTile(row, cf) == 0) {
					// move tile up to empty space
					swap(row, cf, row, cf - 1);
					if(cf == 0) {
						// set last slide fused to false if we are about to exit loop
						lastSlideFused = false;
					}
				}
				else if(readTile(row, cf) == readTile(row, cf - 1) && !lastSlideFused) {
					// fuse with identical tile
					lastSlideFused = true;
					uint fusedTile = readTile(row, cf) + 1;
					sumOfNewTiles += (1u << fusedTile);
					writeTile(row, cf, fusedTile);
					writeTile(row, cf - 1, 0);
					break;
				}
				else {
					// we collided with tile to which we cannot fuse
					lastSlideFused = false;
					break;
				}
			}
		}
	}
	else {
		for(int ci = 1; ci < (int)N; ++ci) {
			for(int cf = ci - 1; cf >= 0; --cf) {
				if(readTile(row, cf) == 0) {
					// move tile up to empty space
					swap(row, cf, row, cf + 1);
					if(cf == 0) {
						// set last slide fused to false if we are about to exit loop
						lastSlideFused = false;
					}
				}
				else if(readTile(row, cf) == readTile(row, cf + 1) && !lastSlideFused) {
					// fuse with identical tile
					lastSlideFused = true;
					uint fusedTile = readTile(row, cf) + 1;
					sumOfNewTiles += (1u << fusedTile);
					writeTile(row, cf, fusedTile);
					writeTile(row, cf+1, 0);
					break;
				}
				else {
					// we collided with tile to which we cannot fuse
					lastSlideFused = false;
					break;
				}
			}
		}
	}
	return sumOfNewTiles;
}

template<uint N, class BITSET_T>
uint GridState<N, BITSET_T>::readTile(uint row, uint col) const {
	assert(row < N && col < N);
	uint tile = 0;
	for(uint i = 0; i < BITS_PER_TILE; ++i) {
		tile |= (m_grid.test((row * N + col) * BITS_PER_TILE + i) << i);
	}
	return tile;
}

template<uint N, class BITSET_T>
void GridState<N, BITSET_T>::writeTile(uint row, uint col, uint tile) {
	assert(row < N && col < N);
	for(uint i = 0; i < BITS_PER_TILE; ++i) {
		m_grid.set((row * N + col) * BITS_PER_TILE + i, tile & 1);
		tile >>= 1;
	}
}

template<uint N, class BITSET_T>
bool GridState<N, BITSET_T>::isEmpty(uint row, uint col) const {
	return readTile(row, col) == 0;
}

template<uint N, class BITSET_T>
uint GridState<N, BITSET_T>::swipe(int dirRow, int dirCol) {
	assert(((dirRow == -1 || dirRow == 1) && dirCol == 0) || ((dirCol == -1 || dirCol == 1) && dirRow == 0));
	uint sumOfNewTiles = 0;
	if(dirCol == 0) {
		for(uint col = 0; col < N; ++col) {
			sumOfNewTiles += slideCol(col, dirRow == 1);
		}
	}
	else {
		for(uint row = 0; row < N; ++row) {
			sumOfNewTiles += slideRow(row, dirCol == 1);
		}
	}
	return sumOfNewTiles;
}

// Don't call if a full grid
template<uint N, class BITSET_T>
void GridState<N, BITSET_T>::genRand(float fourRatio) {
	std::uniform_real_distribution<float> d_dist;
	std::uniform_int_distribution<uint> i_dist(0, N-1);
	float roll = d_dist(s_random);
	for(uint i = 0; i < 1000*N*N; ++i) {
		uint row = i_dist(s_random);
		uint col = i_dist(s_random);
		if(readTile(row, col) == 0) {
			writeTile(row, col, roll < fourRatio ? 2 : 1);
			return;
		}
	}
	// If we haven't found an empty tile in 1000N**2 attempts, the grid is probably full.
	assert(0);
}

template<uint N, class BITSET_T>
bool GridState<N, BITSET_T>::hasMoves() const {
	GridState<N> clone1 = *this;
	clone1.swipe(1, 0);
	GridState<N> clone2 = *this;
	clone2.swipe(-1, 0);
	GridState<N> clone3 = *this;
	clone3.swipe(0, 1);
	GridState<N> clone4 = *this;
	clone4.swipe(0,-1);
	return !(clone1 == *this && clone2 == *this && clone3 == *this && clone4 == *this);
}

template<uint N, class BITSET_T>
bool GridState<N, BITSET_T>::hasTile(uint tile) const {
	for(uint r = 0; r < N; ++r) {
		for(uint c = 0; c < N; c++) {
			if(readTile(r, c) == tile) return true;
		}
	}
	return false;
}

template<uint N, class BITSET_T>
uint GridState<N, BITSET_T>::numEmptyTiles() const {
	uint emptyTiles = 0;
	for(uint r = 0; r < N; ++r) {
		for(uint c = 0; c < N; c++) {
			emptyTiles += isEmpty(r,c);
		}
	}
	return emptyTiles;
}

template<uint N, class BITSET_T>
void GridState<N, BITSET_T>::printCompact(std::ostream &o) const {
	for(uint r = 0; r < N; ++r) {
		for(uint c = 0; c < N; c++) {
			o << readTile(r,c);
			if(r != N - 1 || c != N - 1) o << ',';
		}
	}
}

template<uint N, class BITSET_T>
std::ostream& operator<<(std::ostream& o, const GridState<N, BITSET_T>& grid) {
	for(uint r = 0; r < N; ++r) {
		for(uint c = 0; c < N; c++) {
			o.setf(std::ios::right);
  			o.width(8);
			o << GridState<N, BITSET_T>::tileToStr(grid.readTile(r,c));
		}
		o << std::endl;
	}
	return o;
}

template<uint N>
class Game {
public:
	Game(float fourChance) : m_state(), m_score{0}, m_fourChance{fourChance}, m_gameOver{false} { 
		m_state.genRand(fourChance);
	}
	void reset() { 
		m_state = GridState<N>();
		m_state.genRand(m_fourChance);
		m_gameOver = false;
		m_score = 0;
	}
	void swipe(int dirRow, int dirCol) {
		assert(!m_gameOver);
		GridState<N> prevState = m_state;
		m_score += m_state.swipe(dirRow, dirCol);
		if(m_state != prevState) {
			m_state.genRand(m_fourChance);
			updateGameOver();
		}
	}

	uint getScore() const { return m_score; }
	bool isGameOver() const { return m_gameOver; }
	const GridState<N>& getState() const { return m_state; }
private:
	void updateGameOver();
	GridState<N> m_state;
	uint m_score;
	float m_fourChance;
	bool m_gameOver;
};

template<uint N>
void Game<N>::updateGameOver() {
	m_gameOver = !m_state.hasMoves();
}

template<uint N>
std::ostream& operator<<(std::ostream& o, const Game<N>& game) {
	o << "Score: " << game.getScore() << std::endl;
	o << game.getState();
	return o;
}

namespace std {
	template<uint N>
	struct hash<packed_bitset<N>> {
		std::size_t operator()(const packed_bitset<N>& n) {
			return n.hash();
		}
	};
	
	template<uint N>
	struct hash<GridState<N>> {
		std::size_t operator()(const GridState<N>& n) {
			return n.getGrid().hash();
		}
	};
}
