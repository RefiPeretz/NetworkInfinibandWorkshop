//
// Created by fimka on 11/03/17.
//

#ifndef SOCKETEXCEPTION_HPP
#define SOCKETEXCEPTION_HPP

#include <string>

class SocketException
{
 public:
  SocketException ( std::string s ) : m_s ( s ) {};
  ~SocketException (){};

  std::string description() { return m_s; }

 private:

  std::string m_s;

};

#endif //EX1_SOCKETEXCEPTION_HPP
