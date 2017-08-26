.PHONY: all clean

FLAGS = -Wall -Wextra -Wvla -libverbs

all: eager rand

eager: eagerClientServer.o pingpong.o MetricsIBV.o
	gcc -g $(FLAGS) eagerClientServer.o MetricsIBV.o pingpong.o -o  eager -lm

rand: randClientServer.o pingpong.o MetricsIBV.o
	gcc -g $(FLAGS) randClientServer.o MetricsIBV.o pingpong.o -o  rand -lm

eagerClientServer.o: eagerClientServer.c
	gcc -g -c $(FLAGS) eagerClientServer.c

randClientServer.o: randClientServer.c
	gcc -g -c $(FLAGS) randClientServer.c
	
pingpong.o: pingpong.c pingpong.h
	gcc -g -c $(FLAGS) pingpong.c
	
MetricsIBV.o: MetricsIBV.c MetricsIBV.h
	gcc -g -c $(FLAGS) MetricsIBV.c

clean:
	rm -f *.o eager rand