#pragma once

#include "Common.h"
#include "Model.h"

void* createGame(uint N, double fourChance);
void deleteGame(uint N, void* game);

void resetGame(uint N, void* game);
uint getScore(uint N, void* game);
void swipe(uint N, void* game, int dirRow, int dirCol);

void printGame(std::ostream &o, uint N, const void* game);
