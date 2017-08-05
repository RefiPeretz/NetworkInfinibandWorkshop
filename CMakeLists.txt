cmake_minimum_required(VERSION 3.6)
project(ex1v2)
set(LDFLAGS "-pthread")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)
SET(GCC_COVERAGE_LINK_FLAGS "-lm -g -libverbs")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LDFLAGS} -g -lm -Wall -Wextra -o")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LDFLAGS} -g -lm -Wall -Wextra -o")

set(ex3_1_server eagerServer.c pingpong.c pingpong.h)
add_executable(ex3_1_server ${ex3_1_server})
target_link_libraries(ex3_1_server "${GCC_COVERAGE_LINK_FLAGS}")

set(ex3_1_client eagerClient.c pingpong.c pingpong.h)
add_executable(ex3_1_client ${ex3_1_client})
target_link_libraries(ex3_1_client "${GCC_COVERAGE_LINK_FLAGS}")

set(ex3_1_serverP3 eagerServerP3.c pingpong.c pingpong.h)
add_executable(ex3_1_serverP3 ${ex3_1_serverP3})
target_link_libraries(ex3_1_serverP3 "${GCC_COVERAGE_LINK_FLAGS}")

set(ex3_clientP3 eagerClientP3.c pingpong.c pingpong.h)
add_executable(ex3_clientP3 ${ex3_clientP3})
target_link_libraries(ex3_clientP3 "${GCC_COVERAGE_LINK_FLAGS}")