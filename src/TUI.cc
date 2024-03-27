#include <curses.h>
#include <fstream>
#include <locale.h>
#include <sstream>
#include "Model.h"
#include "TemplateAdaptor.h"

std::mt19937 s_random((std::random_device())());

enum class UIState {
	GAME_ON,
	GAME_OVER,
	RESETING
};

void writeGame(WINDOW* w, void *game, uint gameSize, uint tileRows, uint tileCols) {
	std::ostringstream str;
	printGame(str, gameSize, game);
	waddstr(w, str.str().c_str());
}

int main() {
	// initialization
	setlocale(LC_ALL, "");
	WINDOW* stdscr = initscr();
	cbreak(); 
	noecho();
	intrflush(stdscr, true);
	keypad(stdscr, true);
	UIState uiState = UIState::GAME_ON;

	std::string resetBuffer = "";
	uint gameSize = 4;
	void* game = createGame(gameSize, 0.2);

	std::ofstream logFile("tui.log");

	for(;;) {
		clear();
		move(0,0);
		if(uiState == UIState::GAME_ON) 
			addstr("Welcome to Orson's 2048. Use WASD or arrow keys to swipe, n to reset, and backspace to close.");
		else if(uiState == UIState::GAME_OVER) 
			addstr("GAME OVER! Use n to reset, and backspace to close.");
		else if(uiState == UIState::RESETING)
			addstr(("Size (2,3,*4,5,6,7,8): " + resetBuffer).c_str());
		if(uiState != UIState::RESETING) {
			addch('\n');
			writeGame(stdscr, game, gameSize, 1, 8);
		}
		refresh();
		int key = getch();
		if(key == KEY_BACKSPACE) {
			break;
		}
		else if(key == 'n') {
			uiState = UIState::RESETING;
		}
		else if(uiState == UIState::RESETING && key >= '0' && key <= '9') {
			resetBuffer.push_back(key);
		}
		else if(uiState == UIState::RESETING && key == '\n') {
			if(resetBuffer.empty()) {
				resetGame(gameSize, game);
				uiState = UIState::GAME_ON;
			}
			else {
				uint N = std::stoi(resetBuffer);
				if(isValidGameSize(N)) {
					deleteGame(gameSize, game);
					gameSize = N;
					game = createGame(gameSize, 0.2);
					logFile << "Creating Game " << gameSize << " " << 0.2 << std::endl;
					uiState = UIState::GAME_ON;
				}
				else {
					logFile << "Bad size, clearing buffer." << std::endl;
				}
				resetBuffer.clear();
			}
		}
		else if(uiState == UIState::GAME_ON) {
			switch (key) {
			case 'a':
			case KEY_LEFT:
				swipe(gameSize, game, 0, -1);
				break;
			case 'd':
			case KEY_RIGHT:
				swipe(gameSize, game, 0, 1);
				break;
			case 'w':
			case KEY_UP:
				swipe(gameSize, game, -1, 0);
				break;
			case 's':
			case KEY_DOWN:
				swipe(gameSize, game, 1, 0);
				break;
			}
			if(isGameOver(gameSize, game)) {
				uiState = UIState::GAME_OVER;
			}
		}
		else {
			logFile << "Unrecognized key in state " << uint(uiState) << ": " << key << std::endl;
		}
	}

	deleteGame(gameSize, game);
	return endwin();
}
