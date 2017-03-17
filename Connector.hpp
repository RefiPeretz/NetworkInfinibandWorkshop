//
// Created by fimka on 13/03/17.
//

#ifndef EX1V2_CONNECTOR_HPP
#define EX1V2_CONNECTOR_HPP

#include <netinet/in.h>
#include "Stream.hpp"
class Connector
{
 public:
  Stream* connect(const char* server, int port);
  Stream* connect(const char* server, int port, int timeout);


 private:
  int resolveHost(const char* host, struct in_addr* addr);
};


#endif //EX1V2_CONNECTOR_HPP
