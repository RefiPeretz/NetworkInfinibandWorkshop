# the compiler: gcc for C program, define as g++ for C++
CC = gcc

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall

# the build target executable:
TARGET = myprog

all: MultiStreamIB MultiStreamTcpClient MultiThreadTcpClient MultiThreadTcpServer NonBlockingTcpServer

MetricsIBV: MetricsIBV.c MetricsIBV.h
	gcc MetricsIBV.c MetricsIBV.h -o

MultistreamPPSupport: multistreamPPSupport.c multistreamPPSupport.h
	gcc multistreamPPSupport.c multistreamPPSupport.h -o

MultiStreamIB: multistreamPPSupport.o multistreamTestRunner.c MetricsIBV.o
	gcc multistreamPPSupport.o MetricsIBV.o multistreamTestRunner.c -libverbs -lpthread -lm -o MultiStreamIB

Acceptor: Acceptor.cpp Acceptor.hpp
	g++ Acceptor -o Acceptor.cpp Acceptor.hpp

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) $(TARGET)
