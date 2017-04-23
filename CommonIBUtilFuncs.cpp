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

#include <sys/param.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <zconf.h>
#include <iostream>
#include <algorithm>
#include <malloc.h>
#include "CommonIBUtilFuncs.hpp"

enum ibv_mtu pp_mtu_to_enum(int mtu)
{
  switch (mtu)
  {
	case 256:
	  return IBV_MTU_256;
	case 512:
	  return IBV_MTU_512;
	case 1024:
	  return IBV_MTU_1024;
	case 2048:
	  return IBV_MTU_2048;
	case 4096:
	  return IBV_MTU_4096;
	  //        default:
	  //            return 0; //TODO:
  }
}

int pp_get_port_info(struct ibv_context *context, int port, struct ibv_port_attr *attr)
{
  return ibv_query_port(context, port, attr);
}

void wire_gid_to_gid(char *wgid, union ibv_gid *gid)
{
  char tmp[9];
  unsigned int v32;
  int i;
  uint32_t tmp_gid[4];

  for (tmp[8] = 0, i = 0; i < 4; ++i)
  {
	memcpy(tmp, wgid + i * 8, 8);
	sscanf(tmp, "%x", &v32);
	tmp_gid[i] = be32toh(v32);
  }
  memcpy(gid, tmp_gid, sizeof(*gid));
}

void gid_to_wire_gid(union ibv_gid *gid, char wgid[]);


void gid_to_wire_gid(union ibv_gid *gid, char *wgid)
{
  uint32_t tmp_gid[4];
  int i;

  memcpy(tmp_gid, gid, sizeof(tmp_gid));
  for (i = 0; i < 4; ++i)
  {
	sprintf(&wgid[i * 8], "%08x", htobe32(tmp_gid[i]));
  }
}


//TODO: clean this and replace with methods from Stream, Connector and Acceptor
int connectClientToRemote(const char *servername,
	int port,
	std::vector<serverInfo> &localQPserverInfo,
	std::vector<serverInfo> &remoteQPserverInfo)
{
  struct addrinfo *res, *t;
  struct addrinfo hints;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char *service;
  char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
  int n;
  int sockfd = -1;
  serverInfo *rem_dest = NULL;

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

  unsigned int k = 0;
  std::for_each(localQPserverInfo.begin(), localQPserverInfo.end(), [&](serverInfo &localQP)
  {
	char gid[33];
	gid_to_wire_gid(&localQP.gid, gid);
	sprintf(msg, "%04x:%06x:%06x:%s", localQP.lid, localQP.qpn, localQP.psn, localQP.gid);
	printf("Sending Local QP info - %s", msg);
	if (write(sockfd, msg, sizeof msg) != sizeof msg)
	{
	  fprintf(stderr, "Couldn't send local address\n");
	  close(sockfd);
	  return 1;
	}

	if (read(sockfd, msg, sizeof msg) != sizeof msg)
	{
	  perror("client read");
	  fprintf(stderr, "Couldn't read remote address\n");
	  close(sockfd);
	  return 1;
	}
	printf("Read remote QP info - %s", msg);

	write(sockfd, "done", sizeof "done");

	sscanf(msg,
		"%d:%d:%d:%s",
		remoteQPserverInfo[k].lid,
		remoteQPserverInfo[k].qpn,
		remoteQPserverInfo[k].psn,
		remoteQPserverInfo[k].gid);
	wire_gid_to_gid(gid, &remoteQPserverInfo[k].gid);


	k++;
  });

  out:
  close(sockfd);
  return 0;
}


struct Connection *init_connection(struct ibv_device *ib_dev,
	int size,
	int rx_depth,
	int port,
	int use_event,
	int is_server,
	int peerNum,
	int messageChar)
{
  //struct Connection *ctx;


  connection = (Connection *) calloc(1, sizeof *connection);
  if (!connection)
	return NULL;

  std::cout<< "Init connection " << std::endl;
  connection->peerNum = peerNum;

  std::cout<< "Open device "<<ib_dev->dev_name << std::endl;
  connection->context = ibv_open_device(ib_dev);
  if (!connection->context)
  {
	fprintf(stderr, "Couldn't get context for %s\n", ibv_get_device_name(ib_dev));
	return NULL;
  }

  if (use_event)
  {
	connection->channel = ibv_create_comp_channel(connection->context);
	if (!connection->channel)
	{
	  fprintf(stderr, "Couldn't create completion channel\n");
	  return nullptr;
	}
  }
  else
  {
	connection->channel = NULL;
  }

  std::cout<< "Init connection " << std::endl;
  connection->pd = ibv_alloc_pd(connection->context);
  if (!connection->pd)
  {
	fprintf(stderr, "Couldn't allocate PD\n");
	return NULL;
  }

  std::cout<< "Init connection size" << std::endl;

  connection->size = sizeof(char) + 1;
  connection->rx_depth = rx_depth;
  page_size = sysconf(_SC_PAGESIZE);

  connection->buf = malloc(roundup(size, page_size));
  if (!connection->buf)
  {
	fprintf(stderr, "Couldn't allocate work buf.\n");
	return NULL;
  }
  std::cout<< "Set buffer char " << std::endl;

  memset(connection->buf, messageChar, connection->size);
  std::cout<< "Buffer char set with " <<messageChar << " with size " << connection->size<< std::endl;

  connection->pd = ibv_alloc_pd(connection->context);
  if (!connection->pd) {
	fprintf(stderr, "Couldn't allocate PD\n");
	return NULL;
  }
  std::cout<< "PD set " << std::endl;


  connection->mr = ibv_reg_mr(connection->pd, connection->buf, connection->size, IBV_ACCESS_LOCAL_WRITE);
  if (!connection->mr)
  {
	fprintf(stderr, "Couldn't register MR\n");
	return NULL;
  }
  std::cout<< "MR set " << std::endl;

  connection->cq = ibv_create_cq(connection->context, rx_depth + 1, NULL, connection->channel, 0);
  if (!connection->cq)
  {
	fprintf(stderr, "Couldn't create CQ\n");
	return NULL;
  }
  std::cout<< "CQ set " << std::endl;

  {
	struct ibv_qp_init_attr attr;
	attr.send_cq = connection->cq;
	attr.recv_cq = connection->cq;
	attr.cap = {
		1, rx_depth, 1, 1
	};
	attr.qp_type = IBV_QPT_RC;

	std::cout<< "QP BASE attr set " << std::endl;


	//Create our QP's
	connection->qp = std::vector<ibv_qp>(peerNum);
	std::cout<< "QP vector create and assign " << std::endl;

	std::for_each(connection->qp.begin(), connection->qp.end(), [&](ibv_qp &_qp)
	{
	  _qp = *ibv_create_qp(connection->pd, &attr);
	  std::cout<< "QP create  " << std::endl;
	});
  }
  std::cout<< "Finished creating QP's  " << std::endl;

  {
	InitQPs(port);
  }

  //Init our connection structs that will hold information on our and our destination QP addresses


  return connection;
}

void InitQPs(int port)
{
  struct ibv_qp_attr attr;
  attr.qp_state = IBV_QPS_INIT;
  attr.pkey_index = 0;
  attr.port_num = (uint8_t) port;
  attr.qp_access_flags = 0;

  for_each(connection->qp.begin(), connection->qp.end(), [&](ibv_qp &_qp)
  {
	//INIT state of QP's
	if (ibv_modify_qp(&_qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS))
	{
	  fprintf(stderr, "Failed to modify QP to INIT\n");
	  return NULL;
	}

  });
}

int connectRemoteToClient(struct Connection *ctx,
	int ib_port,
	enum ibv_mtu mtu,
	int port,
	int sl,
	int sgid_idx,
	std::vector<serverInfo> &localQPserverInfo,
	std::vector<serverInfo> &remoteQPserverInfo)
{
  struct addrinfo *res, *t;
  struct addrinfo hints = {.ai_flags    = AI_PASSIVE, .ai_family   = AF_INET, .ai_socktype = SOCK_STREAM};
  char *service;
  char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
  int n;
  int sockfd = -1, connfd;
  char gid[33];

  if (asprintf(&service, "%d", port) < 0)
  {
	return 1;
  }

  n = getaddrinfo(NULL, service, &hints, &res);

  if (n < 0)
  {
	fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
	free(service);
	return 1;
  }

  for (t = res; t; t = t->ai_next)
  {
	sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
	if (sockfd >= 0)
	{
	  n = 1;

	  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

	  if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
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
	fprintf(stderr, "Couldn't listen to port %d\n", port);
	return 1;
  }

  listen(sockfd, 1);
  connfd = accept(sockfd, NULL, 0);
  close(sockfd);
  if (connfd < 0)
  {
	fprintf(stderr, "accept() failed\n");
	return 1;
  }


  unsigned int l = 0;
  std::for_each(remoteQPserverInfo.begin(), remoteQPserverInfo.end(), [&](serverInfo &remoteQP)
  {

	n = read(connfd, msg, sizeof msg);
	if (n != sizeof msg)
	{
	  perror("server read");
	  fprintf(stderr, "%d/%d: Couldn't read remote address\n", n, (int) sizeof msg);
	  close(connfd);
	  return 1;
	}

	sscanf(msg, "%x:%x:%x:%s", &remoteQP.lid, &remoteQP.qpn, &remoteQP.psn, gid);
	wire_gid_to_gid(gid, &remoteQP.gid);
	printf("Read client QP address %s \n", msg);
	if (prepIbDeviceToConnect(ctx, ib_port, localQPserverInfo[l].psn, mtu, sl, &remoteQP, sgid_idx))
	{
	  fprintf(stderr, "Couldn't connect to remote QP\n");
	  close(connfd);

	  return 1;
	}

	gid_to_wire_gid(&localQPserverInfo[l].gid, gid);
	sprintf(msg, "%04x:%06x:%06x:%s", localQPserverInfo[l].lid, localQPserverInfo[l].qpn, localQPserverInfo[l].psn, gid);
	printf("Read Server QP address %s \n", msg);
	if (write(connfd, msg, sizeof msg) != sizeof msg)
	{
	  fprintf(stderr, "Couldn't send local address\n");
	  close(connfd);
	  return 1;
	}
	read(connfd, msg, sizeof msg);
	l++;

  });





  out:
  return 0;
}

int
setQPstateRTR(struct Connection *ctx, int port, int my_psn, enum ibv_mtu mtu, int sl, serverInfo *dest, int sgid_idx)
{
  struct ibv_qp_attr attr;
  attr.qp_state = IBV_QPS_RTR;
  attr.path_mtu = mtu;
  attr.dest_qp_num = dest->qpn;
  attr.rq_psn = dest->psn;
  attr.max_dest_rd_atomic = 1;
  attr.min_rnr_timer = 12;
  attr.ah_attr.is_global = 0;
  attr.ah_attr.dlid = dest->lid;
  attr.ah_attr.sl = sl;
  attr.ah_attr.src_path_bits = 0;
  attr.ah_attr.port_num = port;


  if (dest->gid.global.interface_id)
  {
	attr.ah_attr.is_global = 1;
	attr.ah_attr.grh.hop_limit = 1;
	attr.ah_attr.grh.dgid = dest->gid;
	attr.ah_attr.grh.sgid_index = sgid_idx;
  }

  std::vector<ibv_qp> &_qpVector = ctx->qp;
  std::for_each(_qpVector.

		  begin(), _qpVector

		  .

			  end(),

	  [&](ibv_qp &qp)
	  {
		if (ibv_modify_qp(&qp,
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
	  });


  return 0;
}

int
setQPstateRTS(struct Connection *ctx, int port, int my_psn, enum ibv_mtu mtu, int sl, serverInfo *dest, int sgid_idx)
{
  // first the qp state has to be changed to rtr
  setQPstateRTR(ctx, port, my_psn, mtu, sl, dest, sgid_idx);
  struct ibv_qp_attr attr;

  attr.ah_attr.is_global = 0;
  attr.ah_attr.dlid = dest->lid;
  attr.ah_attr.sl = sl;
  attr.ah_attr.src_path_bits = 0;
  attr.ah_attr.port_num = port;


  attr.qp_state = IBV_QPS_RTS;
  attr.path_mtu = mtu;
  attr.dest_qp_num = dest->qpn;
  attr.rq_psn = dest->psn;
  attr.max_dest_rd_atomic = 1;
  attr.min_rnr_timer = 12;
  attr.path_mtu = mtu;
  attr.timeout = 14;
  attr.retry_cnt = 7;
  attr.rnr_retry = 7;
  attr.sq_psn = my_psn;
  attr.max_rd_atomic = 1;

  std::vector<ibv_qp> &_qpVector = ctx->qp;
  std::for_each(_qpVector.begin(), _qpVector.end(), [&](ibv_qp &qp)
  {
	if (ibv_modify_qp(&qp,
		&attr,
		IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC))
	{
	  fprintf(stderr, "Failed to modify QP to RTS\n");
	  return 1;
	}
  });

  return 0;
}


/*
 * program posts a receive work request to the QP
 */
int postRecvWorkReq(struct Connection *ctx, int n, int tid)
{
  struct ibv_sge list = {
	  .addr    = (uintptr_t) ctx->buf, .length = ctx->size, .lkey    = ctx->mr->lkey
  };
  struct ibv_recv_wr wr = {
	  RECV_WRID, NULL, &list, 1
  };
  struct ibv_recv_wr *bad_wr;
  int i;

  for (i = 0; i < n; ++i)
  {
	if (ibv_post_recv(&ctx->qp[tid], &wr, &bad_wr))
	{
	  break;
	}
  }

  return i;
}

int closeConnection(struct Connection *ctx, int tid)
{
  if (ibv_destroy_qp(&ctx->qp[tid]))
  {
	fprintf(stderr, "Couldn't destroy QP\n");
	return 1;
  }

  if (ibv_destroy_cq(ctx->cq))
  {
	fprintf(stderr, "Couldn't destroy CQ\n");
	return 1;
  }

  if (ibv_dereg_mr(ctx->mr))
  {
	fprintf(stderr, "Couldn't deregister MR\n");
	return 1;
  }

  if (ibv_dealloc_pd(ctx->pd))
  {
	fprintf(stderr, "Couldn't deallocate PD\n");
	return 1;
  }

  if (ctx->channel)
  {
	if (ibv_destroy_comp_channel(ctx->channel))
	{
	  fprintf(stderr, "Couldn't destroy completion channel\n");
	  return 1;
	}
  }

  if (ibv_close_device(ctx->context))
  {
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
int postSendWorkReq(Connection *ctx, int &threadId)
{
  struct ibv_sge list = {
	  (uintptr_t) ctx->buf, ctx->size, ctx->mr->lkey
  };

  struct ibv_send_wr wr;

  wr.wr_id = SEND_WRID;
  wr.sg_list = &list;
  wr.num_sge = 1;
  wr.opcode = IBV_WR_SEND;
  wr.send_flags = IBV_SEND_SIGNALED;
  struct ibv_send_wr *bad_wr;

  return ibv_post_send(&ctx->qp[threadId], &wr, &bad_wr);
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



