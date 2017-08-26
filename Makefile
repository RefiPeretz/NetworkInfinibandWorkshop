.PHONY: all clean

FLAGS = -Wall -Wextra -Wvla -libverbs

all: clientP3 serverP3

serverP3: eagerServerP3.o pingpong.o MetricsIBV.o
	gcc -g $(FLAGS) eagerServerP3.o MetricsIBV.o pingpong.o -o  serverP3 -lm

clientP3: eagerClientP3.o pingpong.o MetricsIBV.o
	gcc -g $(FLAGS) eagerClientP3.o MetricsIBV.o pingpong.o -o  clientP3 -lm

eagerServerP3.o: eagerServerP3.c
	gcc -g -c $(FLAGS) eagerServerP3.c

eagerClientP3.o: eagerClientP3.c
	gcc -g -c $(FLAGS) eagerClientP3.c

pingpong.o: pingpong.c pingpong.h
	gcc -g -c $(FLAGS) pingpong.c
	
MetricsIBV.o: MetricsIBV.c MetricsIBV.h
	gcc -g -c $(FLAGS) MetricsIBV.c

clean:
	rm -f *.o *.gch clientP3 serverP3