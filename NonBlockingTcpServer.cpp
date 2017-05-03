//
// Created by fimka on 13/03/17.
//

#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include "Metrics.hpp"
#include "Acceptor.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


int main(int argc, char *argv[]) {
    int  addrlen, new_socket, clients[10],
            status, i, bytesRead, cur_sd;


    char buffer[MAX_PACKET_SIZE];  //data buffer of 1K

    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = 0;
    }

    Acceptor *acceptor = new Acceptor(SERVER_PORT, SERVER_ADDRESS);
    acceptor->start();


    addrlen = sizeof(acceptor->address);
    printf("MultiStream ser"
                   "ver is up\n");
    while (1) {
        acceptor->reinit();
        acceptor->max_sd = acceptor->m_lsd;

        for (i = 0; i < MAX_CLIENTS; i++) {
            //socket descriptor
            cur_sd = clients[i];
            if (cur_sd > 0)
                FD_SET(cur_sd, &acceptor->fds);

            if (cur_sd > acceptor->max_sd)
                acceptor->max_sd = cur_sd;
        }

        status = select(acceptor->max_sd + 1, &acceptor->fds, NULL, NULL, NULL);

        if ((status < 0) && (errno != EINTR)) {
            printf("select error");
        }

        if (FD_ISSET(acceptor->m_lsd, &acceptor->fds)) {
            if ((new_socket = accept(acceptor->m_lsd,
                                     (struct sockaddr *) &acceptor->address, (socklen_t *) &addrlen)) < 0) {
                perror("accept");
                exit(-1);
            }

            for (i = 0; i < MAX_CLIENTS; i++) {
                //if position is empty
                if (clients[i] == 0) {
                    clients[i] = new_socket;
                    break;
                }
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            cur_sd = clients[i];
            if (FD_ISSET(cur_sd, &acceptor->fds)) {
                if ((bytesRead = read(cur_sd, buffer, 1024)) == 0) {
                    printf("received - %s\n", buffer);
                    close(cur_sd);
                    clients[i] = 0;
                }
                else {
                    buffer[bytesRead] = '\0';
                    send(cur_sd, buffer, strlen(buffer), 0);
                }
            }
        }
    }

    return 0;
}