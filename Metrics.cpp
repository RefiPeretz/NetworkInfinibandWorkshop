//
// Created by raphael on 09/04/17.
//
#include <sys/time.h>
#include <cstdlib>
#include <string>
#include <memory.h>
#include "Connector.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <math.h>

#define MICRO_TO_SEC pow(10, -6)
#define SEC_TO_NANO pow(10, 9)
#define MICRO_SEC_TO_NANO pow(10, 3)
#define BIT_TO_MBIT pow(10, -6)
#define SEC_TO_MICRO pow(10, 6)
#define MICRO_TO_SEC pow(10, -6)



using namespace std;


void createResultFile(int resultLength,char* nameOfFile,double* results){
    int rttIndex,throIndex, pktRate,numOfSockets,packetSize,totalNumOfMsgs;

    ofstream myFile;
    myFile.open(nameOfFile);
    myFile << "Results\n";
    myFile << "Number of Threads/Sockets[Integer],Number Of messages[Integer],Size per message[Bytes],RTT[us],Throughput[Mib/s],Packet Rate[P/s],Packet Size[Bytes]\n";
    for(int i = 0 ; i < resultLength; i+=6) {
        rttIndex = i;
        throIndex = i + 1;
        pktRate = i + 2;
        numOfSockets = i+3;
        packetSize = i + 4;
        totalNumOfMsgs = i+5;
        std::stringstream rttSS(stringstream::in | stringstream::out);
        rttSS << setprecision(5) << results[rttIndex] << endl;
        std::stringstream throughputSS(stringstream::in | stringstream::out);
        throughputSS << setprecision(5) << results[throIndex] << endl;
        std::stringstream packetRateSS(stringstream::in | stringstream::out);
        packetRateSS << setprecision(5) << results[pktRate] << endl;
        std::stringstream packetSizeSS(stringstream::in | stringstream::out);
        packetSizeSS << setprecision(6) << results[packetSize] << endl;
        std::stringstream numOfSocketsSS(stringstream::in | stringstream::out);
        numOfSocketsSS << setprecision(1) << results[numOfSockets] << endl;
        std::stringstream totalNumOfMsgsSS(stringstream::in | stringstream::out);
        totalNumOfMsgsSS << setprecision(5) <<  results[totalNumOfMsgs] << endl;
        std::stringstream msgSizeSS(stringstream::in | stringstream::out);
        msgSizeSS << setprecision(5) <<  results[packetSize]/results[numOfSockets] << endl;
        std::string writeToFile = numOfSocketsSS.str()+","+totalNumOfMsgsSS.str()+","+msgSizeSS.str()+","+rttSS.str() +","+throughputSS.str()+","+packetRateSS.str()+","+packetSizeSS.str();
        writeToFile.erase(std::remove(writeToFile.begin(), writeToFile.end(), '\n'), writeToFile.end());
        myFile << writeToFile+"\n";
    }
    myFile.close();
}

void createMsg(int sizeMsg, char baseChar, char** msg){
    *msg = (char *)malloc(sizeMsg * sizeof(char) + 1);
    memset(*msg, baseChar, sizeMsg);
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




double calcAverageRTT(int numOfSockets,size_t numOfMessages, double totalTime)
{
    return (totalTime / (double)numOfMessages)/numOfSockets;
}
//
//double calcAverageThroughput(size_t numOfMessages, size_t messageSize, double totalTime)
//{
//    return (2 * numOfMessages * messageSize) / totalTime;
//}
//
//double calcAveragePacketRate(size_t numOfMessages, double totalTime)
//{
//    return (double) 2 * numOfMessages / totalTime; //messages per second
//}

double timeDifference(timeval time1, timeval time2)
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




