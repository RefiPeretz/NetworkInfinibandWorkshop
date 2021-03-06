/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/param.h>
#include <pthread.h>




#include "multithreadIBSupport.h"
#include "MetricsIBV.h"

#define CLIENT_TO_SERVER_RETRY_TIMEOUT 64 //Timeout to connect from client to server

enum
{
    PINGPONG_RECV_WRID = 1, PINGPONG_SEND_WRID = 2,
};

static int page_size;
struct pingpong_context
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
typedef struct Commands
{
    int port;
    int size;
    char *servername;
    int connfd;
    int threadNum;
} Commands;

struct pingpong_dest
{
    int lid;
    int qpn;
    int psn;
    union ibv_gid gid;
};

static int pp_connect_ctx(struct pingpong_context *ctx, int port, int my_psn,
                          enum ibv_mtu mtu, int sl, struct pingpong_dest *dest,
int sgid_idx)
{
    struct ibv_qp_attr attr =
            {.qp_state        = IBV_QPS_RTR, .path_mtu        = mtu, .dest_qp_num        = dest
                    ->qpn, .rq_psn            = dest
                    ->psn, .max_dest_rd_atomic    = 1, .min_rnr_timer        = 12, .ah_attr        = {.is_global    = 0, .dlid        = dest
                    ->lid, .sl        = sl, .src_path_bits    = 0, .port_num    = port}};

    if (dest->gid.global.interface_id)
    {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = dest->gid;
        attr.ah_attr.grh.sgid_index = sgid_idx;
    }
    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
                      IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                      IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER))
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
    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                      IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |
                      IBV_QP_MAX_QP_RD_ATOMIC))
    {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }

    return 0;
}

static struct pingpong_dest *
pp_client_exch_dest(const char *servername, int port,
                    const struct pingpong_dest *my_dest, int connectionId) {
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    struct pingpong_dest *rem_dest = NULL;
    char gid[33];

    if (connectionId < 0) {
        fprintf(stderr, "Couldn't connect to %s:%d\n", servername, port);
        return NULL;
    }

    gid_to_wire_gid(&my_dest->gid, gid);
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn,
            gid);
    if (write(connectionId, msg, sizeof msg) != sizeof msg) {
        fprintf(stderr, "Couldn't send local address\n");
        goto out;
    }
    printf("Wrote msg to server: %s\n", msg);

    if (read(connectionId, msg, sizeof msg) != sizeof msg) {
        perror("client read");
        fprintf(stderr, "Couldn't read remote address\n");
        goto out;
    }
    printf("Got msg from server: %s\n", msg);

    write(connectionId, "done", sizeof "done");
    printf("Wrote Done to server\n");
    rem_dest = malloc(sizeof *rem_dest);
    if (!rem_dest) {
        goto out;
    }

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn,
           gid);
    wire_gid_to_gid(gid, &rem_dest->gid);
    printf("got msg from server: %s\n", msg);

    out:
    return rem_dest;
}



int acceptClientConnection(int sockfd);

int createClientSocketConnection(int port, char *servername);

static int createServerConnection(int port)
{
    struct addrinfo *res, *t;
    struct addrinfo hints =
            {.ai_flags    = AI_PASSIVE, .ai_family   = AF_INET, .ai_socktype = SOCK_STREAM};
    char *service;
    //        char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    int sockfd = -1;

    if (asprintf(&service, "%d", port) < 0)
    {
        return -1;
    }

    n = getaddrinfo(NULL, service, &hints, &res);

    if (n < 0)
    {
        fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
        free(service);
        return -1;
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
        return -1;
    }

    int listen1 = listen(sockfd, 10); //TODO: check for errors
    if (listen1 < 0)
    {
        perror("Couldn't listen \n");
        return -1;
    }


    return sockfd;
}

int acceptClientConnection(int sockfd)
{
    int connfd;
    struct sockaddr_in	 peer_addr;
    socklen_t		 peer_addr_len	= sizeof(struct sockaddr_in);

    connfd = accept(sockfd, (struct sockaddr *)&peer_addr,
                    &peer_addr_len);
//    printf("current connfd %d\n", connfd);
    if(connfd < 0){
//        perror("accept() failed");
        return -1;
    }
//    close(sockfd); //TODO:
    return connfd;
}


static struct pingpong_dest *
pp_server_exch_dest(struct pingpong_context *ctx, int ib_port, enum ibv_mtu mtu,
                    int port, int sl, const struct pingpong_dest *my_dest,
                    int sgid_idx, int connfd)
{
    //    struct addrinfo *res, *t;
    //    struct addrinfo hints =
    //            {.ai_flags    = AI_PASSIVE, .ai_family   = AF_INET, .ai_socktype = SOCK_STREAM};
    //    char *service;
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    //    int sockfd = -1, connfd;
    struct pingpong_dest *rem_dest = NULL;
    char gid[33];

    //    if (asprintf(&service, "%d", port) < 0)
    //    {
    //        return NULL;
    //    }
    //
    //    n = getaddrinfo(NULL, service, &hints, &res);
    //
    //    if (n < 0)
    //    {
    //        fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
    //        free(service);
    //        return NULL;
    //    }
    //
    //    for (t = res; t; t = t->ai_next)
    //    {
    //        sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
    //        if (sockfd >= 0)
    //        {
    //            n = 1;
    //
    //            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);
    //
    //            if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
    //            {
    //                break;
    //            }
    //            close(sockfd);
    //            sockfd = -1;
    //        }
    //    }
    //
    //    freeaddrinfo(res);
    //    free(service);
    //
    //    if (sockfd < 0)
    //    {
    //        fprintf(stderr, "Couldn't listen to port %d\n", port);
    //        return NULL;
    //    }
    //
    //    listen(sockfd, 1);
    //
    //
    //    connfd = accept(sockfd, NULL, 0);
    //    close(sockfd); //TODO:

    //    int connfd = createServerConnection(port);
    if (connfd < 0)
    {
        fprintf(stderr, "accept() failed\n");
        return NULL;
    }

    n = read(connfd, msg, sizeof msg);
    if (n != sizeof msg)
    {
        perror("server read");
        fprintf(stderr, "%d/%d: Couldn't read remote address\n", n,
                (int) sizeof msg);
        goto out;
    }

    rem_dest = malloc(sizeof *rem_dest);
    if (!rem_dest)
    {
        goto out;
    }

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn,
           gid);
    wire_gid_to_gid(gid, &rem_dest->gid);

    if (pp_connect_ctx(ctx, ib_port, my_dest->psn, mtu, sl, rem_dest, sgid_idx))
    {
        fprintf(stderr, "Couldn't connect to remote QP\n");
        free(rem_dest);
        rem_dest = NULL;
        goto out;
    }


    gid_to_wire_gid(&my_dest->gid, gid);
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn,
            gid);
    if (write(connfd, msg, sizeof msg) != sizeof msg)
    {
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

static struct pingpong_context *
pp_init_ctx(struct ibv_device *ib_dev, int size, int rx_depth, int port,
            int use_event, int is_server)
{
    struct pingpong_context *ctx;

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
        fprintf(stderr, "Couldn't get context for %s\n",
                ibv_get_device_name(ib_dev));
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
    } else
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
        struct ibv_qp_init_attr attr = {.send_cq = ctx->cq, .recv_cq = ctx
                ->cq, .cap     = {.max_send_wr  = 1, .max_recv_wr  = rx_depth, .max_send_sge = 1, .max_recv_sge = 1}, .qp_type = IBV_QPT_RC};

        ctx->qp = ibv_create_qp(ctx->pd, &attr);
        if (!ctx->qp)
        {
            fprintf(stderr, "Couldn't create QP\n");
            return NULL;
        }
    }

    {
        struct ibv_qp_attr attr =
                {.qp_state        = IBV_QPS_INIT, .pkey_index      = 0, .port_num        = port, .qp_access_flags = 0};

        if (ibv_modify_qp(ctx->qp, &attr,
                          IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
                          IBV_QP_ACCESS_FLAGS))
        {
            fprintf(stderr, "Failed to modify QP to INIT\n");
            return NULL;
        }
    }

    return ctx;
}

int pp_close_ctx(struct pingpong_context *ctx)
{
    if (ibv_destroy_qp(ctx->qp))
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

static int pp_post_recv(struct pingpong_context *ctx, int n)
{
    struct ibv_sge list = {.addr    = (uintptr_t) ctx->buf, .length = ctx
            ->size, .lkey    = ctx->mr->lkey};
    struct ibv_recv_wr wr =
            {.wr_id        = PINGPONG_RECV_WRID, .sg_list    = &list, .num_sge    = 1,};
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

static int pp_post_send(struct pingpong_context *ctx)
{
    struct ibv_sge list = {.addr    = (uintptr_t) ctx->buf, .length = ctx
            ->size, .lkey    = ctx->mr->lkey};
    struct ibv_send_wr wr =
            {.wr_id        = PINGPONG_SEND_WRID, .sg_list    = &list, .num_sge    = 1, .opcode     = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED,};
    struct ibv_send_wr *bad_wr;

    return ibv_post_send(ctx->qp, &wr, &bad_wr);
}


void *runPingPong(void *commands1)
{
    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    struct pingpong_context *ctx;
    struct pingpong_dest my_dest;
    struct pingpong_dest *rem_dest;
    struct timeval start, end;
    char *ib_devname = NULL;
    char *servername = NULL;
    int port = 18515;
    int ib_port = 1;
    int size = 1048576;
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

    port = ((Commands *) (commands1))->port;
    if (port < 0 || port > 65535)
    {
        return (void *) 1;
    }
    printf("new port - %d\n", port);

    printf(" size param - %d\n", ((Commands *) (commands1))->size);

    size = ((Commands *) (commands1))->size;
    printf("new size - %d\n", size);

    servername = ((Commands *) (commands1))->servername;
    printf("commands - %x\n", *((Commands *) (commands1)));

    int connfd = ((Commands *) (commands1))->connfd;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(((Commands *) (commands1))->threadNum, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        perror("pthread_setaffinity_np");
        exit(-1);
    }
    printf("new connection id - %d\n", connfd);


    page_size = sysconf(_SC_PAGESIZE);

    dev_list = ibv_get_device_list(NULL);
    if (!dev_list)
    {
        perror("Failed to get IB devices list");
        return (void *) 1;
    }

    if (!ib_devname)
    {
        ib_dev = *dev_list;
        if (!ib_dev)
        {
            fprintf(stderr, "No IB devices found\n");
            return (void *) 1;
        }
    } else
    {
        int i;
        for (i = 0; dev_list[i]; ++i)
        {
            if (!strcmp(ibv_get_device_name(dev_list[i]), ib_devname))
            {
                break;
            }
        }
        ib_dev = dev_list[i];
        if (!ib_dev)
        {
            fprintf(stderr, "IB device %s not found\n", ib_devname);
            return (void *) 1;
        }
    }

    ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port, use_event, !servername);
    if (!ctx)
    {
        return (void *) 1;
    }

    routs = pp_post_recv(ctx, ctx->rx_depth);
    if (routs < ctx->rx_depth)
    {
        fprintf(stderr, "Couldn't post receive (%d)\n", routs);
        return (void *) 1;
    }

    if (use_event)
    {
        if (ibv_req_notify_cq(ctx->cq, 0))
        {
            fprintf(stderr, "Couldn't request CQ notification\n");
            return (void *) 1;
        }
    }


    if (pp_get_port_info(ctx->context, ib_port, &ctx->portinfo))
    {
        fprintf(stderr, "Couldn't get port info\n");
        return (void *) 1;
    }

    my_dest.lid = ctx->portinfo.lid;
    if (ctx->portinfo.link_layer == IBV_LINK_LAYER_INFINIBAND && !my_dest.lid)
    {
        fprintf(stderr, "Couldn't get local LID\n");
        return (void *) 1;
    }

    if (gidx >= 0)
    {
        if (ibv_query_gid(ctx->context, ib_port, gidx, &my_dest.gid))
        {
            fprintf(stderr, "Could not get local gid for gid index %d\n", gidx);
            return (void *) 1;
        }
    } else
    {
        memset(&my_dest.gid, 0, sizeof my_dest.gid);
    }

    my_dest.qpn = ctx->qp->qp_num;
    my_dest.psn = lrand48() & 0xffffff;
    inet_ntop(AF_INET6, &my_dest.gid, gid, sizeof gid);
    printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
           my_dest.lid, my_dest.qpn, my_dest.psn, gid);


    if (servername)
    {
        rem_dest = pp_client_exch_dest(servername, port, &my_dest, connfd);
    } else
    {
        rem_dest =
                pp_server_exch_dest(ctx, ib_port, mtu, port, sl, &my_dest, gidx,
                                    connfd);
    }

    if (!rem_dest)
    {
        return (void *) 1;
    }

    inet_ntop(AF_INET6, &rem_dest->gid, gid, sizeof gid);
    printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
           rem_dest->lid, rem_dest->qpn, rem_dest->psn, gid);

    if (servername)
    {
        if (pp_connect_ctx(ctx, ib_port, my_dest.psn, mtu, sl, rem_dest, gidx))
        {
            return (void *) 1;
        }
    }
    close(connfd);
    printf("Closed TCP connection to remote\n");
    ctx->pending = PINGPONG_RECV_WRID;

    if (servername)
    {
        if (pp_post_send(ctx))
        {
            fprintf(stderr, "Couldn't post send\n");
            return (void *) 1;
        }
        ctx->pending |= PINGPONG_SEND_WRID;
    }

    if (gettimeofday(&start, NULL))
    {
        perror("gettimeofday");
        return (void *) 1;
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
                return (void *) 1;
            }

            ++num_cq_events;

            if (ev_cq != ctx->cq)
            {
                fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
                return (void *) 1;
            }

            if (ibv_req_notify_cq(ctx->cq, 0))
            {
                fprintf(stderr, "Couldn't request CQ notification\n");
                return (void *) 1;
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
                    return (void *) 1;
                }

            } while (!use_event && ne < 1);

            for (i = 0; i < ne; ++i)
            {
                if (wc[i].status != IBV_WC_SUCCESS)
                {
                    fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                            ibv_wc_status_str(wc[i].status), wc[i].status,
                            (int) wc[i].wr_id);
                    return (void *) 1;
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
                                fprintf(stderr, "Couldn't post receive (%d)\n",
                                        routs);
                                return (void *) 1;
                            }
                        }

                        ++rcnt;
                        break;

                    default:
                        fprintf(stderr, "Completion for unknown wr_id %d\n",
                                (int) wc[i].wr_id);
                        return (void *) 1;
                }

                ctx->pending &= ~(int) wc[i].wr_id;
                if (scnt < iters && !ctx->pending)
                {
                    if (pp_post_send(ctx))
                    {
                        fprintf(stderr, "Couldn't post send\n");
                        return (void *) 1;
                    }
                    ctx->pending = PINGPONG_RECV_WRID | PINGPONG_SEND_WRID;
                }
            }
        }
    }

    if (gettimeofday(&end, NULL))
    {
        perror("gettimeofday");
        return (void *) 1;
    }


    ibv_ack_cq_events(ctx->cq, num_cq_events);

    if (pp_close_ctx(ctx))
    {
        return (void *) 1;
    }

    ibv_free_device_list(dev_list);
    free(rem_dest);

    {
        float *usec = malloc(sizeof(float));
        *usec = ((end.tv_sec - start.tv_sec) * 1000000 +
                 (end.tv_usec - start.tv_usec));
        long long bytes = (long long) size * iters * 2;

        printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n", bytes,
               *usec / 1000000., bytes * 8. / *usec);
        printf("%d iters in %.2f seconds = %.2f usec/iter\n", iters,
               *usec / 1000000., *usec / iters);
        printf("Calculate total time of work\n");
        free(usec);
        double *timeForCSV = malloc(sizeof(double));
        *timeForCSV = timeDifference(start,end);

        return (void *) timeForCSV;
    }

    return 0;
}


int main(int argc, char *argv[])
{
    int is_server = 0;
    //Usage first param if not null is num of threads, second is port,
    // third is servername
    printf("Num of args %d\n", argc);
    if (argc < 3 || argc > 4)
    {
        printf("Bad usage - please enter max num of threads, port and if "
                       "client - servername\n");
        return 1;
    }
    pthread_t *pthread;
    Commands commands;

    int maxThreadNum = atoi(argv[1]);

    commands.port = atoi(argv[2]);
    if (commands.port < 0 || commands.port > 65535)
    {
        return 1;
    }
    printf("new port - %d\n", commands.port);


    commands.servername = NULL;
    if (argc == 4)
    {
        commands.servername = argv[3];
        printf("ServerAddress %s\n", commands.servername);
    }
    commands.connfd = -1;
    if(!commands.servername){
        is_server = 1;
    }
    double results[1000] = {0.0};
    int resultIndex = 0;
    for (int varsize = 1; varsize <= MAX_MSG_SIZE; varsize *= 2)
    {
        printf("starting round with size- %d\n", varsize);


        for (int numThreads = 1; numThreads <= maxThreadNum; numThreads *= 2)
        {
            printf("starting round with %d threads\n", (numThreads));

            int *sizePerPeer = calloc(numThreads, sizeof(int));
            int regularSize = (int) floor(varsize / numThreads);
            for(int i = 1; i < numThreads; i++) {
                sizePerPeer[i] = regularSize;
            }
            sizePerPeer[0] = (varsize - (regularSize * (numThreads - 1)));


            printf("Size per regular thread %d\n", regularSize);
            printf("Size per first thread %d\n", sizePerPeer[0]);

            pthread = (pthread_t *) calloc(numThreads, sizeof(pthread_t));


            if (is_server)
            {
                int sockfd;
                sockfd = createServerConnection(commands.port);
                printf("started server bind\n");
                int thread = 0;
                while(thread < numThreads){
                    Commands *newCommands;
                    newCommands = calloc(0, sizeof(Commands));
                    newCommands->port = commands.port;
                    newCommands->servername = commands.servername;
                    newCommands->size = sizePerPeer[thread];
                    if(newCommands->size == 0){
                        printf("Payload size is zero- skipping thread\n");
                        thread++;
                        continue;
                    }
                    newCommands->connfd = acceptClientConnection(sockfd);
                    printf("new confd - %d\n", (*newCommands).connfd);
                    if((*newCommands).connfd >= 0){
                        pthread_create(&pthread[thread], NULL, runPingPong,
                                      newCommands);
                        thread++;
                    } else {
                        fprintf(stderr, "Failed creating connection\n");
                        return 1;
                    }
                }
                printf("Close FD\n");
                close(sockfd); //TODO:


            } else
            {

                for (int thread = 0; thread < numThreads; thread++)
                {

                    Commands *newCommands;
                    newCommands = calloc(0, sizeof(Commands));
                    newCommands->port = commands.port;
                    newCommands->servername = commands.servername;
                    newCommands->size = sizePerPeer[thread];
                    newCommands->threadNum = 0;
                    if(newCommands->size == 0){
                        printf("Payload size is zero- skipping thread\n");
                        thread++;
                        continue;
                    }

                    int sockfd = createClientSocketConnection(commands.port,
                                                              commands.servername);
                    if (sockfd== -1) {
                        fprintf(stderr, "Error creating socket\n");
                        return 1;
                    }

                    (*newCommands).connfd = sockfd;
                    printf("new connection fd - %d\n", (*newCommands).connfd);
                    pthread_create(&pthread[thread], NULL, runPingPong,
                                   newCommands);

                }

            }


            void *result;
            double maxThreadTime = DEFAULT_THREAD_TIME;
            for (int thread = 0; thread < numThreads; thread++)
            {
                double curTime = 0.0;
                if(pthread[thread] == NULL){
                    continue;
                }
                pthread_join(pthread[thread], &result);
                curTime = *((double *)result);
                printf("Time: %f\n",curTime);
                if(maxThreadTime < curTime){
                    maxThreadTime = curTime;
                }

            }
            free(sizePerPeer);
            free(pthread);
            double rtt = calcAverageRTT(1,DEFAULT_NUM_OF_MSGS*numThreads, maxThreadTime);
            double packetRate = calcAveragePacketRate(DEFAULT_NUM_OF_MSGS*numThreads,
                                                      maxThreadTime);
            double throughput = calcAverageThroughput(DEFAULT_NUM_OF_MSGS*numThreads,
                                                      varsize,maxThreadTime);
            double numOfSockets = numThreads;
            printf("avgRTT: %g\n", rtt);
            printf("avgPacketRate: %g\n", packetRate);
            printf("avgThroughput: %g\n", throughput);
            resultIndex = saveResults(rtt,throughput,packetRate,resultIndex,
                                      results,numOfSockets,varsize,numThreads*DEFAULT_NUM_OF_MSGS);

        }
    }
    char* filename = "InfinbandMultiSingleThread.csv";
    createCSV(filename,results,1000);
}

int createClientSocketConnection(int port, char *servername)
{
    struct addrinfo *res, *t;
    struct addrinfo
            hints = {.ai_family   = AF_INET, .ai_socktype = SOCK_STREAM};
    char *service;
    int n;
    int sockfd = -1;

    if (asprintf(&service, "%d", port) < 0) {
        return -1;
    }

    n = getaddrinfo(servername, service, &hints, &res);

    if (n < 0) {
        fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername, port);
        free(service);
        return -1;
    }

    for (int numsec = 1; numsec <= CLIENT_TO_SERVER_RETRY_TIMEOUT; numsec <<= 1) {

        for (t = res; t; t = t->ai_next) {
            sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
            if (sockfd >= 0) {
                if (!connect(sockfd, t->ai_addr, t->ai_addrlen)) {
                    break;
                }
                close(sockfd);
                sockfd = -1;
            }
        }

        if (sockfd >= 0) {
            break;
        }

        /*
        * Delay before trying again.
        */
        if (numsec <= CLIENT_TO_SERVER_RETRY_TIMEOUT) {
            sleep(numsec);
            printf("Retrying to connect to server after %d sec\n", numsec);
        }
    }


    freeaddrinfo(res);
    free(service);
    if (sockfd < 0) {
        fprintf(stderr, "Couldn't connect to %s:%d\n", servername, port);
        return -1;
    }
    return sockfd;
}