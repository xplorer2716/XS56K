# A tree of a few files and one test, used only by test_mutate.py (RQ-BLD-015): small enough to
# configure and build in a few seconds, so the mutation tool's own behaviour can be checked without
# paying the cost of the real juce/ project for every alteration. Never added to the main juce/
# CMakeLists.txt: it is its own standalone CMake project, configured and built by the tool under test,
# in a scratch copy, the same way the tool will later be pointed at juce/ itself.
cmake_minimum_required(VERSION 3.22)
project(mutate_fixture CXX)
enable_testing()

add_library(fixture_lib STATIC fixture.cpp)
target_include_directories(fixture_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(fixture_add test_add.cpp)
target_link_libraries(fixture_add PRIVATE fixture_lib)
add_test(NAME fixture_add COMMAND fixture_add)

add_executable(fixture_unused test_unused.cpp)
target_link_libraries(fixture_unused PRIVATE fixture_lib)
add_test(NAME fixture_unused COMMAND fixture_unused)

add_executable(fixture_hang test_hang.cpp)
target_link_libraries(fixture_hang PRIVATE fixture_lib)
add_test(NAME fixture_hang COMMAND fixture_hang)
