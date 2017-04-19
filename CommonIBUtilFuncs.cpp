/*
 * Copyright (c) 2006 Cisco Systems.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
#include <iostream>
#include <algorithm>
#include "CommonIBUtilFuncs.hpp"



enum ibv_mtu pp_mtu_to_enum(int mtu)
{
  switch (mtu) {
	case 256:  return IBV_MTU_256;
	case 512:  return IBV_MTU_512;
	case 1024: return IBV_MTU_1024;
	case 2048: return IBV_MTU_2048;
	case 4096: return IBV_MTU_4096;
	default:   return 0;
  }
}

int pp_get_port_info(struct ibv_context *context, int port,
	struct ibv_port_attr *attr)
{
  return ibv_query_port(context, port, attr);
}

void wire_gid_to_gid(const char *wgid, union ibv_gid *gid)
{
  char tmp[9];
  unsigned int v32;
  int i;
  uint32_t tmp_gid[4];

  for (tmp[8] = 0, i = 0; i < 4; ++i) {
	memcpy(tmp, wgid + i * 8, 8);
	sscanf(tmp, "%x", &v32);
	tmp_gid[i] = be32toh(v32);
  }
  memcpy(gid, tmp_gid, sizeof(*gid));
}

void gid_to_wire_gid(const union ibv_gid *gid, char wgid[]);


void gid_to_wire_gid(const union ibv_gid *gid, char *wgid)
{
  uint32_t tmp_gid[4];
  int i;

  memcpy(tmp_gid, gid, sizeof(tmp_gid));
  for (i = 0; i < 4; ++i)
	sprintf(&wgid[i * 8], "%08x", htobe32(tmp_gid[i]));
}


//TODO: clean this and replace with methods from Stream, Connector and Acceptor
serverInfo *connectClientToRemote(const char *servername, int port, const std::vector<serverInfo> &localQPserverInfo)
{
  struct addrinfo *res, *t;
  struct addrinfo hints = {
	  .ai_family   = AF_INET, .ai_socktype = SOCK_STREAM
  };
  char *service;
  char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
  int n;
  int sockfd = -1;
  serverInfo *rem_dest = NULL;
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

  rem_dest = (serverInfo *) malloc(sizeof *rem_dest);
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

void InitQPs(int port);
struct Connection *init_connection(struct ibv_device *ib_dev,
	int size,
	int rx_depth,
	int port,
	int use_event,
	int is_server,
	int peerNum,
	int messageChar)
{

  ctx.peerNum = peerNum;

  ctx.context = ibv_open_device(ib_dev);
  if (!ctx.context)
  {
	fprintf(stderr, "Couldn't get context for %s\n", ibv_get_device_name(ib_dev));
	return NULL;
  }

  if (use_event)
  {
	ctx.channel = ibv_create_comp_channel(ctx.context);
	if (!ctx.channel)
	{
	  fprintf(stderr, "Couldn't create completion channel\n");
	  return nullptr;
	}
  }
  else
  {
	ctx.channel = NULL;
  }

  ctx.pd = ibv_alloc_pd(ctx.context);
  if (!ctx.pd)
  {
	fprintf(stderr, "Couldn't allocate PD\n");
	return NULL;
  }

  ctx.size = sizeof(char) + 1;
  ctx.rx_depth = rx_depth;

  ctx.buf = malloc(roundup(size, page_size));
  if (!ctx.buf)
  {
	fprintf(stderr, "Couldn't allocate work buf.\n");
	return NULL;
  }

  memset(ctx.buf, messageChar, ctx.size);



  ctx.mr = ibv_reg_mr(ctx.pd, ctx.buf, ctx.size, IBV_ACCESS_LOCAL_WRITE);
  if (!ctx.mr)
  {
	fprintf(stderr, "Couldn't register MR\n");
	return NULL;
  }

  ctx.cq = ibv_create_cq(ctx.context, rx_depth + 1, NULL, ctx.channel, 0);
  if (!ctx.cq)
  {
	fprintf(stderr, "Couldn't create CQ\n");
	return NULL;
  }

  {
	struct ibv_qp_init_attr attr = {
		.send_cq = ctx.cq, .recv_cq = ctx.cq, .cap     = {
			.max_send_wr  = 1, .max_recv_wr  = rx_depth, .max_send_sge = 1, .max_recv_sge = 1
		}, .qp_type = IBV_QPT_RC
	};

	//Create our QP's
	ctx.qp = std::vector<ibv_qp>(peerNum);
	std::for_each(ctx.qp.begin(), ctx.qp.end(),[](ibv_qp& _qp){
	  _qp = *ibv_create_qp(ctx.pd, &attr);

	});
  }

  {
	InitQPs(port);
  }

  //Init our connection structs that will hold information on our and our destination QP addresses


  return &ctx;
}
void InitQPs(int port)
{
  struct ibv_qp_attr attr = {
		.qp_state        = IBV_QPS_INIT, .pkey_index      = 0, .port_num        = (uint8_t) port, .qp_access_flags = 0
	};

  for_each(ctx.qp.begin(), ctx.qp.end(),[](ibv_qp& _qp){
	  //INIT state of QP's
	  if (ibv_modify_qp(&_qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS))
	  {
		fprintf(stderr, "Failed to modify QP to INIT\n");
		return NULL;
	  }

	});
}

serverInfo *connectRemoteToClient(struct Connection *ctx,
	int ib_port,
	enum ibv_mtu mtu,
	int port,
	int sl,
	const serverInfo *my_dest,
	int sgid_idx)
{
  struct addrinfo *res, *t;
  struct addrinfo hints = {
	  .ai_flags    = AI_PASSIVE,
	  .ai_family   = AF_INET,
	  .ai_socktype = SOCK_STREAM
  };
  char *service;
  char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
  int n;
  int sockfd = -1, connfd;
  serverInfo *rem_dest = NULL;
  char gid[33];

  if (asprintf(&service, "%d", port) < 0)
	return NULL;

  n = getaddrinfo(NULL, service, &hints, &res);

  if (n < 0) {
	fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
	free(service);
	return NULL;
  }

  for (t = res; t; t = t->ai_next) {
	sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
	if (sockfd >= 0) {
	  n = 1;

	  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

	  if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
		break;
	  close(sockfd);
	  sockfd = -1;
	}
  }

  freeaddrinfo(res);
  free(service);

  if (sockfd < 0) {
	fprintf(stderr, "Couldn't listen to port %d\n", port);
	return NULL;
  }

  listen(sockfd, 1);
  connfd = accept(sockfd, NULL, 0);
  close(sockfd);
  if (connfd < 0) {
	fprintf(stderr, "accept() failed\n");
	return NULL;
  }

  n = read(connfd, msg, sizeof msg);
  if (n != sizeof msg) {
	perror("server read");
	fprintf(stderr, "%d/%d: Couldn't read remote address\n", n, (int) sizeof msg);
	goto out;
  }

  rem_dest = malloc(sizeof *rem_dest);
  if (!rem_dest)
	goto out;

  sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn, gid);
  wire_gid_to_gid(gid, &rem_dest->gid);

  if (prepIbDeviceToConnect(ctx, ib_port, my_dest->psn, mtu, sl, rem_dest, sgid_idx)) {
	fprintf(stderr, "Couldn't connect to remote QP\n");
	free(rem_dest);
	rem_dest = NULL;
	goto out;
  }


  gid_to_wire_gid(&my_dest->gid, gid);
  sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn, gid);
  if (write(connfd, msg, sizeof msg) != sizeof msg) {
	fprintf(stderr, "Couldn't send local address\n");
	free(rem_dest);
	rem_dest = NULL;
	goto out;
  }

  read(connfd, msg, sizeof msg);

  out:
  close(connfd);
  return rem_dest;
}
int setQPstateRTR(struct Connection *ctx,
	int port,
	int my_psn,
	enum ibv_mtu mtu,
	int sl,
	serverInfo *dest,
	int sgid_idx)
{
  struct ibv_qp_attr attr = {
	  .qp_state        = IBV_QPS_RTR,
	  .path_mtu        = mtu,
	  .dest_qp_num        = dest->qpn,
	  .rq_psn            = dest->psn,
	  .max_dest_rd_atomic    = 1,
	  .min_rnr_timer        = 12,
	  .ah_attr        = {
		  .is_global    = 0,
		  .dlid        = dest->lid,
		  .sl        = sl,
		  .src_path_bits= 0,
		  .port_num    = port
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

  return 0;
}

int setQPstateRTS(struct Connection *ctx,
	int port,
	int my_psn,
	enum ibv_mtu mtu,
	int sl,
	serverInfo *dest,
	int sgid_idx)
{
  // first the qp state has to be changed to rtr
  setQPstateRTR(ctx,
	  port,
	  my_psn,
	  mtu,
	  sl,
	  dest,
	  sgid_idx);
  struct ibv_qp_attr attr = {
	  .qp_state        = IBV_QPS_RTR,
	  .path_mtu        = mtu,
	  .dest_qp_num        = dest->qpn,
	  .rq_psn            = dest->psn,
	  .max_dest_rd_atomic    = 1,
	  .min_rnr_timer        = 12,
	  .ah_attr        = {
		  .is_global    = 0,
		  .dlid        = dest->lid,
		  .sl        = sl,
		  .src_path_bits= 0,
		  .port_num    = port
	  }};
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



/*
 * program posts a receive work request to the QP
 */
int postRecvWorkReq(struct Connection *ctx, int n)
{
  struct ibv_sge list = {
	  .addr    = (uintptr_t) ctx->buf, .length = ctx->size, .lkey    = ctx->mr->lkey
  };
  struct ibv_recv_wr wr = {
	  .wr_id        = RECV_WRID, .sg_list    = &list, .num_sge    = 1,
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

int closeConnection(struct Connection *ctx)
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

/*
 * program posts a send work request to the QP
 */
int postSendWorkReq(struct Connection *ctx)
{
  struct ibv_sge list = {
	  .addr    = (uintptr_t) ctx->buf, .length = ctx->size, .lkey    = ctx->mr->lkey
  };
  struct ibv_send_wr wr = {
	  .wr_id        = SEND_WRID, .sg_list    = &list, .num_sge    = 1, .opcode     = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED,
  };
  struct ibv_send_wr *bad_wr;

  return ibv_post_send(ctx->qp, &wr, &bad_wr);
}


int prepIbDeviceToConnect(struct Connection *ctx,
	int port,
	int my_psn,
	enum ibv_mtu mtu,
	int sl,
	serverInfo *dest,
	int sgid_idx)
{
  setQPstateRTR(ctx, port, my_psn, mtu, sl, dest, sgid_idx);
  setQPstateRTS(ctx, port, my_psn, mtu, sl, dest, sgid_idx);

  return 0;
}


serverInfo *connectClientToRemote(const char *servername, int port, const serverInfo *my_dest);

