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
#include "CommonIBUtilFuncs.hpp"




unsigned int _numThreads = 1;
std::vector<std::thread> _threadsVec;
struct ibv_device **dev_list;
struct ibv_device *ib_dev;
int size = 4096;
int ib_port = 1;

int rx_depth = 500; //Used to note minimum number of enteries for CQ
int use_event = 0;




char *servername = NULL;
int peerNum = 1;
char messageChar = 'w'; //Classic 'w'. The famous w.

int setupIB()
{
  //get the device list on the client
  dev_list = ibv_get_device_list(NULL);
  if (!dev_list) {
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

  //Init's all the needed structures for the connection and returns "ctx" Holds the whole connection data
  ctx = *init_connection(ib_dev, size, rx_depth, ib_port, use_event, !servername, peerNum, messageChar);
  //if (!ctx)
  //{
	//return 1;
  //}//TODO: Find way to check for null!!

  /*
	* prepares connection to get the given amount of packets
	*/
  int routs = postRecvWorkReq(ctx, ctx->rx_depth);
  if (routs < ctx->rx_depth)
  {
	fprintf(stderr, "Couldn't post receive (%d)\n", routs);
	return 1;
  }

  if (use_event)
	if (ibv_req_notify_cq(ctx->cq, 0)) {
	  fprintf(stderr, "Couldn't request CQ notification\n");
	  return 1;
	}


  if (pp_get_port_info(ctx->context, ib_port, &ctx->portinfo)) {
	fprintf(stderr, "Couldn't get port info\n");
	return 1;
  }

  my_dest.lid = ctx->portinfo.lid;
  if (ctx->portinfo.link_layer == IBV_LINK_LAYER_INFINIBAND && !my_dest.lid) {
	fprintf(stderr, "Couldn't get local LID\n");
	return 1;
  }

  if (gidx >= 0) {
	if (ibv_query_gid(ctx->context, ib_port, gidx, &my_dest.gid)) {
	  fprintf(stderr, "Could not get local gid for gid index %d\n", gidx);
	  return 1;
	}
  } else
	memset(&my_dest.gid, 0, sizeof my_dest.gid);

  my_dest.qpn = ctx->qp->qp_num;
  my_dest.psn = lrand48() & 0xffffff;
  inet_ntop(AF_INET6, &my_dest.gid, gid, sizeof gid);
  printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
	  my_dest.lid, my_dest.qpn, my_dest.psn, gid);



  //Exchange information on target server
  rem_dest = connectClientToRemote(servername, port, &my_dest);

  if (!rem_dest)
  {
	return 1;
  }

  inet_ntop(AF_INET6, &rem_dest->gid, gid, sizeof gid);
  printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
	  rem_dest->lid,
	  rem_dest->qpn,
	  rem_dest->psn,
	  gid);


  if (prepIbDeviceToConnect(ctx, ib_port, my_dest.psn, mtu, sl, rem_dest, gidx))
  {
	return 1;
  }
};

int main(int argc, char*argv[])
  {
	int ret = 0;

	//TODO: check args

	if(argc < 4)
	{
	  std::cerr << "Usage: <server address> <Char to send> <client threads num, Default is 1>"<<std::endl;
	  return 1;
	}

	servername = argv[1];
	peerNum = *argv[3];
	messageChar = *argv[2];

	if(setupIB())
	{
	  std::cerr << "IB Client setup failed"<<std::endl;
	  return 1;
	}

	for(unsigned int i=0; i < _numThreads;i++){
	  _threadsVec[i] = std::thread(&threadFunc, i);
	}


	std::for_each(_threadsVec.begin(), _threadsVec.end(),[](std::thread& t){t.join();} );

	return ret;
  }




  void threadFunc(int threadId){
	int         ret             = 0, i = 0, n = 0;
	int         num_concurr_msgs= 1;//config_info.num_concurr_msgs;
	int         msg_size        = 4096;//config_info.msg_size;
	int		num_wc		= 20;
	bool	start_sending   = false;
	bool        stop            = false;

	pthread_t   self;
	cpu_set_t   cpuset;

	struct ibv_qp	*qp	    = ib_res.qp;
	struct ibv_cq	*cq	    = ib_res.cq;
	struct ibv_wc	*wc	    = NULL;
	uint32_t             lkey       = ib_res.mr->lkey;
	char		*buf_ptr    = ib_res.ib_buf;
	int			 buf_offset = 0;
	size_t               buf_size   = ib_res.ib_buf_size;

	struct timeval      start, end;
	long                ops_count  = 0;
	double              duration   = 0.0;
	double throughput = 0.0;




	if(setThreadAffinity(threadId)){
	  return;
	}

	if (postSendWorkReq(&ctx))
	{
	  std::cerr<<stderr, "Couldn't post send\n";
	  return;
	}

	ctx.pending |= SEND_WRID;

	if (gettimeofday(&start, NULL))
	{
	  std::cerr<< "gettimeofday";
	  return;
	}

	rcnt = scnt = 0;
	while (rcnt < iters || scnt < iters)
	{
	  if (use_event)
	  {
		struct ibv_cq *ev_cq;
		void *ev_ctx;

		if (ibv_get_cq_event(ctx->channel, &ev_cq, &ev_ctx))
		{
		  fprintf(stderr, "Failed to get cq_event\n");
		  return 1;
		}

		++num_cq_events;

		if (ev_cq != ctx->cq)
		{
		  fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
		  return 1;
		}

		if (ibv_req_notify_cq(ctx->cq, 0))
		{
		  fprintf(stderr, "Couldn't request CQ notification\n");
		  return 1;
		}
	  }

	  {
		struct ibv_wc wc[2];
		int ne, i;

		do
		{
		  ne = ibv_poll_cq(ctx->cq, 2, wc);
		  if (ne < 0)
		  {
			fprintf(stderr, "poll CQ failed %d\n", ne);
			return 1;
		  }

		}

		while (!use_event && ne < 1);

		for (i = 0; i < ne; ++i)
		{
		  if (wc[i].status != IBV_WC_SUCCESS)
		  {
			fprintf(stderr,
				"Failed status %s (%d) for wr_id %d\n",
				ibv_wc_status_str(wc[i].status),
				wc[i].status,
				(int) wc[i].wr_id);
			return 1;
		  }

		  switch ((int) wc[i].wr_id)
		  {
			case SEND_WRID:
			  ++scnt;
			  break;

			case RECV_WRID:
			  if (--routs <= 1)
			  {
				routs += postRecvWorkReq(ctx, ctx->rx_depth - routs);
				if (routs < ctx->rx_depth)
				{
				  fprintf(stderr, "Couldn't post receive (%d)\n", routs);
				  return 1;
				}
			  }

			  ++rcnt;
			  break;

			default:
			  fprintf(stderr, "Completion for unknown wr_id %d\n", (int) wc[i].wr_id);
			  return 1;
		  }

		  ctx->pending &= ~(int) wc[i].wr_id;
		  if (scnt < iters && !ctx->pending)
		  {
			if (postSendWorkReq(ctx))
			{
			  fprintf(stderr, "Couldn't post send\n");
			  return 1;
			}
			ctx->pending = RECV_WRID | SEND_WRID;
		  }
		}
	  }
	}

	if (gettimeofday(&end, NULL)) {
	  perror("gettimeofday");
	  return 1;
	}

	{
	  float usec = (end.tv_sec - start.tv_sec) * 1000000 +
		  (end.tv_usec - start.tv_usec);
	  long long bytes = (long long) size * iters * 2;

	  printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n",
		  bytes, usec / 1000000., bytes * 8. / usec);
	  printf("%d iters in %.2f seconds = %.2f usec/iter\n",
		  iters, usec / 1000000., usec / iters);
	}

	ibv_ack_cq_events(ctx->cq, num_cq_events);


  }

  int setThreadAffinity(int threadId) const
  {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(threadId, &cpuset);
	int rc = pthread_setaffinity_np(pthread_self(),
		sizeof(cpu_set_t), &cpuset);
	if (rc != 0) {
	  std::cerr << "Error calling pthread_setaffinity_np: " << rc << " for thread with id: " << threadId << "\n";
	  return 1;
	}
	return 0;
  }

  int initThreads(){


	return 0;
  }

};
#endif //EX1V2_SIMPLEIBCLIENT_HPP
