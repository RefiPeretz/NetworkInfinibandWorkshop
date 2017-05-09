cmake_minimum_required(VERSION 3.6)
project(ex1v2)
set(LDFLAGS "-pthread")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)


SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LDFLAGS} -g -lm -Wall -Wextra -o")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LDFLAGS} -g -lm -Wall -Wextra -o")


set(MSTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp MultiStreamTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(MultiStreamTcpClient ${MSTS})

set(MTTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
        Acceptor.hpp MultiThreadTcpServer.cpp Metrics.cpp Metrics.hpp)
add_executable(MultiThreadTcpServer ${MTTS})


set(MTTC Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
        Acceptor.hpp MultiThreadTcpClient.cpp MultiThreadTcpClient.cpp Metrics.cpp Metrics.hpp)
add_executable(MultiThreadTcpClient ${MTTC})

set(NBTS Stream.cpp Stream.hpp Connector.cpp Connector.hpp Acceptor.cpp
    Acceptor.hpp NonBlockingTcpServer.cpp)
add_executable(NonBlockingTcpServer ${NBTS})

#Verbs Api versions
SET(GCC_COVERAGE_LINK_FLAGS "-lm -g -libverbs")

set(multithreadIB multithreadIB.c multithreadIB.h multithreadIBSupport.h
        multithreadIBSupport.c MetricsIBV.c MetricsIBV.h)
add_executable(multithreadIB ${multithreadIB})
target_link_libraries(multithreadIB "${GCC_COVERAGE_LINK_FLAGS}")

set(MultiStreamIBFiles multistreamPPSupport.c multistreamPPSupport.h
        multistreamTestRunner.c MetricsIBV.c MetricsIBV.h)
add_executable(MultiStreamIB ${MultiStreamIBFiles})
target_link_libraries(MultiStreamIB "${GCC_COVERAGE_LINK_FLAGS}")



