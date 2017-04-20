//
// Created by fimka on 13/03/17.
//

#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>
#include "Stream.hpp"

Stream::Stream(int sd, struct sockaddr_in* address) : m_sd(sd) {
  char ip[50];
  inet_ntop(PF_INET, (struct in_addr*)&(address->sin_addr.s_addr), ip, sizeof(ip)-1);
  m_peerIP = ip;
  m_peerPort = ntohs(address->sin_port);
}

Stream::~Stream()
{
  close(m_sd);
}

ssize_t Stream::send(const char *buffer, int len)
{
    //printf("send: %s , in size: %d\n",buffer,len);
  return write(m_sd, buffer, len);
}

ssize_t Stream::receive(char *buffer, int len, int timeout)
{
  if (timeout <= 0) return read(m_sd, buffer, len);

  if (waitForReadEvent(timeout) == true)
  {
	return read(m_sd, buffer, len);
  }
  return connectionTimedOut;

}



string Stream::getPeerIP()
{
  return m_peerIP;
}

int Stream::getPeerPort()
{
  return m_peerPort;
}

bool Stream::waitForReadEvent(int timeout)
{
  int result = 0;
  fd_set sdset;
  struct timeval tv;

  tv.tv_sec = timeout;
  tv.tv_usec = 0;
  FD_ZERO(&sdset);
  FD_SET(m_sd, &sdset);
  if (result = select(m_sd+1, &sdset, NULL, NULL, &tv) > 0)
  {
	return true;
  }
  printf("False: %d\n",result);
  return false;
}