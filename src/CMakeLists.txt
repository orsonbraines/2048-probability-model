cmake_minimum_required(VERSION 3.16)
project(2048-probability-model VERSION 0.0.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(STATIC_LINK "Static link libraries")
if(${STATIC_LINK})
	set(BUILD_SHARED_LIBS OFF)
endif()

message(DEBUG "source dir ${PROJECT_SOURCE_DIR}")
message(DEBUG "bin dir ${PROJECT_BINARY_DIR}")

find_package (Python3 REQUIRED COMPONENTS Interpreter)
# autogen TemplateAdaptor.cc
execute_process(COMMAND ${Python3_EXECUTABLE} TemplateAdaptorGenerator.py WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(model_test ModelTest.cc TemplateAdaptor.cc)
target_include_directories(model_test PRIVATE ${PROJECT_SOURCE_DIR})
if(${MSVC})
	message(DEBUG "Currently not using any MSVC compile options")
else()
	target_compile_options(model_test PRIVATE -g)
endif()

find_package(Curses)
if(${CURSES_FOUND})
	message(DEBUG "include dirs: ${CURSES_INCLUDE_DIRS}")
	message(DEBUG "link libs: ${CURSES_LIBRARIES}")
	add_executable(tui TUI.cc TemplateAdaptor.cc)
	target_include_directories(tui PRIVATE ${PROJECT_SOURCE_DIR} ${CURSES_INCLUDE_DIRS})
	if(${MSVC})
		message(DEBUG "Currently not using any MSVC compile options")
	else()
		target_compile_options(model_test PRIVATE -g)
	endif()
	if(${STATIC_LINK})
		message(DEBUG "static linking curses")
		if(${MSVC})
			target_compile_options(tui PRIVATE /MT)
			target_link_libraries(tui PRIVATE ${CURSES_LIBRARIES})
		else()
			target_link_libraries(tui PRIVATE "/usr/lib/x86_64-linux-gnu/libcurses.a"
											  "/usr/lib/x86_64-linux-gnu/libform.a"
											  "/usr/lib/x86_64-linux-gnu/libmenu.a"
											  "/usr/lib/x86_64-linux-gnu/libpanel.a"
											  "/usr/lib/x86_64-linux-gnu/libtermcap.a"
											  "/usr/lib/x86_64-linux-gnu/libtic.a"
											  "/usr/lib/x86_64-linux-gnu/libtinfo.a")
			target_link_options(tui PRIVATE -static)
		endif()
	else()
		message(DEBUG "dynamic linking curses")
		target_link_libraries(tui PRIVATE ${CURSES_LIBRARIES})
	endif()
endif()
