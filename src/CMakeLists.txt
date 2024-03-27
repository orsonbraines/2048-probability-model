cmake_minimum_required(VERSION 3.16)
project(2048-probability-model VERSION 0.0.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


message(DEBUG "source dir ${PROJECT_SOURCE_DIR}")
message(DEBUG "bin dir ${PROJECT_BINARY_DIR}")

find_package (Python3 REQUIRED COMPONENTS Interpreter)
# autogen TemplateAdaptor.cc
execute_process(COMMAND "${Python3_EXECUTABLE}" "TemplateAdaptorGenerator.py" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_executable(model_test ModelTest.cc TemplateAdaptor.cc)
target_include_directories(model_test PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_options(model_test PRIVATE "-g")
