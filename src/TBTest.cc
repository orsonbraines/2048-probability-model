#include <iostream>
#include <random>

#include "Tablebase.h"

std::mt19937 s_random;

int main() {
	GridState<2> state;
	InMemoryTablebase<2> tablebase(0.2f);
	tablebase.init(state);
	tablebase.dump(std::cout);
}
