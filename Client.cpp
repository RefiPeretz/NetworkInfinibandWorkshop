//
// Created by fimka on 13/03/17.
//

#include <cstdlib>
#include <string>
#include <sys/time.h>
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
  double t1,t2;
  Connector* connector = new Connector();
  Stream* stream = connector->connect(argv[2], atoi(argv[1]));
  if(stream){
	t1=0.0; t2=0.0;
	if(gettimeofday(&start,NULL)) {
	  printf("time failed\n");
	  exit(1);
	}

	stream->send(&message, sizeof(char));
	printf("sent - %c with sizeof %d\n", message, (int) sizeof(char));
	len = stream->receive(&ack, sizeof(char));
	if(gettimeofday(&end,NULL)) {
	  printf("time failed\n");
	  exit(1);
	}
	t1+=start.tv_sec+(start.tv_usec/1000000.0);
	t2+=end.tv_sec+(end.tv_usec/1000000.0);
	printf("received - %c\n", ack);
	//calculate and print mean rtt
	printf("RTT = %g ms\n",(t2-t1)/100);
	delete stream;
  }

  exit(0);
}