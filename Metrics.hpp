//
// Created by raphael on 09/04/17.
//

#ifndef EX1V2_METRICS_H
#define EX1V2_METRICS_H

#include <sstream>
typedef struct{
    int port;
    char baseWord = 'w';
    char* server;
    char* msg;
    int msgSize;
}socketData;

void warmUpServer(int port,int numOfMsgs = 1000, std::string server = "localhost");

void createMsg(int sizeMsg, char baseChar, char** msg);

double timeDifference(timeval time1, timeval time2);

double calcAverageRTT(size_t numOfMessages, double totalTime);


double calcAverageThroughput(size_t numOfMessages, size_t messageSize, double totalTime);


double calcAveragePacketRate(size_t numOfMessages, double totalTime);



#endif //EX1V2_METRICS_H
