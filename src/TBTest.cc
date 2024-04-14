#include <atomic>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <random>

#include "Tablebase.h"

std::mt19937 s_random((std::random_device())());

std::atomic<bool> s_interrupted = false;

extern "C" void interruptHandler(int sig) {
	if (sig == SIGINT) {
		s_interrupted = true;
	}
}

int main() {
	try {
		signal(SIGINT, interruptHandler);
		//InMemoryTablebase<2> tablebase(0.2f);
		//tablebase.init();
		//EmpiricalTablebase<2> eTablebase;	
		SqliteTablebase<3> sTablebase(0.2f, "testdb.sqlite");
		while(!s_interrupted && !sTablebase.partialInit(25000));
		//bool same = (tablebase == sTablebase);
		//std::cout << "same? " << same << std::endl;
		//assert(same);
		if (s_interrupted) std::cout << "ending to to SIGINT" << std::endl;

		//GridState<2> testInitState;
		//testInitState.writeTile(0, 0, 1);
		//auto p = sTablebase.bestMove(testInitState);
		//std::cout << p.first << p.second << std::endl;
		//QueryResultsType<2> queryResults;
		//sTablebase.recursiveQuery(testInitState, 0, 3, queryResults);
		//printQueryResults(std::cout, queryResults);
	
		//for (int i = 0; i < 1000000; ++i) {
		//	if (i % 1000000 == 0) {
		//		std::cout << i << " iterations complete." << std::endl;
		//	}
		//	std::vector<GridState<2>> states;
		//	Game<2> game(0.2f);
		//	states.push_back(game.getState());
		//	while (!game.isGameOver()) {
		//		auto [dirRow, dirCol] = tablebase.bestMove(game.getState());
		//		game.swipe(dirRow, dirCol);
		//		states.push_back(game.getState());
		//	}
		//	eTablebase.addResult(states, game.getState().hasTile(2 * 2 + 1));
		//}

		//eTablebase.compare(tablebase, std::cout, std::cerr);
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
}
