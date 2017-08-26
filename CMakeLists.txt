cmake_minimum_required(VERSION 3.6)
project(ex1v2)
set(LDFLAGS "-pthread")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)
SET(GCC_COVERAGE_LINK_FLAGS "-lm -g -libverbs")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LDFLAGS} -g -lm -Wall -Wextra -o")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LDFLAGS} -g -lm -Wall -Wextra -o")

set(ex4_serverP1 RandServerP1.c pingpong.c pingpong.h)
add_executable(ex4_serverP1 ${ex4_serverP1})
target_link_libraries(ex4_serverP1 "${GCC_COVERAGE_LINK_FLAGS}")

set(ex4_clientP1 RandClientP1.c pingpong.c pingpong.h)
add_executable(ex4_clientP1 ${ex4_clientP1})
target_link_libraries(ex4_clientP1 "${GCC_COVERAGE_LINK_FLAGS}")