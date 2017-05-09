//
// Created by fimka on 13/03/17.
//

#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>    //close
#include "Metrics.hpp"
#include "Acceptor.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>


int sendVerifier(int s, char *buf, int len){
  int total = 0;        // how many bytes we've sent
  int bytesleft = len; // how many we have left to send
  int n;

  std::cout << "len to be sent: " << len<<std::endl;

  while(total < len) {
	n = send(s, buf+total, bytesleft, 0);
	if (n == -1) { break; }
	total += n;
	bytesleft -= n;
  }

  len = total; // return number actually sent here
    std::cout << "total sent: " << total<<std::endl;
  return n==-1?-1:0; // return -1 on failure, 0 on success
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s <port>\n", argv[0]);
        exit(1);
    }
    int serverPort = atoi(argv[1]);
    int  addrlen, new_socket, clients[MAX_CLIENTS],
            status, i, bytesRead, cur_sd;

    Stream *stream = NULL;
    char buffer[MAX_PACKET_SIZE];  //data buffer of 1K

    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = 0;
    }

    Acceptor *acceptor = new Acceptor(serverPort, SERVER_ADDRESS);
    struct sockaddr_in address = acceptor->start();


    addrlen = sizeof(address);
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
                                     (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
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
                if ((bytesRead = read(cur_sd, buffer, MAX_PACKET_SIZE)) == 0) {
                    close(cur_sd);
                    clients[i] = 0;
                }
                else if(bytesRead == -1)
				{
					perror("Failed reading from socket\n");
				  return 1;
				}else {
                    buffer[bytesRead] = '\0';
                    //send(cur_sd, buffer, strlen(buffer), 0);
				  if(sendVerifier(cur_sd, buffer, strlen(buffer)) < 0){
					std::cerr<<"Full send failed - sent only partial"<<std::endl;
					return 1;
				  };
                }
            }
        }
    }

    return 0;
}