//
// Created by fimka on 13/03/17.
//

#include <cstdlib>
#include <string>
#include <sys/time.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "Stream.hpp"
#include "Connector.hpp"
#include <sstream>
#include "Metrics.hpp"

using namespace std;


int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: %s <port> <ip>\n", argv[0]);
        exit(1);
    }
    warmUpServer(atoi(argv[1]));

    int numMsgs = 10;
    int len;
    char* message;
    char ack;
    struct timeval start, end;
    double t1, t2;
    for (int msgSize = 1; msgSize <= 1024; msgSize = msgSize * 2){
        t1 = 0.0;
        t2 = 0.0;
        createMsg(msgSize,'w',&message);
        message[msgSize] = '\0';
        if (gettimeofday(&start, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        for(int i = 0 ; i < numMsgs; i++){
            Connector *connector = new Connector();
            Stream *stream = connector->connect(argv[2], atoi(argv[1]));
            if (stream) {

                stream->send(message, msgSize);
                printf("sent - %s with sizeof %d\n", message, msgSize);
                len = stream->receive(&ack, sizeof(char));
                printf("received - %c\n", ack);
                //calculate and print mean rtt
                delete stream;

            }


        }
        if (gettimeofday(&end, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        t1 += start.tv_sec + (start.tv_usec / 1000000.0);
        t2 += end.tv_sec + (end.tv_usec / 1000000.0);
        double rtt = calcAverageRTT(numMsgs, (t2-t1) / 100);
        double packetRate = calcAveragePacketRate(numMsgs,(t2-t1) / 100);
        double throughput = calcAverageThroughput(numMsgs,msgSize,(t2-t1) / 100);
        printf("avgRTT: %g\n", rtt);
        printf("avgPacketRate: %g\n", packetRate);
        printf("avgThroughput: %g\n", throughput);
        free(message);

    }

    //printf("Total time: %g\n", rtt);
//    printf("RTT = %g ms\n", rtt);
//    double rtt_d = 1000 / rtt;
//    printf("Packet Rate = 1 / %g = %g byte / ms \n", rtt, rtt_d);
//    auto patcketRate_str = std::to_string(rtt_d);
//    double rttAvg = measureAvgRTT(argv[1],argv[2],1000);
//    std::stringstream ss(stringstream::in | stringstream::out);
//    ss << setprecision(5) << rttAvg << endl;
////    printf("str: %s\n", ss.str());
////    printf("avg: %g , normal: %g", rttAvg, rtt);
//    ofstream myfile;
//    myfile.open("SingleStreamResults.csv");
//    myfile << "Results\n";
//    myfile << "RTT,"+ ss.str() +" ms\n";
//    myfile << "Packet Rate, " + patcketRate_str + " ms\n";
//    myfile.close();
    exit(0);
}