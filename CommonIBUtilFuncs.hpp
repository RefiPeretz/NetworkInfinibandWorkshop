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
  std::vector<ibv_qp*> qp;
  int peerNum;
  void *buf;
  int size;
  int rx_depth;
  int pending;
  struct ibv_port_attr portinfo;
	int routs;
};
static Connection *connection;

/*
 * Connect remote to client and exchange QP address information
 */
int connectRemoteToClient(struct Connection *ctx,
	int ib_port,
	enum ibv_mtu mtu,
	int port,
	int sl,
	int sgid_idx,
	std::vector<serverInfo> &localQPserverInfo,
	std::vector<serverInfo> &remoteQPserverInfo);


struct Connection *init_connection(struct ibv_device *ib_dev,
	int size,
	int rx_depth,
	int port,
	int use_event,
	int is_server,
	int peerNum,
	int messageChar);



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

int InitQPs(int port);


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



#endif /* IBV_PINGPONG_H */
