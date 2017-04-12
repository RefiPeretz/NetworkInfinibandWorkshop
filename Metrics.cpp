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

#define SEC_TO_MICRO pow(10, 6)
#define MAX_PACKET_SIZE 1024


using namespace std;


void createResultFile(int numOfMsgs,char* nameOfFile,double* results){
    int rttIndex,throIndex, pktRate;
    std::stringstream numOfMsgsSS(stringstream::in | stringstream::out);
    numOfMsgsSS << setprecision(5) << numOfMsgs << endl;
    ofstream myFile;
    myFile.open(nameOfFile);
    myFile << "Results\n";
    myFile << "Number Of messages[Integer],RTT[ms],Throughput[ms],Packet Rate[ms],Packet Size[bytes]\n";
    int packetSize = 1;
    for(int i = 0 ; i < (int)((log2(MAX_PACKET_SIZE) + 1) * 3); i+=3) {
        rttIndex = i;
        throIndex = i + 1;
        pktRate = i + 2;
        std::stringstream rttSS(stringstream::in | stringstream::out);
        rttSS << setprecision(5) << results[rttIndex] << endl;
        std::stringstream throughputSS(stringstream::in | stringstream::out);
        throughputSS << setprecision(5) << results[throIndex] << endl;
        std::stringstream packetRateSS(stringstream::in | stringstream::out);
        packetRateSS << setprecision(5) << results[pktRate] << endl;
        std::stringstream packetSizeSS(stringstream::in | stringstream::out);
        packetSizeSS << setprecision(1) << packetSize << endl;
        std::string writeToFile = numOfMsgsSS.str()+","+rttSS.str() +","+throughputSS.str()+","+packetRateSS.str()+","+packetSizeSS.str();
        writeToFile.erase(std::remove(writeToFile.begin(), writeToFile.end(), '\n'), writeToFile.end());
        myFile << writeToFile+"\n";
        packetSize *= 2;
    }
    myFile.close();
}

void createMsg(int sizeMsg, char baseChar, char** msg){
    *msg = (char *)malloc(sizeMsg * sizeof(char) + 1);
    memset(*msg, baseChar, sizeMsg);
}

int saveResults(double rtt,double throughput, double packetRate,int resultIndex,double *results){
    results[resultIndex] = rtt;
    resultIndex++;
    results[resultIndex] = throughput;
    resultIndex++;
    results[resultIndex] = packetRate;
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



