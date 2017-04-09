//
// Created by refi950 on 08/04/17.
// This imitiates multiple clients connecting in parallel
//

#include <cstdlib>
#include <string>
#include <sys/time.h>
#include "Stream.hpp"
#include "Connector.hpp"
using namespace std;
void* client(void* data);
struct threadData
{
    int port;
    char* server;
};

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("usage: %s <number of max parallel clients>\n", argv[1]);
        exit(1);
    }
    unsigned int clientNum = atoi(argv[1]);
    unsigned int counter = 0;
    threadData* data;
    data->server = "localhost";
    data->port = 8081;
    pthread_t *socketThreads = new pthread_t[clientNum];
    printf("\n===========================================================\n");
    printf("Now checking %d parallel clients\n", clientNum);
    for (int j = 0; j < clientNum; j++) {

        if(pthread_create(&socketThreads[j], NULL, client, data))
        {
            printf("error create thread\n");
        }
    }
    for (int j = 0; j < clientNum; j++) {

        pthread_join(socketThreads[j],NULL);
    }


}

void* client(void* data)
{
    printf("In client\n");
    threadData* handlerData = (threadData*)(data);
    std::string serverAddress = handlerData->server;
    int port = handlerData->port;
    printf("usage: <server = %s> <port = %d>\n", serverAddress.c_str(), port);

    int len;
    char message = 'w';
    char ack;
    struct timeval start, end;
    double t1, t2;
    Connector *connector = new Connector();
    Stream *stream = connector->connect(serverAddress.c_str(), port);
    if (stream)
    {
        t1 = 0.0;
        t2 = 0.0;
        if (gettimeofday(&start, NULL))
        {
            printf("time failed\n");
            exit(1);
        }

        stream->send(&message, sizeof(char));
        printf("sent - %c with sizeof %d\n", message, (int) sizeof(char));
        len = stream->receive(&ack, sizeof(char));
        if (gettimeofday(&end, NULL))
        {
            printf("time failed\n");
            exit(1);
        }
        t1 += start.tv_sec + (start.tv_usec / 1000000.0);
        t2 += end.tv_sec + (end.tv_usec / 1000000.0);
        printf("received - %c\n", ack);
        //calculate and print mean rtt
        double rtt = (t2 - t1) / 100;
        printf("RTT = %g ms\n", rtt);
        printf("Packet Rate = 1 / %g = %g byte / s \n", rtt, 1000 / rtt);

        delete stream;
    }

    return nullptr;
}