cmake_minimum_required(VERSION 3.6)
project(ex1v2)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES Stream.cpp Stream.hpp Connector.cpp Connector.hpp
    Acceptor.cpp Acceptor.hpp)
add_executable(ex1v2 ${SOURCE_FILES})

set(SSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp Acceptor.hpp SingleStreamTcpServer.cpp)
add_executable(SingleStreamTcpServer ${SSTS})

set(SSTC Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp SingleStreamTcpClient.cpp)
add_executable(SingleStreamTcpClient ${SSTC})

set(MSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp MultiStreamSizeClient.cpp)
add_executable(MultiStreamSizeClient ${MSTS})