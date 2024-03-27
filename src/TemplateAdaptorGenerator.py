import sys

compiled_sizes = {2, 3, 4, 5, 6, 7, 8}

with open("TemplateAdaptor.cc", "w") as f:
	f.write("#include <stdexcept>\n\n")
	f.write("#include \"TemplateAdaptor.h\"\n\n")
	# create game
	f.write("void* createGame(uint N, double fourChance) {\n")
	f.write("\tswitch(N) {\n")
	for i in compiled_sizes:
		f.write(f"\tcase {i}: return new Game<{i}>(fourChance);\n")
	f.write(f"\tdefault: throw std::invalid_argument(\"Size must be one of {compiled_sizes}\");\n")
	f.write("\t}\n")
	f.write("}\n\n")
	# delete game
	f.write("void deleteGame(uint N, void* game) {\n")
	f.write("\tswitch(N) {\n")
	for i in compiled_sizes:
		f.write(f"\tcase {i}: delete static_cast<Game<{i}>*>(game); return;\n")
	f.write(f"\tdefault: throw std::invalid_argument(\"Size must be one of {compiled_sizes}\");\n")
	f.write("\t}\n")
	f.write("}\n\n")
	# reset game
	f.write("void resetGame(uint N, void* game) {\n")
	f.write("\tswitch(N) {\n")
	for i in compiled_sizes:
		f.write(f"\tcase {i}: (static_cast<Game<{i}>*>(game))->reset(); return;\n")
	f.write(f"\tdefault: throw std::invalid_argument(\"Size must be one of {compiled_sizes}\");\n")
	f.write("\t}\n")
	f.write("}\n\n")
	# get score
	f.write("uint getScore(uint N, void* game) {\n")
	f.write("\tswitch(N) {\n")
	for i in compiled_sizes:
		f.write(f"\tcase {i}: return (static_cast<Game<{i}>*>(game))->getScore();\n")
	f.write(f"\tdefault: throw std::invalid_argument(\"Size must be one of {compiled_sizes}\");\n")
	f.write("\t}\n")
	f.write("}\n\n")
	# swipe
	f.write("void swipe(uint N, void* game, int dirRow, int dirCol) {\n")
	f.write("\tswitch(N) {\n")
	for i in compiled_sizes:
		f.write(f"\tcase {i}: (static_cast<Game<{i}>*>(game))->swipe(dirRow, dirCol); return;\n")
	f.write(f"\tdefault: throw std::invalid_argument(\"Size must be one of {compiled_sizes}\");\n")
	f.write("\t}\n")
	f.write("}\n\n")
	# print game 
	f.write("void printGame(std::ostream &o, uint N, const void* game) {\n")
	f.write("\tswitch(N) {\n")
	for i in compiled_sizes:
		f.write(f"\tcase {i}: o << *(static_cast<const Game<{i}>*>(game)); return;\n")
	f.write(f"\tdefault: throw std::invalid_argument(\"Size must be one of {compiled_sizes}\");\n")
	f.write("\t}\n")
	f.write("}\n\n")
