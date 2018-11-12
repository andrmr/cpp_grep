cmake_minimum_required(VERSION 2.8)

project(cppgrep)

set(sources
    ${CMAKE_CURRENT_LIST_DIR}/src/cppgrep.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/thread_pool.cpp)

set(headers
    ${CMAKE_CURRENT_LIST_DIR}/include/logger.h
    ${CMAKE_CURRENT_LIST_DIR}/include/thread_pool.h)

add_executable(${PROJECT_NAME} ${sources} ${headers})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

# C++ 17 std::filesystem support for Linux
if (UNIX)
target_link_libraries(${PROJECT_NAME} stdc++fs)
endif (UNIX)
