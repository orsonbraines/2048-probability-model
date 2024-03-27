#pragma once

#include <algorithm>
#include <vector>

#include "Common.h"
#include "Model.h"

std::vector<uint> getValidGameSizes();
inline bool isValidGameSize(uint N) {
	std::vector<uint> validSizes = getValidGameSizes();
	return std::find(validSizes.begin(), validSizes.end(), N) != validSizes.end();
}

void* createGame(uint N, double fourChance);
void deleteGame(uint N, void* game);

void resetGame(uint N, void* game);
uint getScore(uint N, void* game);
void swipe(uint N, void* game, int dirRow, int dirCol);
bool isGameOver(uint N, void* game);

void printGame(std::ostream &o, uint N, const void* game);
