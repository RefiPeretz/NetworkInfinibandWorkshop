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
    char ack[MAX_MSG_SIZE];
    struct timeval start, end;
    double t1, t2;
    double results[1000] = {0.0};
    int resultIndex = 0;
    int curMsgSize;
    for (int msgSize = MIN_MSG_SIZE; msgSize <= 4096; msgSize = msgSize * 2){
        for(int socketNum = 1; socketNum < MAX_CLIENTS; socketNum++){
            t1 = 0.0;
            t2 = 0.0;
            int msgSizes[socketNum] = {0};
            char* msgs[socketNum];
            if(msgSize < socketNum){
                createMsg(msgSize,'w',&msgs[0]);
                msgs[0][msgSize] = '\0';
                msgSizes[0] = msgSize;
                for(int i = 1; i < socketNum;i++){
                    createMsg(1,'w',&msgs[i]);
                    msgs[i][1] = '\0';
                    msgSizes[i] = 1;
                }

            }
            else{
                for(int i = 1; i < socketNum;i++){
                    createMsg(msgSize/socketNum,'w',&msgs[i]);
                    msgs[i][msgSize/socketNum] = '\0';
                    msgSizes[i] = msgSize/socketNum;
                }
                createMsg((msgSize/socketNum) + (msgSize%socketNum),'w',&msgs[0]);
                msgs[0][(msgSize/socketNum) + (msgSize%socketNum)] = '\0';
                msgSizes[0] = (msgSize/socketNum) + (msgSize%socketNum);

            }

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
                    if (streams[stream]) {
                        streams[stream]->send(msgs[stream], msgSizes[stream]);
                        printf("sent - %s with sizeof %d\n", msgs[stream], msgSizes[stream]);
                        bytesRead +=  streams[stream]->receive(ack, MAX_MSG_SIZE);
                        //printf("Bytes read before loop %d\n",bytesRead);
                        while(bytesRead < msgSizes[stream]){
                            bytesRead +=  streams[stream]->receive(ack, MAX_MSG_SIZE);
                        }
                        printf("received - %d Bytes\n", bytesRead);


                    }
                }
            }

            if (gettimeofday(&end, NULL)) {
                printf("time failed\n");
                exit(1);
            }
            double totalTime = timeDifference(start,end);
            double rtt = calcAverageRTT(1,socketNum*numMsgs, totalTime);
            double packetRate = calcAveragePacketRate(socketNum*numMsgs,totalTime);
            double throughput = calcAverageThroughput(socketNum*numMsgs,msgSize,totalTime);
            double numOfSockets = 1;
            printf("avgRTT: %g\n", rtt);
            printf("avgPacketRate: %g\n", packetRate);
            printf("avgThroughput: %g\n", throughput);
            resultIndex = saveResults(rtt,throughput,packetRate,resultIndex,results,socketNum,msgSize,numMsgs*socketNum);
            for(int stream = 0; stream < socketNum;stream++){
                delete(streams[stream]);
                free(msgs[stream]);
            }

        }


    }
    createResultFile(1000,"MultiStreamResults.csv",results);


}
