cmake_minimum_required(VERSION 3.8)

project(cppgrep)

include(${CMAKE_CURRENT_LIST_DIR}/util/CMakeLists.txt)

set(sources
    ${util_sources}
    ${CMAKE_CURRENT_LIST_DIR}/src/cppgrep.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp)

set(headers
    ${util_headers}
    ${CMAKE_CURRENT_LIST_DIR}/include/cppgrep.h)

add_executable(${PROJECT_NAME} ${sources} ${headers})
target_include_directories(${PROJECT_NAME}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/util/include>)

# compiler settings
if (CMAKE_COMPILER_IS_GNUCC)
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
    
    # gprof profiling
    # set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    
    # enabled warnings
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align \
    -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wsign-conversion -Wmisleading-indentation -Wduplicated-cond \
    -Wduplicated-branches -Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2")
    
    # disabled warnings
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-nonliteral -Wno-sign-conversion -Wno-sign-compare -Wno-ignored-qualifiers")
elseif (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /std:c++17")
endif()

# libraries
find_package(Threads)
set(libraries ${CMAKE_THREAD_LIBS_INIT})

# C++ 17 std::filesystem support for Linux
if (UNIX)
    set(libraries ${libraries} stdc++fs)
endif (UNIX)

target_link_libraries(${PROJECT_NAME} ${libraries})
