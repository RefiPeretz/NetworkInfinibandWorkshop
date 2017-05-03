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
    int bytesRead;
    char* message;
    char ack[MAX_MSG_SIZE];
    struct timeval start, end;
    double t1, t2;
    int resultSize = (int)((log2(MAX_MSG_SIZE) + 1) * 6);
    double results[resultSize] = {0.0};
    int resultIndex = 0;
    for (int msgSize = 1; msgSize <= MAX_MSG_SIZE; msgSize = msgSize * 2){
        t1 = 0.0;
        t2 = 0.0;
        createMsg(msgSize,'w',&message);
        message[msgSize] = '\0';
        if (gettimeofday(&start, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        Connector *connector = new Connector();
        Stream *stream = connector->connect(argv[2], atoi(argv[1]));
        for(int i = 0 ; i < numMsgs; i++){
            bytesRead = 0;
            if (stream) {
                stream->send(message, msgSize);
                printf("sent - %s with sizeof %d\n", message, msgSize);
                while(bytesRead < msgSize){
                    bytesRead += stream->receive(ack, MAX_MSG_SIZE);
                }
                printf("received - %d Bytes\n", bytesRead);

            }
        }
        if (gettimeofday(&end, NULL)) {
            printf("time failed\n");
            exit(1);
        }

        double totalTime = timeDifference(start,end);
        double rtt = calcAverageRTT(1,numMsgs, totalTime);
        double packetRate = calcAveragePacketRate(numMsgs,totalTime);
        double throughput = calcAverageThroughput(numMsgs,msgSize,totalTime);
        double numOfSockets = 1;
        printf("avgRTT: %g\n", rtt);
        printf("avgPacketRate: %g\n", packetRate);
        printf("avgThroughput: %g\n", throughput);
        resultIndex = saveResults(rtt,throughput,packetRate,resultIndex,results,numOfSockets,msgSize,numMsgs);
        free(message);
        delete stream;

    }
    createResultFile(resultSize,"SingleStreamResults.csv",results);


}
