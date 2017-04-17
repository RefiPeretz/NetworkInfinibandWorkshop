cmake_minimum_required(VERSION 3.6)
project(ex1v2)
set(LDFLAGS "-pthread")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)


SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LDFLAGS} -Wall -Wextra")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LDFLAGS} -libverbs -Wall -Wextra")


set(SSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp Acceptor.hpp SingleStreamTcpServer.cpp)
add_executable(SingleStreamTcpServer ${SSTS})

set(SSTC Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp SingleStreamTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(SingleStreamTcpClient ${SSTC})

set(MSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp MultiStreamTcpClient.cpp Metrics.cpp Metrics.hpp SingleInfiniBandServer.cpp)
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

#Verbs versions
SET(GCC_COVERAGE_LINK_FLAGS "-lrdmacm -libverbs -lpthread")
#
#set(tempVerbs temp.cpp pingpong.cpp pingpong.hpp)
#add_executable(tempVerbs ${tempVerbs})
#target_link_libraries(tempVerbs "${GCC_COVERAGE_LINK_FLAGS}")

set(SIBC SingleInfiniBandClient.c)
add_executable(SingleInfiniBandClient ${SIBC})
target_link_libraries(SingleInfiniBandClient "${GCC_COVERAGE_LINK_FLAGS}")
#
#
#set(SIBS SingleInfiniBandServer.cpp)
#add_executable(SingleInfiniBandServer ${SIBS})