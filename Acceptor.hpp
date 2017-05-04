//
// Created by fimka on 13/03/17.
//

#ifndef EX1V2_ACCEPT_HPP
#define EX1V2_ACCEPT_HPP

#include <iostream>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include "Stream.hpp"

using std::string;

class Acceptor {
    //Socket descriptor
    string m_address;
    int m_port;
    bool m_listening;

public:
    int max_sd;
    fd_set fds;
    int m_lsd;

    Acceptor(int port, const char *address = "");

    ~Acceptor();

    void reinit();

    struct sockaddr_in start();

    Stream *accept();

private:
    Acceptor() {}
};


#endif //EX1V2_ACCEPT_HPP
