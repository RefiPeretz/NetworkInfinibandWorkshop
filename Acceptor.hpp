//
// Created by fimka on 13/03/17.
//

#ifndef EX1V2_ACCEPT_HPP
#define EX1V2_ACCEPT_HPP

#include <iostream>
#include "Stream.hpp"

using std::string;

class Acceptor
{
  int    m_lsd; //Socket descriptor
  string m_address;
  int    m_port;
  bool   m_listening;

 public:
  Acceptor(int port, const char* address="");
  ~Acceptor();

  int     start();
  Stream* accept();

 private:
  Acceptor() {}
};



#endif //EX1V2_ACCEPT_HPP
