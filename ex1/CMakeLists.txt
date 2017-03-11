cmake_minimum_required(VERSION 3.5)
project(ex1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")

set(SOURCE_FILES TcpSpeedTracer.cpp Socket.cpp Socket.hpp)
add_executable(TcpSpeedTracer ${SOURCE_FILES})

set(SOURCE_FILES Socket.cpp Socket.hpp)
add_executable(Socket ${SOURCE_FILES})


set(SimpleServer TcpServer.cpp Socket.o Server.o)
add_executable(TcpServer ${SOURCE_FILES})