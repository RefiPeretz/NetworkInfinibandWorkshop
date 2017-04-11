//
// Created by raphael on 09/04/17.
//
#include <sys/time.h>
#include <cstdlib>
#include <math.h>
#include <memory.h>
#include "Connector.hpp"

#define SEC_TO_MICRO pow(10, 6)

using namespace std;

void createMsg(int sizeMsg, char baseChar, char** msg){
    *msg = (char *)malloc(sizeMsg * sizeof(char) + 1);
    memset(*msg, baseChar, sizeMsg);
}


void warmUpServer(int port,int numOfMsgs = 1000,std::string server = "localhost"){
    int len;
    char message = 'w';
    char ack;
    printf("====Starting warm up with %d msgs of 1 byte=====\n",numOfMsgs);
    for(int i = 0 ; i < numOfMsgs; i++){
        Connector *connector = new Connector();
        Stream *stream = connector->connect(server.c_str(), port);
        if (stream) {
            stream->send(&message, sizeof(char));
            printf("sent - %c with sizeof %d\n", message, (int) sizeof(char));
            len = stream->receive(&ack, sizeof(char));
            printf("received - %c\n", ack);
            //calculate and print mean rtt
            delete stream;
        }


    }
    printf("====Warmup is done=====\n",numOfMsgs);

}


double timeDifference(timeval time1, timeval time2)
{
    double res =  ((time2.tv_sec - time1.tv_sec) * SEC_TO_MICRO)
                  + ((time2.tv_usec - time1.tv_usec)); //TODO change to seconds
    // return abs of difference
    return res < 0 ? (-1)*res : res;
}


double calcAverageRTT(size_t numOfMessages, double totalTime)
{
    return totalTime / (double)numOfMessages;
}

double calcAverageThroughput(size_t numOfMessages, size_t messageSize, double totalTime)
{
    return (2 * numOfMessages * messageSize) / totalTime;
}

double calcAveragePacketRate(size_t numOfMessages, double totalTime)
{
    return (double) 2 * numOfMessages / totalTime; //messages per second
}

