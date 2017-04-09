//
// Created by raphael on 09/04/17.
//
#include <sys/time.h>
#include <cstdlib>
#include <math.h>
#define SEC_TO_MICRO pow(10, 6)

using namespace std;

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

