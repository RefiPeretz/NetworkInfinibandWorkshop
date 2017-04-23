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
//    warmUpServer(atoi(argv[1]));

    int numMsgs = atoi(argv[3]);
    int len;
    int bytesRead;
    char* message;
    char ack[MAX_MSG_SIZE];
    struct timeval start, end;
    double t1, t2;
   // int resultSize = (int)((log2(MAX_MSG_SIZE) + 1) * 6);
    //double results[resultSize] = {0.0};
    int resultIndex = 0;
    int curMsgSize;
    for (int msgSize = MIN_MSG_SIZE; msgSize <= MAX_MSG_SIZE; msgSize = msgSize * 2){
        for(int socketNum = 1;socketNum < MAX_CORE;socketNum*=2){
            t1 = 0.0;
            t2 = 0.0;
            curMsgSize = msgSize/socketNum;
            createMsg(curMsgSize,'w',&message);
            message[curMsgSize] = '\0';
//            Connector *connectors[socketNum] = {new Connector()};
            Connector connector;
            Stream *streams[socketNum];
            if (gettimeofday(&start, NULL)) {
                printf("time failed\n");
                exit(1);
            }
            for(int stream = 0; stream < socketNum;stream++){
//                streams[stream] = connectors[stream]->connect(SERVER_ADDRESS, SERVER_PORT, 5);
                streams[stream] = connector.connect(SERVER_ADDRESS, SERVER_PORT);
                for(int i = 0 ; i < numMsgs; i++){
                    bytesRead = 0;
                    if ( streams[stream]) {
                        streams[stream]->send(message, curMsgSize);
                        printf("sent - %s with sizeof %d\n", message, curMsgSize);
                        while(bytesRead < curMsgSize){
                            bytesRead +=  streams[stream]->receive(ack, MAX_MSG_SIZE,1);
                        }
                        printf("received - %d Bytes\n", bytesRead);

                    }
                }
               delete(streams[stream]);
            }

            if (gettimeofday(&end, NULL)) {
                printf("time failed\n");
                exit(1);
            }
            t1 += start.tv_sec + (start.tv_usec / 1000000.0);
            t2 += end.tv_sec + (end.tv_usec / 1000000.0);
            double rtt = calcAverageRTT(1,numMsgs, (t2-t1) / 100);
            double packetRate = calcAveragePacketRate(numMsgs,(t2-t1) / 100);
            double throughput = calcAverageThroughput(numMsgs,msgSize,(t2-t1) / 100);
            double numOfSockets = 1;
            printf("avgRTT: %g\n", rtt);
            printf("avgPacketRate: %g\n", packetRate);
            printf("avgThroughput: %g\n", throughput);
            //resultIndex = saveResults(rtt,throughput,packetRate,resultIndex,results,numOfSockets,msgSize,numMsgs);
            free(message);
//            for(int stream = 0; stream < socketNum;stream++){
//                delete(streams);
//            }

        }


    }
    //createResultFile(resultSize,"SingleStreamResults.csv",results);


}
