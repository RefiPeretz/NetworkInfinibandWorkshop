//
// Created by refi950 on 5/3/17.
//


#include "MetricsIBV.h"

#define MICRO_TO_SEC pow(10, -6)
#define SEC_TO_NANO pow(10, 9)
#define MICRO_SEC_TO_NANO pow(10, 3)
#define BIT_TO_MBIT pow(10, -6)
#define SEC_TO_MICRO pow(10, 6)
#define MICRO_TO_SEC pow(10, -6)


double calcAverageRTT(int numOfSockets,size_t numOfMessages, double totalTime)
{
    return (totalTime / (double)numOfMessages)/numOfSockets;
}

double timeDifference(struct timeval time1, struct timeval time2)
{
    double res =  ((time2.tv_sec - time1.tv_sec) * SEC_TO_MICRO)
                  + ((time2.tv_usec - time1.tv_usec)); //TODO change to seconds
    // return abs of difference
    return res < 0 ? (-1)*res : res;
}

double calcAverageThroughput(size_t numOfMessages, size_t messageSize, double totalTime)
{
    return (2 * 8 * numOfMessages * messageSize * BIT_TO_MBIT) / (totalTime * MICRO_TO_SEC);
}

double calcAveragePacketRate(size_t numOfMessages, double totalTime)
{
    return (double) 2 * numOfMessages / (totalTime * MICRO_TO_SEC); //messages per second
}


int saveResults(double rtt,double throughput, double packetRate,int resultIndex,double *results,int numOfSockets,int msgSize, int totalNumOfMsg){
    results[resultIndex] = rtt;
    resultIndex++;
    results[resultIndex] = throughput;
    resultIndex++;
    results[resultIndex] = packetRate;
    resultIndex++;
    results[resultIndex] = numOfSockets;
    resultIndex++;
    results[resultIndex] = msgSize;
    resultIndex++;
    results[resultIndex] = totalNumOfMsg;
    resultIndex++;
    return resultIndex;
}

void createCSV(char *filename,double results[],int n){

    printf("\n Creating %s file",filename);
    int rttIndex,throIndex, pktRate,numOfSockets,packetSize,totalNumOfMsgs;
    FILE *fp;

    //filename=strcat(filename,".csv");

    fp=fopen(filename,"w+");

    fprintf(fp,"Results,Number of Threads/Sockets[Integer],Number Of messages[Integer],Size per message[Bytes],RTT[us],Throughput[Mib/s],Packet Rate[P/s],Packet Size[Bytes]\n");

    for(int i =0; i < n ;i+=6){
        rttIndex = i;
        throIndex = i + 1;
        pktRate = i + 2;
        numOfSockets = i+3;
        packetSize = i + 4;
        totalNumOfMsgs = i+5;

        fprintf(fp,",%f ",results[numOfSockets]);
        fprintf(fp,",%f ",results[totalNumOfMsgs]);
        fprintf(fp,",%f",results[packetSize]/results[numOfSockets]);
        fprintf(fp,",%f ",results[rttIndex]);
        fprintf(fp,",%f ",results[throIndex]);
        fprintf(fp,",%f ",results[pktRate]);
        fprintf(fp,",%f\n",results[packetSize]);

    }

    fclose(fp);

    printf("%s file created\n",filename);

}
