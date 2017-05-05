//
// Created by fimak on 4/29/17.
//

#ifndef EX1V2_RC_PINGPONG_H
#define EX1V2_RC_PINGPONG_H


static int pp_connect_ctx(struct pingpong_context *ctx, int port, int my_psn,
                          enum ibv_mtu mtu, int sl,
                          struct pingpong_dest *dest, int sgid_idx);

static struct pingpong_dest *pp_client_exch_dest(const char *servername, int port,
                                                 const struct pingpong_dest
                                                 *my_dest);

static struct pingpong_dest *pp_server_exch_dest(struct pingpong_context *ctx,
                                                 int ib_port, enum ibv_mtu mtu,
                                                 int port, int sl,
                                                 const struct pingpong_dest *my_dest,
                                                 int sgid_idx);


static struct pingpong_context *pp_init_ctx(struct ibv_device *ib_dev, int size,
                                            int rx_depth, int port,
                                            int use_event, int is_server);

int pp_close_ctx(struct pingpong_context *ctx);

static int pp_post_recv(struct pingpong_context *ctx, int n);
static int pp_post_send(struct pingpong_context *ctx);

static void usage(const char *argv0);

float runPingPong(int newport, long newsize, char* newservername);


#endif //EX1V2_RC_PINGPONG_H
