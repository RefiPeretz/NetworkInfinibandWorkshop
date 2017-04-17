//
// Created by fimka on 15/04/17.
//

#include <infiniband/verbs.h>
#include <sys/param.h>
#include <stdlib.h>
#include <stdio.h>

enum {
  PINGPONG_RECV_WRID = 1,
  PINGPONG_SEND_WRID = 2,
};

static int page_size;

  int                      ib_port = 1;
  int                      size = 4096;
  int                      rx_depth = 500;
  int                      use_event = 0;
  char                    *servername = NULL;

  struct net_context {
	struct ibv_context	*context;
	struct ibv_comp_channel *channel;
	struct ibv_pd		*pd;
	struct ibv_mr		*mr;
	struct ibv_cq		*cq;
	struct ibv_qp		*qp;
	void			*buf;
	int			 size;
	int			 rx_depth;
	int			 pending;
	struct ibv_port_attr     portinfo;
  };

  static struct net_context *pp_init_ctx(struct ibv_device *ib_dev, int size,
	  int rx_depth, int port,
	  int use_event, int is_server)
  {
	struct net_context *ctx;

	ctx =calloc(1, sizeof *ctx);
	if (!ctx)
	  return NULL;

	ctx->size     = size;
	ctx->rx_depth = rx_depth;

	ctx->buf = malloc(roundup(size, page_size));
	if (!ctx->buf) {
	  fprintf(stderr, "Couldn't allocate work buf.\n");
	  return NULL;
	}

	memset(ctx->buf, 0x7b + is_server, size);

	ctx->context = ibv_open_device(ib_dev);
	if (!ctx->context) {
	  fprintf(stderr, "Couldn't get context for %s\n",
		  ibv_get_device_name(ib_dev));
	  return NULL;
	}

	if (use_event) {
	  ctx->channel = ibv_create_comp_channel(ctx->context);
	  if (!ctx->channel) {
		fprintf(stderr, "Couldn't create completion channel\n");
		return NULL;
	  }
	} else
	  ctx->channel = NULL;

	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
	  fprintf(stderr, "Couldn't allocate PD\n");
	  return NULL;
	}

	ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, size, IBV_ACCESS_LOCAL_WRITE);
	if (!ctx->mr) {
	  fprintf(stderr, "Couldn't register MR\n");
	  return NULL;
	}

	ctx->cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL,
		ctx->channel, 0);
	if (!ctx->cq) {
	  fprintf(stderr, "Couldn't create CQ\n");
	  return NULL;
	}

	{
	  struct ibv_qp_init_attr attr = {
		  .send_cq = ctx->cq,
		  .recv_cq = ctx->cq,
		  .cap     = {
			  .max_send_wr  = 1,
			  .max_recv_wr  = rx_depth,
			  .max_send_sge = 1,
			  .max_recv_sge = 1
		  },
		  .qp_type = IBV_QPT_RC
	  };

	  //Create our QP's
	  ctx->qp = ibv_create_qp(ctx->pd, &attr);
	  if (!ctx->qp)  {
		fprintf(stderr, "Couldn't create QP\n");
		return NULL;
	  }
	}

	{
	  struct ibv_qp_attr attr = {
		  .qp_state        = IBV_QPS_INIT,
		  .pkey_index      = 0,
		  .port_num        = (uint8_t) port,
		  .qp_access_flags = 0
	  };

	  //INIT state of QP's
	  if (ibv_modify_qp(ctx->qp, &attr,
		  IBV_QP_STATE              |
			  IBV_QP_PKEY_INDEX         |
			  IBV_QP_PORT               |
			  IBV_QP_ACCESS_FLAGS)) {
		fprintf(stderr, "Failed to modify QP to INIT\n");
		return NULL;
	  }
	}

	return ctx;
  }

  int main(int argc, char *argv[])
  {

	//get the device list on the clinet
	struct ibv_device ** dev_list = ibv_get_device_list(NULL);

	//Get device from list.
	struct ibv_device *ib_dev = *dev_list;
	if (!ib_dev) {
	  fprintf(stderr, "No IB devices found\n");
	  return 1;
	}


	//Init's all the needed structures for the connection and returns "ctx" Holds the whole connection data
	struct net_context *ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port, use_event, !servername);
	if (!ctx)
	{
	  return 1;
	}



	ibv_free_device_list(dev_list); //Only after we've opened a device



	return 0;
  }



