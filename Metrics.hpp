//
// Created by raphael on 09/04/17.
//

#ifndef EX1V2_METRICS_H
#define EX1V2_METRICS_H
#define MAX_PACKET_SIZE 1024

#include <sstream>
#include <math.h>
#include <math.h>
typedef struct{
    int port;
    char baseWord = 'w';
    char* server;
    char* msg;
    int msgSize;
}socketData;

void createResultFile(int numOfMsgs,char* nameOfFile,double* results);

void warmUpServer(int port,int numOfMsgs = 1000, std::string server = "localhost");

void createMsg(int sizeMsg, char baseChar, char** msg);

int saveResults(double rtt,double throughput, double packetRate,int resultIndex,double *results);

double timeDifference(timeval time1, timeval time2);

double calcAverageRTT(size_t numOfMessages, double totalTime);


double calcAverageThroughput(size_t numOfMessages, size_t messageSize, double totalTime);


double calcAveragePacketRate(size_t numOfMessages, double totalTime);



#endif //EX1V2_METRICS_H
