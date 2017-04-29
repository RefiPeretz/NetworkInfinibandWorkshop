//
// Created by raphael on 09/04/17.
//

#ifndef EX1V2_METRICS_H
#define EX1V2_METRICS_H
#define MAX_MSG_SIZE 1048576
#define MAX_PACKET_SIZE 256
#define MAX_CORE 8
#define SERVER_ADDRESS "localhost"
#define SERVER_PORT 8081
#define MIN_MSG_SIZE 8

#include <sstream>
#include <math.h>
#include <math.h>
#include "Connector.hpp"

#ifdef __cplusplus
extern "C" {

#endif
#endif


typedef struct{
    int thread_num;
    int msgNum;
    char baseWord = 'w';
    char* msg;
    int msgSize;
}socketData;

void createResultFile(int resultLength,char* nameOfFile,double* results);

void warmUpServer(int port,int numOfMsgs = 1000, std::string server = "localhost");

void createMsg(int sizeMsg, char baseChar, char** msg);

int saveResults(double rtt,double throughput, double packetRate,int resultIndex,double *results,int numOfSockets,int msgSize,int totalNumOfMsg);

double timeDifference(timeval time1, timeval time2);

double calcAverageRTT(int numOfSockets,size_t numOfMessages, double totalTime);


double calcAverageThroughput(size_t numOfMessages, size_t messageSize, double totalTime);


double calcAveragePacketRate(size_t numOfMessages, double totalTime);


#ifdef __cplusplus
}
#endif
#endif //EX1V2_METRICS_H
