//
// Created by fimka on 17/03/17.
//

//
// Created by fimka on 13/03/17.
//

#include <cstdlib>
#include <string>
#include <sys/time.h>
#include "Stream.hpp"
#include "Connector.hpp"
using namespace std;

int main(int argc, char **argv)
{
  if (argc != 4)
  {
	printf("usage: %s <base byte char>, <resize char>, <max message size to "
		"test>\n", argv[0]);
	exit(1);
  }

  printf("Params: <base byte char=%s>, <resize char=%s>, <max message size "
	  "to test= %d>\n", argv[1], argv[2], atoi(argv[3]));

  std::string message = argv[1];
  for (int msgByteSize = 1; msgByteSize <= 1024; msgByteSize = msgByteSize * 2)
  {

	int len;
	message.resize(msgByteSize, *argv[2]);
	char ack[msgByteSize];
	struct timeval start, end;
	double t1, t2;
	Connector *connector = new Connector();
	Stream *stream = connector->connect("localhost", 8080);
	if (stream)
	{
	  t1 = 0.0;
	  t2 = 0.0;
	  if (gettimeofday(&start, NULL))
	  {
		printf("time failed\n");
		exit(1);
	  }

	  stream->send(message.c_str(), message.size());
	  printf("sent - %s with sizeof %d bytes\n", message.c_str(), message.size());
	  len = stream->receive(ack, sizeof(ack));
	  ack[len] = 0;
	  if (gettimeofday(&end, NULL))
	  {
		printf("time failed\n");
		exit(1);
	  }
	  t1 += start.tv_sec + (start.tv_usec / 1000000.0);
	  t2 += end.tv_sec + (end.tv_usec / 1000000.0);
	  printf("received - %s\n", ack);
	  //calculate and print mean rtt
	  double rtt = (t2 - t1) / 100;
	  printf("RTT = %g ms\n", rtt);
	  printf("Packet Rate = 1 / %g = %g byte / ms \n", rtt, 1000 / rtt);

	  delete stream;
	}
  }

  exit(0);
}