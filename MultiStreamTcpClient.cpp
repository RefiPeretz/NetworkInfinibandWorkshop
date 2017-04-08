//
// Created by fimka on 13/03/17.
// This imitiates multiple clients connecting in parallel
//

#include <cstdlib>
#include <string>
#include <sys/time.h>
#include "Stream.hpp"
#include "Connector.hpp"

using namespace std;

int client(std::string serverAddress, int port);

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: %s <number of max parallel clients>\n", argv[1]);
        exit(1);
    }
    unsigned int clientNum = atoi(argv[1]);
    unsigned int counter = 0;
    std::string serverAddress = "localhost";
    Connector *connects[clientNum];
    Stream *streams[clientNum];
    struct timeval start, end;
    double t1, t2;
    printf("Bulid connetc\n");
    t1 = 0.0;
    t2 = 0.0;
    if (gettimeofday(&start, NULL)) {
        printf("time failed\n");
        exit(1);
    }
    for (int parallelClientNum = 0; parallelClientNum < clientNum;parallelClientNum++) {

        connects[parallelClientNum] = new Connector();
        streams[parallelClientNum] = connects[parallelClientNum]->connect(serverAddress.c_str(), 8081);

    }
    printf("Multiple sends\n");
    for (int parallelClientNum = 0; parallelClientNum < clientNum;
         parallelClientNum++) {
        char message = 'w';


        streams[parallelClientNum]->send(&message, sizeof(char));
        printf("sent - %c with sizeof %d\n", message, (int) sizeof(char));


    }
    printf("Multiple rcv\n");
    for (int parallelClientNum = 0; parallelClientNum < clientNum;
         parallelClientNum++) {
        char ack;

        streams[parallelClientNum]->receive(&ack, sizeof(char));
        printf("received - %c\n", ack);
        delete streams[parallelClientNum];
    }
    if (gettimeofday(&end, NULL)) {
        printf("time failed\n");
        exit(1);
    }
    t1 += start.tv_sec + (start.tv_usec / 1000000.0);
    t2 += end.tv_sec + (end.tv_usec / 1000000.0);

    //calculate and print mean rtt
    double rtt = (((t2 - t1) / 100))/clientNum;
    printf("Total time %g ",rtt);



    //TODO add delete
    exit(0);
}

int client(std::string serverAddress, int port) {

    printf("usage: <server = %s> <port = %d>\n", serverAddress.c_str(), port);

//  int len;
//  char message = 'w';
//  char ack;
//  struct timeval start, end;
//  double t1, t2;
//  Connector *connector = new Connector();
//  Stream *stream = connector->connect(serverAddress.c_str(), port);
//  if (stream)
//  {
//	t1 = 0.0;
//	t2 = 0.0;
//	if (gettimeofday(&start, NULL))
//	{
//	  printf("time failed\n");
//	  exit(1);
//	}
//
//	stream->send(&message, sizeof(char));
//	printf("sent - %c with sizeof %d\n", message, (int) sizeof(char));
//	len = stream->receive(&ack, sizeof(char));
//	if (gettimeofday(&end, NULL))
//	{
//	  printf("time failed\n");
//	  exit(1);
//	}
//	t1 += start.tv_sec + (start.tv_usec / 1000000.0);
//	t2 += end.tv_sec + (end.tv_usec / 1000000.0);
//	printf("received - %c\n", ack);
//	//calculate and print mean rtt
//	double rtt = (t2 - t1) / 100;
//	printf("RTT = %g ms\n", rtt);
//	printf("Packet Rate = 1 / %g = %g byte / s \n", rtt, 1000 / rtt);
//
//	delete stream;
//  }
//
//  exit(0);
    return 0;
}
