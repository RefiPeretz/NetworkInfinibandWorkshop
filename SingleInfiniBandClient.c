//
// Created by fimka on 15/04/17.
//

#define _GNU_SOURCE
#include <infiniband/verbs.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <zconf.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "CommonIBUtilFuncs.h"





int main(int argc, char *argv[])
{
  struct ibv_device **dev_list;
  struct ibv_device *ib_dev;
  struct net_context *ctx;
  struct pingpong_dest my_dest;
  struct pingpong_dest *rem_dest;
  struct timeval start, end;
  char *ib_devname = NULL;
  char *servername = NULL;
  int port = 18515;
  int ib_port = 1;
  int size = 4096;
  enum ibv_mtu mtu = IBV_MTU_1024;
  int rx_depth = 500;
  int iters = 1000;
  int use_event = 0;
  int routs;
  int rcnt, scnt;
  int num_cq_events = 0;
  int sl = 0;
  int gidx = -1;
  char gid[33];


  //TODO: read device server ip from user

  srand48(getpid() * time(NULL));


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
  ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port, use_event, !servername);
  if (!ctx)
  {
	return 1;
  }




  /*
   * prepares connection to get the given amount of packets
   */
  routs = pp_post_recv(ctx, ctx->rx_depth);
  if (routs < ctx->rx_depth)
  {
	fprintf(stderr, "Couldn't post receive (%d)\n", routs);
	return 1;
  }

  //Exchange information on target server
  rem_dest = pp_client_exch_dest(servername, port, &my_dest);

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


  if (pp_connect_ctx(ctx, ib_port, my_dest.psn, mtu, sl, rem_dest, gidx))
  {
	return 1;
  }

  if (pp_post_send(ctx))
  {
	fprintf(stderr, "Couldn't post send\n");
	return 1;
  }

  ctx->pending |= PINGPONG_SEND_WRID;


  if (gettimeofday(&start, NULL))
  {
	perror("gettimeofday");
	return 1;
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
		  case PINGPONG_SEND_WRID:
			++scnt;
			break;

		  case PINGPONG_RECV_WRID:
			if (--routs <= 1)
			{
			  routs += pp_post_recv(ctx, ctx->rx_depth - routs);
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
		  if (pp_post_send(ctx))
		  {
			fprintf(stderr, "Couldn't post send\n");
			return 1;
		  }
		  ctx->pending = PINGPONG_RECV_WRID | PINGPONG_SEND_WRID;
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

  if (pp_close_ctx(ctx))
	return 1;

  ibv_free_device_list(dev_list);
  free(rem_dest);

  return 0;
}



