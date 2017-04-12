//
// Created by refi950 on 08/04/17.
// This imitiates multiple clients connecting in parallel
//

#include <cstdlib>
#include <sys/time.h>
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
    warmUpServer(8081);
    struct timeval start, end;
    double t1, t2;
    t1 = 0.0;
    t2 = 0.0;
    unsigned int numMsgs = atoi(argv[1]);
    unsigned int counter = 0;
    pthread_t *socketThreads = new pthread_t[numMsgs];
    socketData* data = new socketData;
    data->port = 8081;
    data->server = "localhost";
    data->baseWord = 'w';
    double results[(int)((log2(MAX_PACKET_SIZE) + 1) * 3)] = {0.0};
    int resultIndex = 0;
    for (int msgSize = 1; msgSize <= MAX_PACKET_SIZE; msgSize = msgSize * 2) {
        data->msgSize = msgSize;
        createMsg(msgSize,data->baseWord,&data->msg);
        data->msg[msgSize] = '\0';
        printf("=====Clock Start=====\n");
        if (gettimeofday(&start, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        for (int j = 0; j < numMsgs; j++) {

            if(pthread_create(&socketThreads[j], NULL, client, data))
            {
                printf("error create thread\n");
            }
        }
        for (int j = 0; j < numMsgs; j++) {

            pthread_join(socketThreads[j],NULL);
        }
        if (gettimeofday(&end, NULL)) {
            printf("time failed\n");
            exit(1);

        }
        printf("=====Clock Stop=====\n");
        t1 += start.tv_sec + (start.tv_usec / 1000000.0);
        t2 += end.tv_sec + (end.tv_usec / 1000000.0);
        double rtt = calcAverageRTT(numMsgs, (t2-t1) / 100);
        double packetRate = calcAveragePacketRate(numMsgs,(t2-t1) / 100);
        double throughput = calcAverageThroughput(numMsgs,msgSize,(t2-t1) / 100);
        printf("avgRTT: %g\n", rtt);
        printf("avgPacketRate: %g\n", packetRate);
        printf("avgThroughput: %g\n", throughput);
        resultIndex = saveResults(rtt,throughput,packetRate,resultIndex,results);
        free(data->msg);
    }

    createResultFile(numMsgs,"MultiThreadResults.csv",results);

}

void* client(void* data)
{
    printf("In client\n");
    std::string serverAddress ="localhost";
    socketData* handlerData = (socketData*)(data);
    int port = handlerData->port;
    printf("usage: <server = %s> <port = %d>\n", serverAddress.c_str(), port);

    int len;
    char ack;
    Connector *connector = new Connector();
    Stream *stream = connector->connect(serverAddress.c_str(), port);
    if (stream)
    {

        stream->send(handlerData->msg, handlerData->msgSize);
        printf("sent - %s with sizeof %d\n", handlerData->msg, handlerData->msgSize);
        len = stream->receive(&ack, sizeof(char));
        printf("received - %c\n", ack);
        delete stream;
    }

    return nullptr;
}