cmake_minimum_required(VERSION 3.6)
project(ex1v2)
set(LDFLAGS "-lpthread")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)


SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LDFLAGS} -Wall -Wextra -o")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LDFLAGS} -libverbs -Wall -Wextra")


set(SSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp Acceptor.hpp SingleStreamTcpServer.cpp)
add_executable(SingleStreamTcpServer ${SSTS})

set(SSTC Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp SingleStreamTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(SingleStreamTcpClient ${SSTC})

set(MSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp MultiStreamTcpClient.cpp Metrics.cpp Metrics.hpp SimpleIBClient.hpp)
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

#Verbs Api versions
SET(GCC_COVERAGE_LINK_FLAGS "-g -libverbs")

#set(SIBC SingleInfiniBandClient.c CommonIBUtilFuncs.cpp CommonIBUtilFuncs.hpp)
#add_executable(SingleInfiniBandClient ${SIBC})
#target_link_libraries(SingleInfiniBandClient "${GCC_COVERAGE_LINK_FLAGS}")
#
#set(SIBS SingleInfiniBandServer.c CommonIBUtilFuncs.cpp CommonIBUtilFuncs.hpp)
#add_executable(SingleInfiniBandServer ${SIBS})
#target_link_libraries(SingleInfiniBandServer "${GCC_COVERAGE_LINK_FLAGS}")

#set(IBSC SimpleIBClient.cpp CommonIBUtilFuncs.cpp CommonIBUtilFuncs.hpp)
#add_executable(SimpleIBClient ${IBSC})
#target_link_libraries(SimpleIBClient "${GCC_COVERAGE_LINK_FLAGS}")
#
#set(IBSS SimpleIBServer.cpp CommonIBUtilFuncs.cpp CommonIBUtilFuncs.hpp)
#add_executable(SimpleIBServer ${IBSS})
#target_link_libraries(SimpleIBServer "${GCC_COVERAGE_LINK_FLAGS}")