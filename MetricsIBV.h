//
// Created by refi950 on 5/3/17.
//

#ifndef EX1V2_METRICSIBV_H
#define EX1V2_METRICSIBV_H

#include <math.h>
#include <sys/time.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEFAULT_THREAD_TIME -1
#define DEFAULT_NUM_OF_MSGS 1000
#define MAX_MSG_SIZE 1048576


double calcAverageRTT(int numOfSockets,size_t numOfMessages, double totalTime);

double timeDifference(struct timeval time1, struct timeval time2);

double calcAverageRTT(int numOfSockets,size_t numOfMessages, double totalTime);

void createCSV(char *filename,double results[],int n);

double calcAverageThroughput(size_t numOfMessages, size_t messageSize, double totalTime);

void createMsg(int sizeMsg, char baseChar, char** msg);

double calcAveragePacketRate(size_t numOfMessages, double totalTime);

int saveResults(double rtt,double throughput, double packetRate,int resultIndex,double results[],int numOfSockets,int msgSize,int totalNumOfMsg);

#endif //EX1V2_METRICSIBV_H

