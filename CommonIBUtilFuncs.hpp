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
#include <vector>
static int page_size;

enum ibv_mtu pp_mtu_to_enum(int mtu);
int pp_get_port_info(struct ibv_context *context, int port, struct ibv_port_attr *attr);
void wire_gid_to_gid(char *wgid, union ibv_gid *gid);
void (union ibv_gid *gid, char wgid[]);

/*
 * Holds information that identifies the machine and exchanged during the TCP or any other out of band first handshake
 */
typedef struct serverInfo
{
  int lid; // Local identifier - unique port number (assigned when active)
  int qpn; //Queue pair number - Q identifier on the HCA
  int psn; //Packet seq. number - used by HCA to verify packet coming in order and no missing packets
  union ibv_gid gid; // cast identifier - identifies an endpoint
};



struct Connection{
  struct ibv_context *context;
  struct ibv_comp_channel *channel;
  struct ibv_pd *pd;
  struct ibv_mr *mr;
  struct ibv_cq *cq;
  std::vector<ibv_qp> qp;
  int peerNum;
  void *buf;
  int size;
  int rx_depth;
  int pending;
  struct ibv_port_attr portinfo;
};

extern struct Connection ctx;

struct Connection *init_connection(struct ibv_device *ib_dev, int size, int rx_depth, int port, int use_event, int
is_server,
	int
	peerNum, int messageChar);
void gid_to_wire_gid(const union ibv_gid *gid, char *wgid);

enum
{
  RECV_WRID = 1, SEND_WRID = 2,
};

/*
 *  qp_change_state_rtr
 * **********************
 *  Changes Queue Pair status to RTR (Ready to receive)
 */
int setQPstateRTR(struct Connection *ctx,
	int port,
	int my_psn,
	enum ibv_mtu mtu,
	int sl,
	serverInfo *dest,
	int sgid_idx);


/*
 *  qp_change_state_rts
 * **********************
 *  Changes Queue Pair status to RTS (Ready to send)
 *	QP status has to be RTR before changing it to RTS
 */
int setQPstateRTS(struct Connection *ctx,
	int port,
	int my_psn,
	enum ibv_mtu mtu,
	int sl,
	serverInfo *dest,
	int sgid_idx)
;

int postRecvWorkReq(struct Connection *ctx, int n);


int prepIbDeviceToConnect(struct Connection *ctx,
	int port,
	int my_psn,
	enum ibv_mtu mtu,
	int sl,
	serverInfo *dest,
	int sgid_idx);

int postSendWorkReq(struct Connection *ctx);

int closeConnection(struct Connection *ctx);

/*
 * Connect client to remote and exchange QP address information
 */
int connectClientToRemote(const char *, int , const std::vector<serverInfo> &,
						 const std::vector<serverInfo> &);

/*
 * Connect remote to client and exchange QP address information
 */
serverInfo *connectRemoteToClient(struct Connection *ctx,
	int ib_port,
	enum ibv_mtu mtu,
	int port,
	int sl,
	const serverInfo *my_dest,
	int sgid_idx);

#endif /* IBV_PINGPONG_H */
