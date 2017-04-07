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

using namespace std;

double measureAvgRTT(char* port,char* ip,int rttCount){
    int len;
    char message = 'w';
    char ack;
    struct timeval start, end;
    double t1, t2;
    Connector *connector = new Connector();
    Stream *stream = connector->connect(ip, atoi(port));
    if (stream) {
        t1 = 0.0;
        t2 = 0.0;
        if (gettimeofday(&start, NULL)) {
            printf("time failed\n");
            exit(1);
        }
    }
    for(int i = 0; i < rttCount;i++){
        stream->send(&message, sizeof(char));
        printf("sent - %c with sizeof %d\n", message, (int) sizeof(char));
        len = stream->receive(&ack, sizeof(char));
        if (gettimeofday(&end, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        printf("received - %c\n", ack);
    }

    t1 += start.tv_sec + (start.tv_usec / 1000000.0);
    t2 += end.tv_sec + (end.tv_usec / 1000000.0);

    //calculate and print mean rtt
    double rttAvg = ((t2 - t1) / 100);
    rttAvg = rttAvg/rttCount;

    delete stream;
    return rttAvg;

}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: %s <port> <ip>\n", argv[0]);
        exit(1);
    }

    int len;
    char message = 'w';
    char ack;
    struct timeval start, end;
    double t1, t2;
    Connector *connector = new Connector();
    Stream *stream = connector->connect(argv[2], atoi(argv[1]));
    if (stream) {
        t1 = 0.0;
        t2 = 0.0;
        if (gettimeofday(&start, NULL)) {
            printf("time failed\n");
            exit(1);
        }

        stream->send(&message, sizeof(char));
        printf("sent - %c with sizeof %d\n", message, (int) sizeof(char));
        len = stream->receive(&ack, sizeof(char));
        if (gettimeofday(&end, NULL)) {
            printf("time failed\n");
            exit(1);
        }
        t1 += start.tv_sec + (start.tv_usec / 1000000.0);
        t2 += end.tv_sec + (end.tv_usec / 1000000.0);
        printf("received - %c\n", ack);
        //calculate and print mean rtt
        delete stream;
    }
    double rtt = (t2 - t1) / 100;
    printf("RTT = %g ms\n", rtt);
    double rtt_d = 1000 / rtt;
    printf("Packet Rate = 1 / %g = %g byte / ms \n", rtt, rtt_d);
    auto patcketRate_str = std::to_string(rtt_d);
    double rttAvg = measureAvgRTT(argv[1],argv[2],1000);
    std::stringstream ss(stringstream::in | stringstream::out);
    ss << setprecision(5) << rttAvg << endl;
//    printf("str: %s\n", ss.str());
//    printf("avg: %g , normal: %g", rttAvg, rtt);
    ofstream myfile;
    myfile.open("SingleStreamResults.csv");
    myfile << "Results\n";
    myfile << "RTT,"+ ss.str() +" ms\n";
    myfile << "Packet Rate, " + patcketRate_str + " ms\n";
    myfile.close();
    exit(0);
}