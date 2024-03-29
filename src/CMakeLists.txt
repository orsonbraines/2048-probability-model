cmake_minimum_required(VERSION 3.16)
project(2048-probability-model VERSION 0.0.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
	message(DEBUG "Currently not using any MSVC compile options by default")
else()
	add_compile_options(-g -Wall -Wextra)
endif()

message(DEBUG "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_definitions(_DEBUG=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	if(MSVC)
		add_compile_options(/O2)
	else()
		add_compile_options(-O3)
	endif()
else()
	message(WARNING "Unsupported build type '${CMAKE_BUILD_TYPE}', defaulting to Release")
endif()

include_directories(${PROJECT_SOURCE_DIR} "external/unordered_dense/include" "external/rapidjson/include")

option(STATIC_LINK "Static link libraries")
if(STATIC_LINK)
	set(BUILD_SHARED_LIBS OFF)
endif()

message(DEBUG "source dir ${PROJECT_SOURCE_DIR}")
message(DEBUG "bin dir ${PROJECT_BINARY_DIR}")

find_package (Python3 REQUIRED COMPONENTS Interpreter)
# autogen TemplateAdaptor.cc
execute_process(COMMAND ${Python3_EXECUTABLE} TemplateAdaptorGenerator.py WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} RESULT_VARIABLE CODEGEN_STATUS)
if(CODEGEN_STATUS AND NOT CODEGEN_STATUS EQUAL 0)
	message(FATAL_ERROR "TemplateAdaptorGenerator.py FAILED: ${CODEGEN_STATUS}")
endif()

add_executable(model_test
	Common.h
	Model.h
	TemplateAdaptor.h
	ModelTest.cc
	TemplateAdaptor.cc
)

add_executable(tablebase_test
	Common.h
	Model.h
	Tablebase.h
	TBTest.cc
)

find_package(Curses)
if(CURSES_FOUND)
	message(DEBUG "include dirs: ${CURSES_INCLUDE_DIRS}")
	message(DEBUG "link libs: ${CURSES_LIBRARIES}")
	add_executable(tui
		Common.h
		Model.h
		Tablebase.h
		TemplateAdaptor.h
		TUI.cc
		TemplateAdaptor.cc
	)
	target_include_directories(tui PRIVATE ${PROJECT_SOURCE_DIR} ${CURSES_INCLUDE_DIRS})
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
