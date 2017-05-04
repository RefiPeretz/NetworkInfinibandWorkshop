//
// Created by refi950 on 08/04/17.
// This imitiates multiple clients connecting in parallel
//

#include <cstdlib>
#include <sys/time.h>
#include <stdlib.h>
#include "Stream.hpp"
#include "Connector.hpp"
#include "Metrics.hpp"

using namespace std;
void* client(void* data);


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("usage: %s <number of max parallel clients>\n", argv[1]);
        exit(1);
    }
    warmUpServer(SERVER_PORT);
    struct timeval start, end;
    double t1, t2;
    unsigned int counter = 0;
    socketData* data = new socketData;
    int msgNum = atoi(argv[1]);
    data->msgNum = msgNum;
    std::string server = "localhost";
    data->baseWord = 'w';
    int resultSize = ((int)((log2(MAX_MSG_SIZE/MIN_MSG_SIZE) + 1) * 6))*4;
    double results[resultSize] = {0.0};
    int resultIndex = 0;
    int curMsgSize;
    for (int msgSize = MIN_MSG_SIZE; msgSize <= 4096; msgSize = msgSize * 2) {
        for(int coreCount = 1; coreCount <= MAX_CORE; coreCount*=2){
            int msgSizes[coreCount] = {0};
            char* msgs[coreCount];
            if(msgSize < coreCount){
                createMsg(msgSize,data->baseWord,&msgs[0]);
                msgs[0][msgSize] = '\0';
                msgSizes[0] = msgSize;
                for(int i = 1; i < coreCount;i++){
                    createMsg(1,data->baseWord,&msgs[i]);
                    msgs[i][1] = '\0';
                    msgSizes[i] = 1;
                }

            }
            else{
                for(int i = 1; i < coreCount;i++){
                    createMsg(msgSize/coreCount,data->baseWord,&msgs[i]);
                    msgs[i][msgSize/coreCount] = '\0';
                    msgSizes[i] = msgSize/coreCount;
                }
                createMsg((msgSize/coreCount) + (msgSize%coreCount),data->baseWord,&msgs[0]);
                msgs[0][(msgSize/coreCount) + (msgSize%coreCount)] = '\0';
                msgSizes[0] = (msgSize/coreCount) + (msgSize%coreCount);

            }

//            curMsgSize = msgSize/coreCount;
//            data->msgSize = curMsgSize;
//            createMsg(curMsgSize,data->baseWord,&data->msg);
//            data->msg[curMsgSize] = '\0';
            printf("=====Running on %d threads total size: %d size per thread: %d \n",coreCount,msgSize,curMsgSize);
//            Connector *connectors[coreCount] = {new Connector()};
//            Stream *streams[coreCount];
//            for(int stream = 0; stream < coreCount;stream++){
//                streams[stream] = connectors[stream]->connect(server.c_str(), 8081);
//            }
            pthread_t *socketThreads = new pthread_t[coreCount];
            t1 = 0.0;
            t2 = 0.0;
            printf("=====Clock Start=====\n");
            if (gettimeofday(&start, NULL)) {
                printf("time failed\n");
                exit(1);
            }
            for (int j = 0; j < coreCount; j++) {
                //TODO delete
                data->thread_num = j;
                data->msgSize = msgSizes[j];
                data->msg = msgs[j];
//                data->stream = streams[j];
                if(pthread_create(&socketThreads[j], NULL, client, data))
                {
                    printf("error create thread\n");
                }
            }
            for (int j = 0; j < coreCount; j++) {

                pthread_join(socketThreads[j],NULL);
            }
            if (gettimeofday(&end, NULL)) {
                printf("time failed\n");
                exit(1);

            }
//            for(int i = 0; i < coreCount;i++){
//                delete(streams[i]);
//                delete(connectors[i]);
//            }
            printf("=====Clock Stop=====\n");
            double totalTime = timeDifference(start,end);
            double rtt = calcAverageRTT(1,data->msgNum*coreCount, totalTime);
            double packetRate = calcAveragePacketRate(data->msgNum*coreCount,totalTime);
            double throughput = calcAverageThroughput(data->msgNum*coreCount,msgSize,totalTime);
            double numOfSockets = coreCount;
            printf("avgRTT: %g\n", rtt);
            printf("avgPacketRate: %g\n", packetRate);
            printf("avgThroughput: %g\n", throughput);
            resultIndex = saveResults(rtt,throughput,packetRate,resultIndex,results,coreCount,msgSize,data->msgNum*coreCount);
            for(int i = 0; i < coreCount;i++){
                free(msgs[i]);
            }

        }


    }

    createResultFile(resultSize,"MultiThreadResults.csv",results);

}

void* client(void* data)
{
    socketData* handlerData = (socketData*)(data);
    int msgNum = handlerData->msgNum;
    int msgSize = handlerData->msgSize;
    int thread_num = handlerData->thread_num;
    printf("====Start thread %d on msgSize %d====\n",thread_num,msgSize);
    int len;
    char ack[MAX_PACKET_SIZE];
    int bytesRead = 0;
    Connector *connector = new Connector();
    Stream *stream = connector->connect(SERVER_ADDRESS, SERVER_PORT);
    for(int i = 0;i < msgNum; i++){
        bytesRead = 0;
        if (stream)
        {
            stream->send(handlerData->msg, handlerData->msgSize);
            printf("sent - %s with sizeof %d, thread: %d\n", handlerData->msg, msgSize,thread_num);
            bytesRead += stream->receive(ack, MAX_PACKET_SIZE);
            while(bytesRead < msgSize){
                //printf("papo: %d,%d, thread: %d\n",bytesRead,msgSize, thread_num);
                bytesRead += stream->receive(ack, MAX_PACKET_SIZE);
                // printf("papo: now is: %d,%d, thread: %d, msg: %s\n",bytesRead,msgSize, thread_num, handlerData->msg);
            }
            //printf("len : %d thread: %d\n", bytesRead);
            printf("received - %d Bytes thread: %d\n", bytesRead,thread_num);
        }

    }
    return nullptr;
}