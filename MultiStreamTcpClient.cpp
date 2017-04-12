//
// Created by fimka on 13/03/17.
// This imitiates multiple clients connecting in parallel
//

#include <cstdlib>
#include <string>
#include <sys/time.h>
#include "Stream.hpp"
#include "Connector.hpp"
#include "Metrics.hpp"

using namespace std;


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
    char* msg;
    warmUpServer(8081);
    printf("=====Multi Stream Connection=====\n");
    for(int msgSize = 1; msgSize <= 1024;msgSize = msgSize * 2){
        t1 = 0.0;
        t2 = 0.0;
        createMsg(msgSize,'w',&msg);
        msg[msgSize] = '\0';
        if (gettimeofday(&start, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        for (int parallelClientNum = 0; parallelClientNum < clientNum;parallelClientNum++) {

            connects[parallelClientNum] = new Connector();
            streams[parallelClientNum] = connects[parallelClientNum]->connect(serverAddress.c_str(), 8081, 5000);

        }
        printf("Multiple sends\n");
        for (int parallelClientNum = 0; parallelClientNum < clientNum;
             parallelClientNum++) {
            char message = 'w';


            streams[parallelClientNum]->send(msg, msgSize);
            printf("sent - %c with sizeof %d\n", message, (int) sizeof(char));


        }
        printf("Multiple rcv\n");
        for (int parallelClientNum = 0; parallelClientNum < clientNum;
             parallelClientNum++) {
            char ack;

            streams[parallelClientNum]->receive(&ack, sizeof(char), 5);
            printf("received - %c\n", ack);
            delete streams[parallelClientNum];
        }
        if (gettimeofday(&end, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        t1 += start.tv_sec + (start.tv_usec / 1000000.0);
        t2 += end.tv_sec + (end.tv_usec / 1000000.0);
        double rtt = calcAverageRTT(clientNum, (t2-t1) / 100);
        double packetRate = calcAveragePacketRate(clientNum,(t2-t1) / 100);
        double throughput = calcAverageThroughput(clientNum,msgSize,(t2-t1) / 100);
        printf("avgRTT: %g\n", rtt);
        printf("avgPacketRate: %g\n", packetRate);
        printf("avgThroughput: %g\n", throughput);

        //calculate and print mean rtt
        free(msg);
    }



    exit(0);
}


