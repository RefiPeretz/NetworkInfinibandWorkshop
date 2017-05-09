//
// Created by fimka on 13/03/17.
//

#include <cstdlib>
#include <string>
#include <sys/time.h>
#include <iostream>
#include "Stream.hpp"
#include "Connector.hpp"
#include "Metrics.hpp"


using namespace std;

int sendVerifier(int s, char *buf, int len)
{
  int total = 0;        // how many bytes we've sent
  int bytesleft = len; // how many we have left to send
  int n;

  while (total < len)
  {
	n = send(s, buf + total, bytesleft, 0);
	if (n == -1)
	{
	  break;
	}
	total += n;
	bytesleft -= n;
  }

  len = total; // return number actually sent here

  return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

int main(int argc, char **argv)
{
  if (argc != 4)
  {
	printf("usage: %s <port> <number of msgs> <server>\n", argv[0]);
	exit(1);
  }
  int serverPort = atoi(argv[1]);
  warmUpServer(serverPort, DEFAULT_NUMBER_OF_MSGS, argv[3]);
  int numMsgs = atoi(argv[2]);
  int len;
  int bytesRead;
  char ack[MAX_MSG_SIZE];
  struct timeval start, end;
  double t1, t2;
  double results[3000] = {0.0};
  int resultIndex = 0;
  int curMsgSize;
  for (int msgSize = MIN_MSG_SIZE; msgSize <= MAX_MSG_SIZE; msgSize = msgSize * 2)
  {
	for (int socketNum = 1; socketNum < MAX_CLIENTS; socketNum++)
	{
	  printf("=====Run on %d size with %d =====\n", msgSize, socketNum);
	  t1 = 0.0;
	  t2 = 0.0;
	  int msgSizes[socketNum] = {0};
	  //Prepearedata
	  char *msgs[socketNum];
	  if (msgSize < socketNum)
	  {
		createMsg(msgSize, 'w', &msgs[0]);
		msgs[0][msgSize] = '\0';
		msgSizes[0] = msgSize;
		for (int i = 1; i < socketNum; i++)
		{
		  msgSizes[i] = 0;
		}

	  }
	  else
	  {
		for (int i = 1; i < socketNum; i++)
		{
		  createMsg(msgSize / socketNum, 'w', &msgs[i]);
		  msgs[i][msgSize / socketNum] = '\0';
		  msgSizes[i] = msgSize / socketNum;
		}
		createMsg((msgSize / socketNum) + (msgSize % socketNum), 'w', &msgs[0]);
		msgs[0][(msgSize / socketNum) + (msgSize % socketNum)] = '\0';
		msgSizes[0] = (msgSize / socketNum) + (msgSize % socketNum);

	  }

	  Connector connector;
	  Stream *streams[socketNum];
	  if (gettimeofday(&start, NULL))
	  {
		printf("time failed\n");
		exit(1);
	  }
	  for (int stream = 0; stream < socketNum; stream++)
	  {
		streams[stream] = connector.connect(argv[3], serverPort);
		if (streams[stream] && msgSizes[stream] > 0)
		{
		  sendVerifier(streams[stream]->m_sd, msgs[stream], msgSizes[stream]);
		}
	  }

	  int max_sd = 0;
	  int sd = 0;
	  fd_set readfds;
	  int ret;
	  for (int i = 0; i < numMsgs; i++)
	  {
		int ackedPeers = 0;
		int tempMsgSizes[socketNum] = {0};
		while (ackedPeers != socketNum)
		{
		  FD_ZERO(&readfds);
		  for (int stream = 0; stream < socketNum; stream++)
		  {
			FD_SET(streams[stream]->m_sd, &readfds);
			max_sd = (max_sd > streams[stream]->m_sd) ? max_sd : streams[stream]->m_sd;
		  }
		  ret = select(max_sd + 1, &readfds, NULL, NULL, NULL);
		  if (ret < 0)
		  {
			printf("select failed\n ");
			return -1;
		  }
		  for (int stream = 0; stream < socketNum; stream++)
		  {
			bytesRead = 0;
			if (streams[stream] && msgSizes[stream] > 0)
			{
			  sd = streams[stream] ->m_sd;
			  if (FD_ISSET(sd, &readfds)) {
				bytesRead = streams[stream]->receive(ack, MAX_MSG_SIZE);
				if(bytesRead >= 0){
				  tempMsgSizes[stream] += bytesRead;
				  if(bytesRead ==0 || tempMsgSizes[stream] == msgSizes[stream]){
					ackedPeers++ ;
				  }
				} else {
				  std::cerr<<"Error in read" << std::endl;
				  return 1;
				}
			}}
		  }
		}

	  }


	  if (gettimeofday(&end, NULL))
	  {
		printf("time failed\n");
		exit(1);
	  }
	  double totalTime = timeDifference(start, end);
	  double rtt = calcAverageRTT(1, socketNum * numMsgs, totalTime);
	  double packetRate = calcAveragePacketRate(socketNum * numMsgs, totalTime);
	  double throughput = calcAverageThroughput(socketNum * numMsgs, msgSize, totalTime);
	  double numOfSockets = 1;
	  printf("avgRTT: %g\n", rtt);
	  printf("avgPacketRate: %g\n", packetRate);
	  printf("avgThroughput: %g\n", throughput);
	  resultIndex =
		  saveResults(rtt, throughput, packetRate, resultIndex, results, socketNum, msgSize, numMsgs * socketNum);
	  for (int stream = 0; stream < socketNum; stream++)
	  {
		delete (streams[stream]);
		if (msgSizes[stream] != 0)
		{
		  free(msgs[stream]);
		}

	  }

	}

  }
  createResultFile(3000, "MultiStreamResults.csv", results);

}
