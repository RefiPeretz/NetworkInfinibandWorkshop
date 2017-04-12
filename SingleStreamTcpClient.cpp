//
// Created by fimka on 13/03/17.
//

#include <cstdlib>
#include <string>
#include <sys/time.h>
#include "Stream.hpp"
#include "Connector.hpp"
#include "Metrics.hpp"



using namespace std;


int main(int argc, char **argv) {
    if (argc != 4) {
        printf("usage: %s <port> <ip> <number of msgs>\n", argv[0]);
        exit(1);
    }
    warmUpServer(atoi(argv[1]));

    int numMsgs = atoi(argv[3]);
    int len;
    char* message;
    char ack;
    struct timeval start, end;
    double t1, t2;
    double results[(int)((log2(MAX_PACKET_SIZE) + 1) * 3)] = {0.0};
    int resultIndex = 0;
    for (int msgSize = 1; msgSize <= MAX_PACKET_SIZE; msgSize = msgSize * 2){
        t1 = 0.0;
        t2 = 0.0;
        createMsg(msgSize,'w',&message);
        message[msgSize] = '\0';
        if (gettimeofday(&start, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        for(int i = 0 ; i < numMsgs; i++){
            Connector *connector = new Connector();
            Stream *stream = connector->connect(argv[2], atoi(argv[1]));
            if (stream) {

                stream->send(message, msgSize);
                printf("sent - %s with sizeof %d\n", message, msgSize);
                len = stream->receive(&ack, sizeof(char));
                printf("received - %c\n", ack);
                //calculate and print mean rtt
                delete stream;

            }


        }
        if (gettimeofday(&end, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        t1 += start.tv_sec + (start.tv_usec / 1000000.0);
        t2 += end.tv_sec + (end.tv_usec / 1000000.0);
        double rtt = calcAverageRTT(numMsgs, (t2-t1) / 100);
        double packetRate = calcAveragePacketRate(numMsgs,(t2-t1) / 100);
        double throughput = calcAverageThroughput(numMsgs,msgSize,(t2-t1) / 100);
        printf("avgRTT: %g\n", rtt);
        printf("avgPacketRate: %g\n", packetRate);
        printf("avgThroughput: %g\n", throughput);
        resultIndex = saveResults(rtt,throughput,packetRate,resultIndex,results);
        free(message);

    }
    createResultFile(numMsgs,"SingleStreamResults.csv",results);


}
