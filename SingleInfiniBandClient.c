//
// Created by fimka on 15/04/17.
//

#define _GNU_SOURCE
#include <infiniband/verbs.h>
#include <sys/param.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <zconf.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "pingpong.h"

enum
{
  PINGPONG_RECV_WRID = 1, PINGPONG_SEND_WRID = 2,
};/**/


static int page_size;


struct net_context
{
  struct ibv_context *context;
  struct ibv_comp_channel *channel;
  struct ibv_pd *pd;
  struct ibv_mr *mr;
  struct ibv_cq *cq;
  struct ibv_qp *qp;
  void *buf;
  int size;
  int rx_depth;
  int pending;
  struct ibv_port_attr portinfo;
};

struct pingpong_dest
{
  int lid;
  int qpn;
  int psn;
  union ibv_gid gid;
};

static struct net_context *
pp_init_ctx(struct ibv_device *ib_dev, int size, int rx_depth, int port, int use_event, int is_server)
{
  struct net_context *ctx;

  ctx = calloc(1, sizeof *ctx);
  if (!ctx)
  {
	return NULL;
  }

  ctx->size = size;
  ctx->rx_depth = rx_depth;

  ctx->buf = malloc(roundup(size, page_size));
  if (!ctx->buf)
  {
	fprintf(stderr, "Couldn't allocate work buf.\n");
	return NULL;
  }

  memset(ctx->buf, 0x7b + is_server, size);

  ctx->context = ibv_open_device(ib_dev);
  if (!ctx->context)
  {
	fprintf(stderr, "Couldn't get context for %s\n", ibv_get_device_name(ib_dev));
	return NULL;
  }

  if (use_event)
  {
	ctx->channel = ibv_create_comp_channel(ctx->context);
	if (!ctx->channel)
	{
	  fprintf(stderr, "Couldn't create completion channel\n");
	  return NULL;
	}
  }
  else
  {
	ctx->channel = NULL;
  }

  ctx->pd = ibv_alloc_pd(ctx->context);
  if (!ctx->pd)
  {
	fprintf(stderr, "Couldn't allocate PD\n");
	return NULL;
  }

  ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, size, IBV_ACCESS_LOCAL_WRITE);
  if (!ctx->mr)
  {
	fprintf(stderr, "Couldn't register MR\n");
	return NULL;
  }

  ctx->cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL, ctx->channel, 0);
  if (!ctx->cq)
  {
	fprintf(stderr, "Couldn't create CQ\n");
	return NULL;
  }

  {
	struct ibv_qp_init_attr attr = {
		.send_cq = ctx->cq, .recv_cq = ctx->cq, .cap     = {
			.max_send_wr  = 1, .max_recv_wr  = rx_depth, .max_send_sge = 1, .max_recv_sge = 1
		}, .qp_type = IBV_QPT_RC
	};

	//Create our QP's
	ctx->qp = ibv_create_qp(ctx->pd, &attr);
	if (!ctx->qp)
	{
	  fprintf(stderr, "Couldn't create QP\n");
	  return NULL;
	}
  }

  {
	struct ibv_qp_attr attr = {
		.qp_state        = IBV_QPS_INIT, .pkey_index      = 0, .port_num        = (uint8_t) port, .qp_access_flags = 0
	};

	//INIT state of QP's
	if (ibv_modify_qp(ctx->qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS))
	{
	  fprintf(stderr, "Failed to modify QP to INIT\n");
	  return NULL;
	}
  }

  return ctx;
}

static int pp_post_recv(struct net_context *ctx, int n)
{
  struct ibv_sge list = {
	  .addr    = (uintptr_t) ctx->buf, .length = ctx->size, .lkey    = ctx->mr->lkey
  };
  struct ibv_recv_wr wr = {
	  .wr_id        = PINGPONG_RECV_WRID, .sg_list    = &list, .num_sge    = 1,
  };
  struct ibv_recv_wr *bad_wr;
  int i;

  for (i = 0; i < n; ++i)
  {
	if (ibv_post_recv(ctx->qp, &wr, &bad_wr))
	{
	  break;
	}
  }

  return i;
}


/*
 * Exchange IB information with server - in order to create a new baby.
 * I.E. connection. real connection.
 */
static struct pingpong_dest *pp_client_exch_dest(const char *servername, int port, const struct pingpong_dest *my_dest)
{
  struct addrinfo *res, *t;
  struct addrinfo hints = {
	  .ai_family   = AF_INET, .ai_socktype = SOCK_STREAM
  };
  char *service;
  char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
  int n;
  int sockfd = -1;
  struct pingpong_dest *rem_dest = NULL;
  char gid[33];

  if (asprintf(&service, "%d", port) < 0)
  {
	return NULL;
  }

  n = getaddrinfo(servername, service, &hints, &res);

  if (n < 0)
  {
	fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername, port);
	free(service);
	return NULL;
  }

  for (t = res; t; t = t->ai_next)
  {
	sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
	if (sockfd >= 0)
	{
	  if (!connect(sockfd, t->ai_addr, t->ai_addrlen))
	  {
		break;
	  }
	  close(sockfd);
	  sockfd = -1;
	}
  }

  freeaddrinfo(res);
  free(service);

  if (sockfd < 0)
  {
	fprintf(stderr, "Couldn't connect to %s:%d\n", servername, port);
	return NULL;
  }

  gid_to_wire_gid(&my_dest->gid, gid);
  sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn, gid);
  if (write(sockfd, msg, sizeof msg) != sizeof msg)
  {
	fprintf(stderr, "Couldn't send local address\n");
	goto out;
  }

  if (read(sockfd, msg, sizeof msg) != sizeof msg)
  {
	perror("client read");
	fprintf(stderr, "Couldn't read remote address\n");
	goto out;
  }

  write(sockfd, "done", sizeof "done");

  rem_dest = (struct pingpong_dest *) malloc(sizeof *rem_dest);
  if (!rem_dest)
  {
	goto out;
  }

  sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn, gid);
  wire_gid_to_gid(gid, &rem_dest->gid);

  out:
  close(sockfd);
  return rem_dest;
}


static int pp_connect_ctx(struct net_context *ctx,
	int port,
	int my_psn,
	enum ibv_mtu mtu,
	int sl,
	struct pingpong_dest *dest,
	int sgid_idx)
{
  struct ibv_qp_attr attr = {
	  .qp_state        = IBV_QPS_RTR, .path_mtu        = mtu, .dest_qp_num        = dest->qpn, .rq_psn            = dest
		  ->psn, .max_dest_rd_atomic    = 1, .min_rnr_timer        = 12, .ah_attr        = {
		  .is_global    = 0, .dlid        = dest->lid, .sl        = sl, .src_path_bits    = 0, .port_num    = port
	  }};

  if (dest->gid.global.interface_id)
  {
	attr.ah_attr.is_global = 1;
	attr.ah_attr.grh.hop_limit = 1;
	attr.ah_attr.grh.dgid = dest->gid;
	attr.ah_attr.grh.sgid_index = sgid_idx;
  }
  if (ibv_modify_qp(ctx->qp,
	  &attr,
	  IBV_QP_STATE
		  | IBV_QP_AV
		  | IBV_QP_PATH_MTU
		  | IBV_QP_DEST_QPN
		  | IBV_QP_RQ_PSN
		  | IBV_QP_MAX_DEST_RD_ATOMIC
		  | IBV_QP_MIN_RNR_TIMER))
  {
	fprintf(stderr, "Failed to modify QP to RTR\n");
	return 1;
  }

  attr.qp_state = IBV_QPS_RTS;
  attr.timeout = 14;
  attr.retry_cnt = 7;
  attr.rnr_retry = 7;
  attr.sq_psn = my_psn;
  attr.max_rd_atomic = 1;
  if (ibv_modify_qp(ctx->qp,
	  &attr,
	  IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC))
  {
	fprintf(stderr, "Failed to modify QP to RTS\n");
	return 1;
  }

  return 0;
}

static int pp_post_send(struct net_context *ctx)
{
  struct ibv_sge list = {
	  .addr    = (uintptr_t) ctx->buf, .length = ctx->size, .lkey    = ctx->mr->lkey
  };
  struct ibv_send_wr wr = {
	  .wr_id        = PINGPONG_SEND_WRID, .sg_list    = &list, .num_sge    = 1, .opcode     = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED,
  };
  struct ibv_send_wr *bad_wr;

  return ibv_post_send(ctx->qp, &wr, &bad_wr);
}

int pp_close_ctx(struct net_context *ctx)
{
  if (ibv_destroy_qp(ctx->qp)) {
	fprintf(stderr, "Couldn't destroy QP\n");
	return 1;
  }

  if (ibv_destroy_cq(ctx->cq)) {
	fprintf(stderr, "Couldn't destroy CQ\n");
	return 1;
  }

  if (ibv_dereg_mr(ctx->mr)) {
	fprintf(stderr, "Couldn't deregister MR\n");
	return 1;
  }

  if (ibv_dealloc_pd(ctx->pd)) {
	fprintf(stderr, "Couldn't deallocate PD\n");
	return 1;
  }

  if (ctx->channel) {
	if (ibv_destroy_comp_channel(ctx->channel)) {
	  fprintf(stderr, "Couldn't destroy completion channel\n");
	  return 1;
	}
  }

  if (ibv_close_device(ctx->context)) {
	fprintf(stderr, "Couldn't release context\n");
	return 1;
  }

  free(ctx->buf);
  free(ctx);

  return 0;
}

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

  srand48(getpid() * time(NULL));


  //get the device list on the clinet
  dev_list = ibv_get_device_list(NULL);

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



