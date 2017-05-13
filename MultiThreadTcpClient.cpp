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
    if (argc != 4)
    {
        printf("usage: %s<port> <number of messages per thread> <server>\n", argv[1]);
        exit(1);
    }
    int serverPort = atoi(argv[1]);
    warmUpServer(serverPort,DEFAULT_NUMBER_OF_MSGS,argv[3]);
    struct timeval start, end;
    double t1, t2;
    unsigned int counter = 0;
    //Prepare threads shared data.
    socketData* data = new socketData;
    int msgNum = atoi(argv[2]);
    data->msgNum = msgNum;
    data->serverPort = serverPort;
    data->baseWord = 'w';
    data->serverName = argv[3];
    int resultSize = ((int)((log2(MAX_MSG_SIZE/MIN_MSG_SIZE) + 1) * 6))*4;
    double results[resultSize] = {0.0};
    int resultIndex = 0;
    int curMsgSize;
    for (int msgSize = MIN_MSG_SIZE; msgSize <= MAX_MSG_SIZE; msgSize = msgSize * 2) {
        for(int coreCount = 1; coreCount <= MAX_CORE; coreCount*=2){
            //Prepare data
            int msgSizes[coreCount] = {0};
            char* msgs[coreCount];
            if(msgSize < coreCount){
                createMsg(msgSize,data->baseWord,&msgs[0]);
                msgs[0][msgSize] = '\0';
                msgSizes[0] = msgSize;
                for(int i = 1; i < coreCount;i++){
                    createMsg(0,data->baseWord,&msgs[i]);
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

            printf("=====Running on %d threads total size: %d size per thread: %d \n",coreCount,msgSize,curMsgSize);

            pthread_t *socketThreads = new pthread_t[coreCount];
            t1 = 0.0;
            t2 = 0.0;
            printf("=====Clock Start=====\n");
            if (gettimeofday(&start, NULL)) {
                printf("time failed\n");
                exit(1);
            }
            for (int j = 0; j < coreCount; j++) {
                data->thread_num = j;
                data->msgSize = msgSizes[j];
                data->msg = msgs[j];
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

            printf("=====Clock Stop=====\n");
            double totalTime = timeDifference(start,end);
            double rtt = calcAverageRTT(1,data->msgNum*coreCount, totalTime);
            double packetRate = calcAveragePacketRate(data->msgNum*coreCount,totalTime);
            double throughput = calcAverageThroughput(data->msgNum*coreCount,msgSizes[0],totalTime);
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
    int serverPort = handlerData->serverPort;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_num, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        perror("pthread_setaffinity_np");
        exit(-1);
    }

    printf("====Start thread %d on msgSize %d====\n",thread_num,msgSize);
    int len;
    char ack[MAX_PACKET_SIZE];
    int bytesRead = 0;
    Connector *connector = new Connector();
    Stream *stream = connector->connect(handlerData->serverName, serverPort);
    for(int i = 0;i < msgNum; i++){
        bytesRead = 0;
        if (stream)
        {
            stream->send(handlerData->msg, handlerData->msgSize);
            printf("Sent client %d, thread: %d\n", msgSize,thread_num);
            bytesRead += stream->receive(ack, MAX_PACKET_SIZE);
            while(bytesRead < msgSize){
                bytesRead += stream->receive(ack, MAX_PACKET_SIZE);
            }
            printf("received - %d Bytes thread: %d\n", bytesRead,thread_num);
        }

    }
    return nullptr;
}
