//
// Created by fimka on 13/03/17.
//

#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <algorithm>
#include "Acceptor.hpp"
#include "Metrics.hpp"


void ConnectionHandler(Stream *stream);

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        printf("usage: server <port> [<ip>]\n");
        exit(1);
    }
    std::vector<std::thread> v;

    Stream *stream = NULL;
    Acceptor *acceptor = NULL;
    if (argc == 3) {
        acceptor = new Acceptor(atoi(argv[1]), argv[2]);
    } else {
        acceptor = new Acceptor(atoi(argv[1]));
    }
    acceptor->start();
    int threadNum = 0;
    while (1) {

        stream = acceptor->accept();
        stream->stream_id = threadNum % MAX_CORE;
        if (stream != NULL) {
            std::thread v1(ConnectionHandler, stream);
            v1.detach();
        }
        threadNum++;
    }
}

void ConnectionHandler(Stream *stream) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(stream->stream_id, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        perror("pthread_setaffinity_np");
        exit(-1);
    }
    int len;
    char line[MAX_PACKET_SIZE];
    while ((len = stream->receive(line, sizeof(line))) > 0) {
        line[len] = '\0';
        stream->send(line, len);
        printf("Recevied - %d Bytes\n", len);

    }
    delete stream;
}

