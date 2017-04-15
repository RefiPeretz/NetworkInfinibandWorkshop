cmake_minimum_required(VERSION 3.6)
project(ex1v2)
SET(LDFLAGS "-pthread")
set(CMAKE_CXX_STANDARD 11)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LDFLAGS} -libverbs -Wall -Wextra")

set(SSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp Acceptor.hpp SingleStreamTcpServer.cpp)
add_executable(SingleStreamTcpServer ${SSTS})

set(SSTC Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp SingleStreamTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(SingleStreamTcpClient ${SSTC})

set(MSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp MultiStreamTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(MultiStreamTcpClient ${MSTS})

set(VSTC Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp VaryingSizeTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(VaryingSizeTcpClient ${VSTC})

set(MSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
        Acceptor.hpp MultiThreadTcpClient.cpp MultiThreadTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(MultiThreadTcpClient ${MSTS})

set(MTTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp MultiThreadTcpServer.cpp)
add_executable(MultiThreadTcpServer ${MTTS})

set(NBTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp NonBlockingTcpServer.cpp)
add_executable(NonBlockingTcpServer ${NBTS})