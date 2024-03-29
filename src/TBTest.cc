#include <iostream>
#include <random>

#include "Tablebase.h"

std::mt19937 s_random;

int main() {
	GridState<2> state;
	InMemoryTablebase<2> tablebase(0.2f);
	tablebase.init(state);
	//tablebase.dump(std::cout);
	QueryResultsType<2> res;
	//state.genRand(0.2f);
	state.writeTile(1,1,2);
	tablebase.recursiveQuery(state, 0, 2, res);
	printQueryResults(std::cout, res);
}
