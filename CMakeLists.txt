cmake_minimum_required(VERSION 3.6)
project(ex1v2)
set(LDFLAGS "-pthread")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)


SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LDFLAGS} -g -lm -Wall -Wextra -o")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LDFLAGS} -lm -Wall -Wextra -o")


set(SSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp Acceptor.hpp SingleStreamTcpServer.cpp)
add_executable(SingleStreamTcpServer ${SSTS})

set(SSTC Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp SingleStreamTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(SingleStreamTcpClient ${SSTC})

set(MSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp MultiStreamTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(MultiStreamTcpClient ${MSTS})

set(MTTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
        Acceptor.hpp MultiThreadTcpServer.cpp Metrics.cpp Metrics.hpp)
add_executable(MultiThreadTcpServer ${MTTS})

set(VSTC Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp VaryingSizeTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(VaryingSizeTcpClient ${VSTC})

set(MTTC Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
        Acceptor.hpp MultiThreadTcpClient.cpp MultiThreadTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(MultiThreadTcpClient ${MTTC})

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

set(SinglePingp1 rc_pingpong.c rc_pingpong.h pingpong.h
        pingpong.c MetricsIBV.c MetricsIBV.h)
add_executable(SinglePingp ${SinglePingp1} MetricsIBV.c MetricsIBV.h)
target_link_libraries(SinglePingp "${GCC_COVERAGE_LINK_FLAGS}")

set(MultiStreamIBFiles multistreamPPSupport.c multistreamPPSupport.h
        multistreamTestRunner.c MetricsIBV.c MetricsIBV.h)
add_executable(MultiStreamIB ${MultiStreamIBFiles})
target_link_libraries(MultiStreamIB "${GCC_COVERAGE_LINK_FLAGS}")

