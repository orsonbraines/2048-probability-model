#include <iostream>
#include <random>

#include "Tablebase.h"

std::mt19937 s_random((std::random_device())());

int main() {
	InMemoryTablebase<2> tablebase(0.2f);
	EmpiricalTablebase<2> eTablebase;
	try {
		SqliteTablebase<2> sTablebase(0.2f);
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	tablebase.init();
	
	for (int i = 0; i < 10000000; ++i) {
		if (i % 1000000 == 0) {
			std::cout << i << " iterations complete." << std::endl;
		}
		std::vector<GridState<2>> states;
		Game<2> game(0.2f);
		states.push_back(game.getState());
		while (!game.isGameOver()) {
			auto [dirRow, dirCol] = tablebase.bestMove(game.getState());
			game.swipe(dirRow, dirCol);
			states.push_back(game.getState());
		}
		eTablebase.addResult(states, game.getState().hasTile(2 * 2 + 1));
	}

	eTablebase.compare(tablebase, std::cout, std::cerr);
}
