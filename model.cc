#include "model.h"

#include <iostream>

GridState<2> s_grid2;
GridState<3> s_grid3;
GridState<4> s_grid4;
GridState<5> s_grid5;
GridState<6> s_grid6;
GridState<7> s_grid7;
GridState<8> s_grid8;


int main(int argc, char** argv) {
	std::cout << "2x2: " << GridState<2>::BITS_PER_TILE << " bits-per-tile " << GridState<2>::GRID_BITS << " bits-per-grid" << std::endl;
	std::cout << "3x3: " << GridState<3>::BITS_PER_TILE << " bits-per-tile " << GridState<3>::GRID_BITS << " bits-per-grid" << std::endl;
	std::cout << "4x4: " << GridState<4>::BITS_PER_TILE << " bits-per-tile " << GridState<4>::GRID_BITS << " bits-per-grid" << std::endl;
	std::cout << "5x5: " << GridState<5>::BITS_PER_TILE << " bits-per-tile " << GridState<5>::GRID_BITS << " bits-per-grid" << std::endl;
	std::cout << "6x6: " << GridState<6>::BITS_PER_TILE << " bits-per-tile " << GridState<6>::GRID_BITS << " bits-per-grid" << std::endl;
	std::cout << "7x7: " << GridState<7>::BITS_PER_TILE << " bits-per-tile " << GridState<7>::GRID_BITS << " bits-per-grid" << std::endl;
	std::cout << "8x8: " << GridState<8>::BITS_PER_TILE << " bits-per-tile " << GridState<8>::GRID_BITS << " bits-per-grid" << std::endl;
}
