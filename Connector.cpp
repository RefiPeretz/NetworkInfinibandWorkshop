//
// Created by fimka on 13/03/17.
//

#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstdio>
#include <fcntl.h>
#include <cerrno>
#include "Stream.hpp"
#include "Connector.hpp"

Stream *Connector::connect(const char *server, int port)
{

  struct sockaddr_in address;

  memset (&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  if (resolveHost(server, &(address.sin_addr)) != 0 ) {
	inet_pton(PF_INET, server, &(address.sin_addr));
  }

  // Create and connect the socket, bail if we fail in either case
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  if (sd < 0) {
	perror("socket() failed");
	return NULL;
  }
  if (::connect(sd, (struct sockaddr*)&address, sizeof(address)) != 0) {
	perror("connect() failed");
	close(sd);
	return NULL;
  }
  return new Stream(sd, &address);
}

Stream *Connector::connect(const char* server, int port, int timeout)
{
  if (timeout == 0) return connect(server, port);

  struct sockaddr_in address;

  memset (&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  if (resolveHost(server, &(address.sin_addr)) != 0 ) {
	inet_pton(PF_INET, server, &(address.sin_addr));
  }

  long arg;
  fd_set sdset;
  struct timeval tv;
  socklen_t len;
  int result = -1, valopt, sd = socket(AF_INET, SOCK_STREAM, 0);

  // Bail if we fail to create the socket
  if (sd < 0) {
	perror("socket() failed");
	return NULL;
  }

  // Set socket to non-blocking
  arg = fcntl(sd, F_GETFL, NULL);
  arg |= O_NONBLOCK;
  fcntl(sd, F_SETFL, arg);

  // Connect with time limit
  std::string message;
  if ((result = ::connect(sd, (struct sockaddr *)&address, sizeof(address))) < 0)
  {
	if (errno == EINPROGRESS)
	{
	  tv.tv_sec = timeout;
	  tv.tv_usec = 0;
	  FD_ZERO(&sdset);
	  FD_SET(sd, &sdset);
	  int s = -1;
	  do {
		s = select(sd + 1, NULL, &sdset, NULL, &tv);
	  } while (s == -1 && errno == EINTR);
	  if (s > 0)
	  {
		len = sizeof(int);
		getsockopt(sd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &len);
		if (valopt) {
		  fprintf(stderr, "connect() error %d - %s\n", valopt, strerror(valopt));
		}
		  // Connection established
		else result = 0;
	  }
	  else fprintf(stderr, "connect() timed out\n");
	}
	else fprintf(stderr, "connect() error %d - %s\n", errno, strerror(errno));
  }

  // Return socket to blocking mode
  arg = fcntl(sd, F_GETFL, NULL);
  arg &= (~O_NONBLOCK);
  fcntl(sd, F_SETFL, arg);

  // Create stream object if connected
  if (result == -1) return NULL;
  return new Stream(sd, &address);
}



int Connector::resolveHost(const char *host, struct in_addr *addr)
{
  struct addrinfo *res;

  int result = getaddrinfo (host, NULL, NULL, &res);
  if (result == 0) {
	memcpy(addr, &((struct sockaddr_in *) res->ai_addr)->sin_addr,
		sizeof(struct in_addr));
	freeaddrinfo(res);
  }
  return result;
}
