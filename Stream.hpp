//
// Created by fimka on 13/03/17.
//

#ifndef EX1V2_STREAM_HPP
#define EX1V2_STREAM_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>

using namespace std;

class Stream
{

  string  m_peerIP;
  int     m_peerPort;

 public:
	int stream_id = 0;
  int     m_sd;
  friend class Acceptor;
  friend class Connect;

  enum {
	connectionClosed = 0,
	connectionReset = -1,
	connectionTimedOut = -2
  };

  ~Stream();

  ssize_t send(const char *buffer, int len);
  ssize_t receive(char *buffer, size_t len, int timeout = 0);

  string getPeerIP();
  int getPeerPort();

  Stream(int sd, struct sockaddr_in* address);
 private:
  bool waitForReadEvent(int timeout);

  Stream();
  Stream(const Stream& stream);
};


#endif //EX1V2_STREAM_HPP
