//
// Created by fimka on 13/03/17.
//

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "Acceptor.hpp"

Acceptor::Acceptor(int port, const char *address)
        : m_lsd(0), m_port(port), m_address(address), m_listening(false) {}

Acceptor::~Acceptor() {
    if (m_lsd > 0) {
        close(m_lsd);
    }
}

void Acceptor::reinit() {
    FD_ZERO(&fds);

    FD_SET(m_lsd, &fds);

}

struct sockaddr_in Acceptor::start() {
    struct sockaddr_in address;
    if (m_listening == true) {
        return address;
    }

    m_lsd = socket(PF_INET, SOCK_STREAM, 0);

    memset(&address, 0, sizeof(address));
    address.sin_family = PF_INET;
    address.sin_port = htons(m_port);
    if (m_address.size() > 0) {
        inet_pton(PF_INET, m_address.c_str(), &(address.sin_addr));
    } else {
        address.sin_addr.s_addr = INADDR_ANY;
    }

    int optval = 1;
    setsockopt(m_lsd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

    int result = bind(m_lsd, (struct sockaddr *) &address, sizeof(address));
    if (result != 0) {
        perror("bind() failed");
        exit(-1);
    }
    result = listen(m_lsd, 10);
    if (result != 0) {
        perror("listen() failed");
        exit(-1);
    }
    m_listening = true;
    return address;
}


Stream *Acceptor::accept() {
    struct sockaddr_in address;
    if (!m_listening) {
        return NULL;
    }
    socklen_t len = sizeof(address);
    memset(&address, 0, sizeof(address));
    int sd = ::accept(m_lsd, (struct sockaddr *) &address, &len);
    if (sd < 0) {
        perror("accept() failed");
        return NULL;
    }
    //std::cout<<"confd: "<<sd<<endl;
    return new Stream(sd, &address);
}