//
// Created by fimka on 18/04/17.
//

#ifndef EX1V2_SIMPLEIBCLIENT_HPP
#define EX1V2_SIMPLEIBCLIENT_HPP

#include <thread>
#include <vector>
#include <iostream>
#include <mutex>
#include <algorithm>
#include <infiniband/verbs.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <stdlib.h>
#include "CommonIBUtilFuncs.hpp"


unsigned int _numThreads = 1;
std::vector<std::thread> _threadsVec;
struct ibv_device **dev_list;
struct ibv_device *ib_dev;
int size = 4096;
int ib_port = 1;
int port = 18515;

int rx_depth = 500; //Used to note minimum number of entries for CQ
int use_event = 0;
int gidx = -1;
char gid[33];


int peerNum = 1;
char messageChar = 'w'; //Classic 'w'. The famous w.
int                      sl = 0;
enum ibv_mtu		 mtu = IBV_MTU_1024;


std::vector<serverInfo> _localQPinfo;
std::vector<serverInfo> _remoteQPinfo;



int setThreadAffinity(int threadId);

int setupIB()
{

  //get the device list on the client
  std::cout<<"get device list"<<std::endl;
  dev_list = ibv_get_device_list(NULL);
  if (!dev_list)
  {
	perror("Failed to get IB devices list");
	return 1;
  }

  //Get device from list.
  ib_dev = *dev_list;
  if (!ib_dev)
  {
	fprintf(stderr, "No IB devices found\n");
	return 1;
  }
  std::cout<<"got device from list "<< ib_dev->dev_name<<std::endl;

  //Init's all the needed structures for the Connection and returns "ctx" Holds the whole Connection data
  //Hold locally in connection and globally under "ctx" - pay attantion when making changes and using.
  connection = init_connection(ib_dev, size, rx_depth, ib_port,
	  use_event,
	  1, peerNum, messageChar);
  if (connection == nullptr)
  {
	return 1;
  }


  //Init our vectors that hold information on local and remote QP's
  std::vector<serverInfo> _localQPinfo = std::vector<serverInfo>(peerNum);
  std::vector<serverInfo> _remoteQPinfo = std::vector<serverInfo>(peerNum);


  if (connection->portinfo.link_layer == IBV_LINK_LAYER_INFINIBAND &&
	  !connection->portinfo.lid)
  {
	fprintf(stderr, "Couldn't get local LID\n");
	return 1;
  }


  //    /*
  //     * prepares Connection to get the given amount of packets
  //     */
  //    int routs = postRecvWorkReq(connection, (*connection).rx_depth);
  //    if (routs < (*connection).rx_depth)
  //    {
  //        fprintf(stderr, "Couldn't post receive (%d)\n", routs);
  //        return 1;
  //    }
  //
  //    if (use_event)
  //    {
  //        if (ibv_req_notify_cq(ctx.cq, 0))
  //        {
  //            fprintf(stderr, "Couldn't request CQ notification\n");
  //            return 1;
  //        }
  //    }

  if (pp_get_port_info((*connection).context, ib_port,
	  &(*connection).portinfo))
  {
	fprintf(stderr, "Couldn't get port info\n");
	return 1;
  }

  // Init local QP queue holder with information
  unsigned int j = 0;
  std::for_each(_localQPinfo.begin(), _localQPinfo.end(),
	  [&](serverInfo &localQP)
	  {
		localQP.lid = connection->portinfo.lid;

		if (gidx >= 0)
		{
		  if (ibv_query_gid(connection->context, ib_port, gidx,
			  &localQP.gid))
		  {
			fprintf(stderr,
				"Could not get local gid for gid index %d\n",
				gidx);
			return 1;
		  }
		} else
		{
		  memset(&localQP.gid, 0, sizeof localQP.gid);
		}

		  localQP.qpn = (*connection->qp.at(j)).qp_num;
		  localQP.psn = lrand48() & 0xffffff;
		inet_ntop(AF_INET6, &localQP.gid, gid, sizeof gid);
		printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
			localQP.lid, localQP.qpn, localQP.psn, gid);
		j++;
	  });

    std::cout << "Setting the connection to the clients" <<std::endl;


  //Exchange information on target server
  if (connectRemoteToClient(connection, ib_port, mtu, port, sl, gidx, _localQPinfo, _remoteQPinfo ))  {
	std::cerr
		<< "cannot init connection to remote server and retrieve remote QP"
		<< std::endl;
	return 1;
  }


};
void threadFunc(int threadId);

int main(int argc, char *argv[])
{
  int ret = 0;

  //TODO: check args

  if (argc < 2)
  {
	std::cerr
		<< "Usage: <client threads num, Default is 1>"
		<< std::endl;
	return 1;
  }


  std::cout << "get Socket number: " << *argv[1] << std::endl;
  peerNum = atoi(argv[1]);

  //TODO: Test peerNum isn't bigger than max ThreadNum which is the
  // logical CPU count

  if (setupIB())
  {
	std::cerr << "IB Client setup failed" << std::endl;
	return 1;
  }

  for (unsigned int i = 0; i < _numThreads; i++)
  {
	_threadsVec[i] = std::thread(&threadFunc, i);
  }


  std::for_each(_threadsVec.begin(), _threadsVec.end(), [](std::thread &t)
  {
	t.join();
  });

  return ret;
}



void threadFunc(int threadId)
{
  std::cout<<"running thread " << threadId << std::endl;
  int ret = 0, i = 0, n = 0;
  int num_concurr_msgs = 1;
  int msg_size = 4096;
  int num_wc = 20;
  bool start_sending = false;
  bool stop = false;
  int                      routs;

  pthread_t self;
  cpu_set_t cpuset;

  struct ibv_qp *qp = connection->qp[threadId];
  struct ibv_cq *cq = connection->cq;
  struct ibv_wc *wc = NULL;
  uint32_t lkey = connection->mr->lkey;
  char *buf_ptr = (char *) connection->buf;
  int buf_offset = 0;
  size_t buf_size = connection->size;

  struct timeval start, end;
  long ops_count = 0;
  double duration = 0.0;
  double throughput = 0.0;


  if (setThreadAffinity(threadId))
  {
	return;
  }



  if (gettimeofday(&start, NULL))
  {
	std::cerr << "gettimeofday";
	return;
  }

  int iters = 1000;
  int rcnt, scnt = 0;
  int                      num_cq_events = 0;

  while (rcnt < iters || scnt < iters)
  {
	if (use_event)
	{
	  struct ibv_cq *ev_cq;
	  void *ev_ctx;

	  if (ibv_get_cq_event(connection->channel, &ev_cq, &ev_ctx))
	  {
		fprintf(stderr, "Failed to get cq_event\n");
		std::terminate(); //TODO crash thread
	  }

	  ++num_cq_events;

	  if (ev_cq != connection->cq)
	  {
		fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
		std::terminate(); //TODO crash thread
	  }

	  if (ibv_req_notify_cq(connection->cq, 0))
	  {
		fprintf(stderr, "Couldn't request CQ notification\n");
		std::terminate(); //TODO crash thread
	  }
	}

	{
	  struct ibv_wc wc[2];
	  int ne, i;

	  do
	  {
		ne = ibv_poll_cq(connection->cq, 2, wc);
		if (ne < 0)
		{
		  fprintf(stderr, "poll CQ failed %d\n", ne);
		  std::terminate(); //TODO crash thread
		}

	  } while (!use_event && ne < 1);

	  for (i = 0; i < ne; ++i)
	  {
		if (wc[i].status != IBV_WC_SUCCESS)
		{
		  fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
			  ibv_wc_status_str(wc[i].status), wc[i].status,
			  (int) wc[i].wr_id);
		  std::terminate(); //TODO crash thread
		}

		switch ((int) wc[i].wr_id)
		{
		  case SEND_WRID:
			++scnt;
			break;

		  case RECV_WRID:
			if (--routs <= 1) //TODO: not sure it was init
			{
			  routs +=
				  postRecvWorkReq(connection, connection->rx_depth -
					  routs, threadId);
			  if (routs < connection->rx_depth)
			  {
				fprintf(stderr, "Couldn't post receive (%d)\n",
					routs);
				std::terminate(); //TODO crash thread
			  }
			}

			++rcnt;
			break;

		  default:
			fprintf(stderr, "Completion for unknown wr_id %d\n",
				(int) wc[i].wr_id);
			std::terminate(); //TODO crash thread
		}

		connection->pending &= ~(int) wc[i].wr_id; //check if we
		// shouldnt
		// vecotrize pending
		if (scnt < iters && !connection->pending)
		{
		  if (postSendWorkReq(connection, threadId))
		  {
			fprintf(stderr, "Couldn't post send\n");
			std::terminate(); //TODO crash thread
		  }
		  connection->pending = RECV_WRID | SEND_WRID;
		}
	  }
	}
  }

  if (gettimeofday(&end, NULL))
  {
	perror("gettimeofday");
	std::terminate(); //TODO crash thread
  }

  {
	float usec = (end.tv_sec - start.tv_sec) * 1000000 +
		(end.tv_usec - start.tv_usec);
	long long bytes = (long long) size * iters * 2;

	printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n", bytes,
		usec / 1000000., bytes * 8. / usec);
	printf("%d iters in %.2f seconds = %.2f usec/iter\n", iters,
		usec / 1000000., usec / iters);
  }

  ibv_ack_cq_events(connection->cq, num_cq_events);


}


int setThreadAffinity(int threadId)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(threadId, &cpuset);
  int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  if (rc != 0)
  {
	std::cerr << "Error calling pthread_setaffinity_np: " << rc
			  << " for thread with id: " << threadId << "\n";
	return 1;
  }
  return 0;
}

int initThreads()
{


  return 0;
}

#endif //EX1V2_SIMPLEIBCLIENT_HPP
