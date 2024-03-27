#include "Model.h"
#include "TemplateAdaptor.h"

#include <iostream>

GridState<2> s_grid2;
GridState<3> s_grid3;
GridState<4> s_grid4;
GridState<5> s_grid5;
GridState<6> s_grid6;
GridState<7> s_grid7;
GridState<8> s_grid8;

std::mt19937 s_random((std::random_device())());


int main() {
	std::cout << "2x2: " << GridState<2>::BITS_PER_TILE << " bits-per-tile " << GridState<2>::GRID_BITS << " bits-per-grid " << sizeof(GridState<2>) << " bytes in memory." << std::endl;
	std::cout << "3x3: " << GridState<3>::BITS_PER_TILE << " bits-per-tile " << GridState<3>::GRID_BITS << " bits-per-grid " << sizeof(GridState<3>) << " bytes in memory." << std::endl;
	std::cout << "4x4: " << GridState<4>::BITS_PER_TILE << " bits-per-tile " << GridState<4>::GRID_BITS << " bits-per-grid " << sizeof(GridState<4>) << " bytes in memory." << std::endl;
	std::cout << "5x5: " << GridState<5>::BITS_PER_TILE << " bits-per-tile " << GridState<5>::GRID_BITS << " bits-per-grid " << sizeof(GridState<5>) << " bytes in memory." << std::endl;
	std::cout << "6x6: " << GridState<6>::BITS_PER_TILE << " bits-per-tile " << GridState<6>::GRID_BITS << " bits-per-grid " << sizeof(GridState<6>) << " bytes in memory." << std::endl;
	std::cout << "7x7: " << GridState<7>::BITS_PER_TILE << " bits-per-tile " << GridState<7>::GRID_BITS << " bits-per-grid " << sizeof(GridState<7>) << " bytes in memory." << std::endl;
	std::cout << "8x8: " << GridState<8>::BITS_PER_TILE << " bits-per-tile " << GridState<8>::GRID_BITS << " bits-per-grid " << sizeof(GridState<8>) << " bytes in memory." << std::endl;
	
	double fourChance = 0.2;
	void* game = createGame(3, fourChance);
	printGame(std::cout, 3, game);
	// [right, up, left, down]*5
	for(int i = 0; i < 5; ++i) {
		std::cout << "[RIGHT]" << std::endl;
		swipe(3, game, 0, 1);
		printGame(std::cout, 3, game);
		std::cout << "[UP]" << std::endl;
		swipe(3, game, -1, 0);
		printGame(std::cout, 3, game);
		std::cout << "[LEFT]" << std::endl;
		swipe(3, game, 0, -1);
		printGame(std::cout, 3, game);
		std::cout << "[DOWN]" << std::endl;
		swipe(3, game, 1, 0);
		printGame(std::cout, 3, game);
	}
	deleteGame(3, game);
}
