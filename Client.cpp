//
// Created by fimka on 13/03/17.
//

#include <cstdlib>
#include <string>
#include "Stream.hpp"
#include "Connector.hpp"
using namespace std;

int main(int argc, char** argv)
{
  if (argc != 3) {
	printf("usage: %s <port> <ip>\n", argv[0]);
	exit(1);
  }

  int len;
  char message = 'w';
  char ack;
  struct timeval start,end;
  Connector* connector = new Connector();
  Stream* stream = connector->connect(argv[2], atoi(argv[1]));
  if(stream){
	stream->send(&message, sizeof(char));
	printf("sent - %c with sizeof %d\n", message, (int) sizeof(char));
	len = stream->receive(&ack, sizeof(char));
	//line[len] = NULL;
	printf("received - %c\n", ack);
	delete stream;
  }

  exit(0);
}