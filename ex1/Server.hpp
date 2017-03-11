//
// Created by fimka on 11/03/17.
//

#ifndef EX1_SERVER_HPP
#define EX1_SERVER_HPP
#include "Socket.h"


class ServerSocket : private Socket
{
 public:

  ServerSocket ( int port );
  ServerSocket (){};
  virtual ~ServerSocket();

  const ServerSocket& operator << ( const std::string& ) const;
  const ServerSocket& operator >> ( std::string& ) const;

  void accept ( ServerSocket& );

};
#endif //EX1_SERVER_HPP
