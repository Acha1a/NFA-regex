cmake_minimum_required(VERSION 3.15)
project(nfa_regex CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(nfa STATIC nfa.cpp)
target_include_directories(nfa PUBLIC .)

add_executable(nfa_demo main.cpp)
target_link_libraries(nfa_demo PRIVATE nfa)

add_executable(nfa_tests test_nfa.cpp)
target_link_libraries(nfa_tests PRIVATE nfa)

enable_testing()
add_test(NAME nfa_tests COMMAND nfa_tests)