//
// Created by fimka on 13/03/17.
//

#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <algorithm>
#include "Acceptor.hpp"

//void join_all(std::vector<std::thread>& v);
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
    if (acceptor->start() == 0) {
        while (1) {
            stream = acceptor->accept();
            if (stream != NULL) {
                std::thread v1(ConnectionHandler, stream);
                v1.detach();
            }
        }
    }
    perror("Could not start the server");
    exit(-1);
}

void ConnectionHandler(Stream *stream) {
    int len;
    char line[256];
    while ((len = stream->receive(line, sizeof(line), 1)) > 0) {
        line[len] = '\0';
        stream->send(line, len);
        printf("received - %s\n", line);

    }
    delete stream;
}

