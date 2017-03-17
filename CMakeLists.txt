cmake_minimum_required(VERSION 3.6)
project(ex1v2)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp Stream.cpp Stream.hpp Connector.cpp Connector.hpp
    Acceptor.cpp Acceptor.hpp)
add_executable(ex1v2 ${SOURCE_FILES})


set(SERVER Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp Acceptor.hpp Server.cpp)
add_executable(server ${SERVER})

set(CLIENT Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp Client.cpp)
add_executable(client ${CLIENT})