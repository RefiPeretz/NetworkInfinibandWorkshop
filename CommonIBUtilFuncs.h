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

#ifndef IBV_PINGPONG_H
#define IBV_PINGPONG_H

#include <infiniband/verbs.h>
static int page_size;

enum ibv_mtu pp_mtu_to_enum(int mtu);
int pp_get_port_info(struct ibv_context *context, int port, struct ibv_port_attr *attr);
void wire_gid_to_gid(const char *wgid, union ibv_gid *gid);
void gid_to_wire_gid(const union ibv_gid *gid, char wgid[]);
struct net_context *
pp_init_ctx(struct ibv_device *ib_dev, int size, int rx_depth, int port, int use_event, int is_server);

struct net_context{
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

void gid_to_wire_gid(const union ibv_gid *gid, char *wgid);

enum
{
  PINGPONG_RECV_WRID = 1, PINGPONG_SEND_WRID = 2,
};

struct pingpong_dest
{
  int lid;
  int qpn;
  int psn;
  union ibv_gid gid;
};

int pp_post_recv(struct net_context *ctx, int n);

/*
 * Exchange IB information with server - in order to create a new baby.
 * I.E. connection. real connection.
 */
struct pingpong_dest *pp_client_exch_dest(const char *servername, int port, const struct pingpong_dest *my_dest);


int pp_connect_ctx(struct net_context *ctx,
	int port,
	int my_psn,
	enum ibv_mtu mtu,
	int sl,
	struct pingpong_dest *dest,
	int sgid_idx);

int pp_post_send(struct net_context *ctx);

int pp_close_ctx(struct net_context *ctx);

struct pingpong_dest *pp_server_exch_dest(struct net_context *ctx,
	int ib_port,
	enum ibv_mtu mtu,
	int port,
	int sl,
	const struct pingpong_dest *my_dest,
	int sgid_idx);

#endif /* IBV_PINGPONG_H */
