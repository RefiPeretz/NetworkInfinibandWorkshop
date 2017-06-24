cmake_minimum_required(VERSION 3.6)
project(ex1v2)
set(LDFLAGS "-pthread")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)
SET(GCC_COVERAGE_LINK_FLAGS "-lm -g -libverbs")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LDFLAGS} -g -lm -Wall -Wextra -o")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LDFLAGS} -g -lm -Wall -Wextra -o")

set(ex2_1 rc_pingpong.c pingpong.c pingpong.h stack.c stack.h MetricsIBV.c MetricsIBV.h)
add_executable(eager ${ex2_1})
target_link_libraries(eager "${GCC_COVERAGE_LINK_FLAGS}")

set(ex2_2 rdma_pp.c pingpong.c pingpong.h stack.c stack.h)
add_executable(rand ${ex2_2})
target_link_libraries(rand "${GCC_COVERAGE_LINK_FLAGS}")