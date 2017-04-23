#ifndef IBV_PINGPONG_H
#define IBV_PINGPONG_H

#include <infiniband/verbs.h>
#include <vector>
static int page_size;

enum ibv_mtu pp_mtu_to_enum(int mtu);
int pp_get_port_info(struct ibv_context *context, int port, struct ibv_port_attr *attr);
void wire_gid_to_gid(char *wgid, union ibv_gid *gid);
void gid_to_wire_gid(union ibv_gid *gid, char wgid[]);

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



typedef struct Connection{
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
	  goto out;
	}

	sscanf(msg, "%x:%x:%x:%s", &remoteQP.lid, &remoteQP.qpn, &remoteQP.psn, gid);
	wire_gid_to_gid(gid, &remoteQP.gid);
	printf("Read client QP address %s \n", msg);
	if (prepIbDeviceToConnect(ctx, ib_port, localQPserverInfo[l].psn, mtu, sl, &remoteQP, sgid_idx))
	{
	  fprintf(stderr, "Couldn't connect to remote QP\n");
	  goto out;
	}

	gid_to_wire_gid(&localQPserverInfo[l].gid, gid);
	sprintf(msg, "%04x:%06x:%06x:%s", localQPserverInfo[l].lid, localQPserverInfo[l].qpn, localQPserverInfo[l].psn, gid);
	printf("Read Server QP address %s \n", msg);
	if (write(connfd, msg, sizeof msg) != sizeof msg)
	{
	  fprintf(stderr, "Couldn't send local address\n");
	  goto out;
	}
	read(connfd, msg, sizeof msg);
	l++;

  });





  out:
  close(connfd);
  return 0;
}

extern Connection *connection;


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

int postRecvWorkReq(struct Connection *ctx, int n, int tid);


int prepIbDeviceToConnect(struct Connection *ctx,
	int port,
	int my_psn,
	enum ibv_mtu mtu,
	int sl,
	serverInfo *dest,
	int sgid_idx);

int postSendWorkReq(Connection*, int &);

int closeConnection(struct Connection *ctx);

/*
 * Connect client to remote and exchange QP address information
 */
int connectClientToRemote(const char *, int ,  std::vector<serverInfo> &,
						  std::vector<serverInfo> &);

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
