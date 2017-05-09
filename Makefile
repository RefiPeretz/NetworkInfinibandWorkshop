# the compiler: gcc for C program, define as g++ for C++
CC = gcc
CPP = g++

CFLAGS  = -libverbs -lpthread -lm
CPPFlags = -lpthread -lm
Targets =  MultiStreamIB MultiStreamTcpClient MultiThreadTcpClient MultiThreadTcpServer NonBlockingTcpServer multithreadIB
# the build target executable:

all: $(Targets)

MetricsIBV: MetricsIBV.c MetricsIBV.h
	gcc MetricsIBV.c MetricsIBV.h  $(CFLAGS) -c

Acceptor: Acceptor.cpp Acceptor.hpp
	g++ Acceptor.cpp Acceptor.hpp $(CPPFlags) -c 
	
Connector: Connector.cpp Connector.hpp
	g++  Connector.cpp Connector.hpp $(CPPFlags) -c

Stream: Stream.cpp Stream.hpp
	g++ Stream.cpp Stream.hpp $(CPPFlags) -c
	
Metrics: Metrics.cpp Metrics.hpp
	g++ Metrics.cpp Metrics.hpp $(CPPFlags) -c

MultistreamPPSupport: multistreamPPSupport.c multistreamPPSupport.h
	gcc multistreamPPSupport.c multistreamPPSupport.h $(CFLAGS) -c

MultiStreamIB: multistreamPPSupport.o multistreamTestRunner.c MetricsIBV.o
	gcc multistreamPPSupport.h MetricsIBV.h multistreamTestRunner.c  $(CFLAGS) -o MultiStreamIB multistreamPPSupport.o MetricsIBV.o

multithreadIB: multithreadIB.c multithreadIB.h  MetricsIBV.o
	gcc multistreamPPSupport.h MetricsIBV.h multithreadIB.c  $(CFLAGS) -o multithreadIB multistreamPPSupport.o MetricsIBV.o

MultiStreamTcpClient: MultiStreamTcpClient.cpp Metrics.o Stream.o Connector.o Acceptor.o
	g++ MultiStreamTcpClient.cpp Stream.hpp Connector.hpp Acceptor.hpp Metrics.hpp $(CPPFlags) -o MultiStreamTcpClient Metrics.o Stream.o Connector.o Acceptor.o
	
MultiThreadTcpClient: MultiThreadTcpClient.cpp Stream.o Connector.o Acceptor.o Metrics.o
	g++ Connector.hpp Acceptor.hpp Metrics.hpp Stream.hpp MultiThreadTcpClient.cpp $(CPPFlags) -o MultiThreadTcpClient Metrics.o Stream.o Connector.o Acceptor.o
	
MultiThreadTcpServer: MultiThreadTcpServer.cpp Stream.o Connector.o Acceptor.o Metrics.o
	g++ Connector.hpp Acceptor.hpp Metrics.hpp Stream.hpp MultiThreadTcpServer.cpp $(CPPFlags) -o MultiThreadTcpServer Metrics.o Stream.o Connector.o Acceptor.o
	
NonBlockingTcpServer: NonBlockingTcpServer.cpp Stream.o Connector.o Acceptor.o Metrics.o
	g++ Connector.hpp Acceptor.hpp Metrics.hpp Stream.hpp NonBlockingTcpServer.cpp $(CPPFlags) -o NonBlockingTcpServer Metrics.o Stream.o Connector.o Acceptor.o

clean:
	rm -rf *.o $(Targets)


