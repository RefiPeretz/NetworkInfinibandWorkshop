#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/param.h>
#include <pthread.h>
#include <math.h>
#include "multistreamPPSupport.h"
#include "MetricsIBV.h"


#define CLIENT_TO_SERVER_RETRY_TIMEOUT 64 //Timeout to connect from client to server

enum {
    PINGPONG_RECV_WRID = 1, PINGPONG_SEND_WRID = 2,
};

static int page_size;

struct pingpong_context {
    struct ibv_context *context;
    struct ibv_comp_channel *channel;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    struct ibv_qp **qpArr;
    void *buf;
//    void **buf_arr;

    int size;
    int rx_depth;
    int *pending_qp;
    struct ibv_port_attr portinfo;
    int peerNum;
    int *sizePerQP;
};

typedef struct Commands {
    int port;
    int size;
    int connfd;
    int peerNum;
    char *servername;
} Commands;

struct pingpong_dest {
    int lid;
    int qpn;
    int psn;
    union ibv_gid gid;
};

static int
pp_connect_ctx_per_qp(struct ibv_qp *qp, int port, int my_psn, enum ibv_mtu mtu,
                      int sl, struct pingpong_dest *dest, int sgid_idx) {
    struct ibv_qp_attr attr =
            {.qp_state        = IBV_QPS_RTR, .path_mtu        = mtu, .dest_qp_num        = dest
                    ->qpn, .rq_psn            = dest
                    ->psn, .max_dest_rd_atomic    = 1, .min_rnr_timer        = 12, .ah_attr        = {.is_global    = 0, .dlid        = dest
                    ->lid, .sl        = sl, .src_path_bits    = 0, .port_num    = port}};

    if (dest->gid.global.interface_id) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = dest->gid;
        attr.ah_attr.grh.sgid_index = sgid_idx;
    }
    if (ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
                                 IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                                 IBV_QP_MAX_DEST_RD_ATOMIC |
                                 IBV_QP_MIN_RNR_TIMER)) {
        fprintf(stderr, "Failed to modify QP to RTR\n");
        return 1;
    }

    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = my_psn;
    attr.max_rd_atomic = 1;
    if (ibv_modify_qp(qp, &attr,
                      IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                      IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |
                      IBV_QP_MAX_QP_RD_ATOMIC)) {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }

    return 0;
}

int retryClientServerConnect(int sockfd, const struct sockaddr *addr,
                             socklen_t alen) {
    int numsec;

    /*
    * Try to connect with exponential backoff.
    */
    for (numsec = 1; numsec <= CLIENT_TO_SERVER_RETRY_TIMEOUT; numsec <<= 1) {
        if (connect(sockfd, addr, alen) == 0) {
            /*
            * Connection accepted.
            */
            return (sockfd);
        }
        /*
        * Delay before trying again.
        */
        if (numsec <= CLIENT_TO_SERVER_RETRY_TIMEOUT / 2) {
            sleep(numsec);
        }
    }
    return (-1);
}

int createClientSocketConnection(int port, char *servername) {
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

static int createServerConnection(int port) {
    struct addrinfo *res, *t;
    struct addrinfo hints =
            {.ai_flags    = AI_PASSIVE, .ai_family   = AF_INET, .ai_socktype = SOCK_STREAM};
    char *service;
    //        char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    int sockfd = -1;

    if (asprintf(&service, "%d", port) < 0) {
        return -1;
    }

    n = getaddrinfo(NULL, service, &hints, &res);

    if (n < 0) {
        fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
        free(service);
        return -1;
    }

    for (t = res; t; t = t->ai_next) {
        sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
        if (sockfd >= 0) {
            n = 1;

            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

            if (!bind(sockfd, t->ai_addr, t->ai_addrlen)) {
                break;
            }
            close(sockfd);
            sockfd = -1;
        }
    }

    freeaddrinfo(res);
    free(service);

    if (sockfd < 0) {
        fprintf(stderr, "Couldn't listen to port %d\n", port);
        return -1;
    }

    int listen1 = listen(sockfd, 10); //TODO: check for errors
    if (listen1 < 0) {
        perror("Couldn't listen \n");
        return -1;
    }


    return sockfd;
}

int acceptClientConnection(int sockfd) {
    int connfd;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_in);

    connfd = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_len);

    if (connfd < 0) {
        return -1;
    }
    return connfd;
}


static struct pingpong_dest *

pp_server_exch_dest(struct pingpong_context *ctx, int ib_port, enum ibv_mtu mtu,
                    int port, int sl, struct pingpong_dest *my_dest,
                    int sgid_idx, int connfd, struct ibv_qp *pQp) {
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    struct pingpong_dest *rem_dest = NULL;
    char gid[33];


    if (connfd < 0) {
        fprintf(stderr, "accept() failed\n");
        return NULL;
    }

    n = read(connfd, msg, sizeof msg);
    if (n != sizeof msg) {
        perror("server read");
        fprintf(stderr, "%d/%d: Couldn't read remote address\n", n,
                (int) sizeof msg);
        goto out;
    }

    printf("got %s from client\n", msg);

    rem_dest = malloc(sizeof *rem_dest);
    if (!rem_dest) {
        goto out;
    }

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn,
           gid);
    wire_gid_to_gid(gid, &rem_dest->gid);
    printf("got %s from client\n", msg);

    if (pp_connect_ctx_per_qp(pQp, ib_port, my_dest->psn, mtu, sl, rem_dest,
                              sgid_idx)) {
        fprintf(stderr, "Couldn't connect to remote QP\n");
        free(rem_dest);
        rem_dest = NULL;
        goto out;
    }


    gid_to_wire_gid(&my_dest->gid, gid);
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn,
            gid);
    if (write(connfd, msg, sizeof msg) != sizeof msg) {
        fprintf(stderr, "Couldn't send local address\n");
        free(rem_dest);
        rem_dest = NULL;
        goto out;
    }
    printf("Wrote %s to client\n", msg);

    read(connfd, msg, sizeof msg);
    printf("Read last msg from client %s\n", msg);

    out:
    return rem_dest;
}

static struct pingpong_context *
pp_init_ctx(struct ibv_device *ib_dev, int size, int rx_depth, int port,
            int use_event, int is_server, int peerNumber) {
    struct pingpong_context *ctx;

    ctx = calloc(1, sizeof *ctx);
    if (!ctx) {
        fprintf(stderr, "Couldn't allocate ctx.\n");
        return NULL;
    }

    ctx->peerNum = peerNumber;
    ctx->size = size;
    ctx->rx_depth = rx_depth;
    ctx->sizePerQP = calloc(ctx->peerNum, sizeof(int));
    int sizePerPeer = (int) floor(size / ctx->peerNum);
    for (int i = 1; i < ctx->peerNum; i++) {
        ctx->sizePerQP[i] = sizePerPeer;
    }
    ctx->sizePerQP[0] = (size - (sizePerPeer * (ctx->peerNum - 1)));

    ctx->buf = malloc(roundup(size, page_size));
    if (!ctx->buf) {
        fprintf(stderr, "Couldn't allocate (%d) qp work buf.\n", 0);
        return NULL;
    }

//    for(int i=0; i<ctx->peerNum; i++){
    memset(ctx->buf, 0x7b + is_server, ctx->size);
//    }

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
    } else {
        ctx->channel = NULL;
    }

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

    ctx->cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL, ctx->channel, 0);
    if (!ctx->cq) {
        fprintf(stderr, "Couldn't create CQ\n");
        return NULL;
    }

    {
        struct ibv_qp_init_attr init_attr = {.send_cq = ctx->cq, .recv_cq = ctx
                ->cq, .cap     = {.max_send_wr  = 1, .max_recv_wr  = rx_depth, .max_send_sge = 1, .max_recv_sge = 1}, .qp_type = IBV_QPT_RC};

        ctx->qpArr = (struct ibv_qp **) calloc(ctx->peerNum,
                                               sizeof(struct ibv_qp *));
        if (ctx->qpArr == NULL) {
            fprintf(stderr, "Failed to allocate qp");
            return NULL;
        }

        for (int i = 0; i < ctx->peerNum; i++) {
            ctx->qpArr[i] = ibv_create_qp(ctx->pd, &init_attr);
            if (!ctx->qpArr[i]) {
                fprintf(stderr, "Couldn't create QP\n");
                return NULL;
            }
        }
    }

    {
        struct ibv_qp_attr attr =
                {.qp_state        = IBV_QPS_INIT, .pkey_index      = 0, .port_num        = port, .qp_access_flags = 0};

        for (int i = 0; i < ctx->peerNum; i++) {
            if (ibv_modify_qp(ctx->qpArr[i], &attr,
                              IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
                              IBV_QP_ACCESS_FLAGS)) {
                fprintf(stderr, "Failed to modify QP to INIT\n");
                return NULL;
            }
        }

    }

    return ctx;
}

int pp_close_ctx(struct pingpong_context *ctx) {

    if (ctx->qpArr != NULL) {
        for (int i = 0; i < ctx->peerNum; i++) {
            if (ctx->qpArr[i] != NULL) {
                if (ibv_destroy_qp(ctx->qpArr[i])) {
                    fprintf(stderr, "Couldn't destroy QP\n");
                    return 1;
                }
            }
        }
        free(ctx->qpArr);
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

static int
pp_post_recv_pQp(struct pingpong_context *ctx, int n, struct ibv_qp *qp, int
qp_index) {
    int buf_offset = ctx->sizePerQP[qp_index] * qp_index;
    char *buf_ptr = ctx->buf + buf_offset;


    struct ibv_sge list = {.addr    = (uintptr_t) buf_ptr, .length = ctx
            ->sizePerQP[qp_index], .lkey    = ctx->mr->lkey};
    //TODO: Maybe we need this in ARR?
    struct ibv_recv_wr wr =
            {.wr_id        = PINGPONG_RECV_WRID, .sg_list    = &list, .num_sge    = 1,};
    struct ibv_recv_wr *bad_wr;
    int i = 0;

    for (i = 0; i < n; ++i) {
        if (ibv_post_recv(qp, &wr, &bad_wr)) {
            break;
        }
    }


    return i;
}

static int pp_post_send_qp(struct pingpong_context *ctx, struct ibv_qp *qp, int
qp_index) {
    int buf_offset = ctx->sizePerQP[qp_index] * qp_index;
    char *buf_ptr = ctx->buf + buf_offset;

    struct ibv_sge list = {.addr    = (uintptr_t) buf_ptr, .length = ctx
            ->sizePerQP[qp_index], .lkey    = ctx->mr->lkey}; //TODO: divide size by
    // qp number
    struct ibv_send_wr wr =
            {.wr_id        = PINGPONG_SEND_WRID, .sg_list    = &list, .num_sge    = 1, .opcode     = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED,};
    struct ibv_send_wr *bad_wr;
    //TODO: check how was recieved.
    return ibv_post_send(qp, &wr, &bad_wr);
}


void *runPingPong(void *commands1) {
    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    struct pingpong_context *ctx;
    struct pingpong_dest *my_dest_arr = NULL;
    struct pingpong_dest **rem_dest_arr = {NULL};

    struct timeval start, end;
//    char *ib_devname = NULL;
    char *servername = NULL;
    int port = 18515;
    int ib_port = 1;
    int size = 1048576;
    enum ibv_mtu mtu = IBV_MTU_1024;
    int rx_depth = 500;
    int iters = 1000;
    int use_event = 0;
//    int routs;
    int *routs_arr;
    int rcnt, scnt;
    int num_cq_events = 0;
    int sl = 0;
    int gidx = -1;
    char gid[33];
    int peersNum;
    int connfd;

    srand48(getpid() * time(NULL));

    port = ((Commands *) (commands1))->port;
    if (port < 0 || port > 65535) {
        return (void *) 1;
    }
    printf("new port - %d\n", port);

    printf(" size param - %d\n", ((Commands *) (commands1))->size);

    size = ((Commands *) (commands1))->size;
    printf("new size - %d\n", size);

    servername = ((Commands *) (commands1))->servername;
    printf("commands - %s\n", servername);

    connfd = ((Commands *) (commands1))->connfd;
    printf("new connection id - %d\n", connfd);

    peersNum = ((Commands *) (commands1))->peerNum;
    printf("peers number: %d\n", peersNum);

    page_size = sysconf(_SC_PAGESIZE);

    dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        perror("Failed to get IB devices list");
        return (void *) 1;
    }


    ib_dev = *dev_list;
    if (!ib_dev) {
        fprintf(stderr, "No IB devices found\n");
        return (void *) 1;
    }

    ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port, use_event, !servername,
                      peersNum);
    if (!ctx) {
        fprintf(stderr, "Failed creating connection");
        return (void *) 1;
    }

    routs_arr = (int *) calloc(ctx->peerNum, sizeof(int));
    for (int peerQp = 0; peerQp < ctx->peerNum; peerQp++) {
        routs_arr[peerQp] = pp_post_recv_pQp(ctx, ctx->rx_depth,
                                             ctx->qpArr[peerQp], peerQp);
        if (routs_arr[peerQp] < ctx->rx_depth) {
            fprintf(stderr, "Couldn't post receive (%d)\n", routs_arr[peerQp]);
            return (void *) 1;
        }
    }


    if (pp_get_port_info(ctx->context, ib_port, &ctx->portinfo)) {
        fprintf(stderr, "Couldn't get port info\n");
        return (void *) 1;
    }

    my_dest_arr = (struct pingpong_dest *) calloc(ctx->peerNum,
                                                  sizeof(struct pingpong_dest));
    for (int i = 0; i < ctx->peerNum; i++) {
        printf("Initializing LocalQP address of QP %d\n", (i + 1));
        my_dest_arr[i].lid = ctx->portinfo.lid;
        if (ctx->portinfo.link_layer == IBV_LINK_LAYER_INFINIBAND &&
            !my_dest_arr[i].lid) {
            fprintf(stderr, "Couldn't get local LID\n");
            return (void *) 1;
        }

        if (gidx >= 0) {
            if (ibv_query_gid(ctx->context, ib_port, gidx, &my_dest_arr[i].gid)) {
                fprintf(stderr, "Could not get local gid for gid index %d\n",
                        gidx);
                return (void *) 1;
            }
        } else {
            memset(&my_dest_arr[i].gid, 0, sizeof my_dest_arr[i].gid);
        }

        my_dest_arr[i].qpn = ctx->qpArr[i]->qp_num;
        my_dest_arr[i].psn = lrand48() & 0xffffff;
        inet_ntop(AF_INET6, &my_dest_arr[i].gid, gid, sizeof gid);
        printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
               my_dest_arr[i].lid, my_dest_arr[i].qpn, my_dest_arr[i].psn, gid);

    }
    rem_dest_arr = calloc(ctx->peerNum, sizeof(struct pingpong_dest));

    for (int k = 0; k < ctx->peerNum; k++) {
        if (servername) {
            rem_dest_arr[k] =
                    pp_client_exch_dest(servername, port, &my_dest_arr[k],
                                        connfd);
        } else {
            rem_dest_arr[k] = pp_server_exch_dest(ctx, ib_port, mtu, port, sl,
                                                  &my_dest_arr[k], gidx, connfd,
                                                  ctx->qpArr[k]);
        }
        printf("Finished setting %d remote<->local QP \n", k);
        if (!rem_dest_arr[k]) {
            fprintf(stderr, "Failed to init remdest of qp - %d\n", k);
            close(connfd);
            return (void *) 1;
        }


        inet_ntop(AF_INET6, &rem_dest_arr[k]->gid, gid, sizeof gid);
        printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
               rem_dest_arr[k]->lid, rem_dest_arr[k]->qpn, rem_dest_arr[k]->psn,
               gid);


        if (servername) {
            printf("Connecting client qp to server\n");
            if (pp_connect_ctx_per_qp(ctx->qpArr[k], ib_port,
                                      my_dest_arr[k].psn, mtu, sl,
                                      rem_dest_arr[k], gidx)) {
                return (void *) 1;
            }
        }

    }
    close(connfd);
    printf("Closed TCP connection to remote\n");
    ctx->pending_qp = calloc(ctx->peerNum, sizeof(int));
    for (int y = 0; y < ctx->peerNum; y++) {
        ctx->pending_qp[y] = PINGPONG_RECV_WRID;
    }


    if (servername) {
        for (int i = 0; i < ctx->peerNum; i++) {
            if (pp_post_send_qp(ctx, ctx->qpArr[i], i)) {
                fprintf(stderr, "Couldn't post send for %d qp\n", i);
                return (void *) 1;
            }

            ctx->pending_qp[i] |= PINGPONG_SEND_WRID;
        }
    }

    if (gettimeofday(&start, NULL)) {
        perror("gettimeofday");
        return (void *) 1;
    }

    rcnt = scnt = 0;
    while (rcnt < iters || scnt < iters) {

        struct ibv_wc *wc = (struct ibv_wc *) calloc(ctx->peerNum * 2,
                                                     sizeof(struct ibv_wc));
        int ne, i;

        do {
            ne = ibv_poll_cq(ctx->cq, (ctx->peerNum * 2), wc);
            if (ne < 0) {
                fprintf(stderr, "poll CQ failed %d\n", ne);
                return (void *) -1;
            }
        } while (ne < 1);

        for (i = 0; i < ne; ++i) {
            struct ibv_qp *currentQp = NULL;
            int currentQPquePlace = 0;
            //find wc matching qp
            for (currentQPquePlace = 0; currentQPquePlace < peersNum; currentQPquePlace++) {
                if (wc[i].qp_num == ctx->qpArr[currentQPquePlace]->qp_num) {
                    currentQp = ctx->qpArr[currentQPquePlace];
                    break;
                }
            }

            if (currentQp == NULL || currentQPquePlace >= peersNum) {
                fprintf(stderr, "couldn't connect wc to qp.");
                return (void *) -1;
            }
            //                printf("Found match between WC[%d] to qp num %d",i,currentQp->qp_num);

            if (wc[i].status != IBV_WC_SUCCESS) {
                fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                        ibv_wc_status_str(wc[i].status), wc[i].status,
                        (int) wc[i].wr_id);
                return (void *) -1;
            }

            switch ((int) wc[i].wr_id) {
                case PINGPONG_SEND_WRID:
                    ++scnt;
                    break;

                case PINGPONG_RECV_WRID:
                    if (--routs_arr[currentQPquePlace] <= 1) {
                        routs_arr[currentQPquePlace] += pp_post_recv_pQp(ctx,
                                                                         ctx->rx_depth - routs_arr[currentQPquePlace],
                                                                         currentQp, currentQPquePlace);

                        if (routs_arr[currentQPquePlace] < ctx->rx_depth) {
                            fprintf(stderr, "Couldn't post receive (%d)\n",
                                    routs_arr[currentQPquePlace]);
                            return (void *) -1;
                        }
                    }

                    ++rcnt;
                    break;

                default:
                    fprintf(stderr, "Completion for unknown wr_id %d\n",
                            (int) wc[i].wr_id);
                    return (void *) -1;
            }

            ctx->pending_qp[currentQPquePlace] &= ~(int) wc[i].wr_id;
            if (scnt < iters && !ctx->pending_qp[currentQPquePlace]) {
                if (pp_post_send_qp(ctx, currentQp, currentQPquePlace)) {
                    fprintf(stderr, "Couldn't post send for qp %d\n",
                            currentQp->qp_num);
                    return (void *) -1;
                }
                ctx->pending_qp[currentQPquePlace] = PINGPONG_RECV_WRID | PINGPONG_SEND_WRID;
            }
        }
        free(wc);
    }

    if (gettimeofday(&end, NULL)) {
        perror("gettimeofday");
        return (void *) 1;
    }


    ibv_ack_cq_events(ctx->cq, num_cq_events);

    if (pp_close_ctx(ctx)) {
        return (void *) 1;
    }

    ibv_free_device_list(dev_list);

    for (int p = 0; p < ctx->peerNum; p++)
    {
        free(rem_dest_arr[p]);
    }
    free(rem_dest_arr);// TODO:
    printf("Cleaning rem_dest \n");

    {
        float *usec = malloc(sizeof(float));
        *usec = ((end.tv_sec - start.tv_sec) * 1000000 +
                 (end.tv_usec - start.tv_usec));
        long long bytes = (long long) size * iters * 2;

        printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n", bytes,
               *usec / 1000000., (bytes * 8. / *usec) / ctx->peerNum);
        printf("%d iters in %.2f seconds = %.2f usec/iter\n", iters,
               *usec / 1000000., *usec / iters);
        printf("Free usec\n");
        free(usec);
        printf("Free usec done.\n");
        double *timeForCSV = malloc(sizeof(double));
        printf("Malloc usec.\n");
        *timeForCSV = timeDifference(start, end);
        printf("Malloc usec done.\n");

        return (void *) timeForCSV;
    }

    return 0;
}


int main(int argc, char *argv[]) {
    int is_server = 0;
    //Usage first param if not null is num of threads, second is port,
    // third is servername
    printf("Num of args %d\n", argc);
    if (argc < 4 || argc > 5) {
        printf("Bad usage - please enter max num of threads, port , number "
                       "of peers and if client - servername\n");
        return 1;
    }

    pthread_t *pthread;
    Commands commands;

    int maxThreadNum = atoi(argv[1]);

    commands.port = atoi(argv[2]);
    if (commands.port < 0 || commands.port > 65535) {
        printf("port should be between 0 and 65535\n");
        return 1;
    }
    printf("new port - %d\n", commands.port);

    commands.peerNum = atoi(argv[3]);
    if (commands.peerNum <= 0) {
        printf("peer number should be bigger then zero\n");
        return 1;
    }
    printf("Peer number - %d\n", commands.peerNum);


    commands.servername = NULL;
    if (argc == 5) {
        commands.servername = argv[4];
        printf("ServerAddress %s\n", commands.servername);
    }

    commands.connfd = -1;

    if (!commands.servername) {
        is_server = 1;
    }
    double results[3000] = {0.0};
    int resultIndex = 0;
    for (int varsize = 1; varsize <= MAX_MSG_SIZE; varsize *= 2) {
        printf("starting round with size- %d\n", varsize);
        commands.size = varsize;
        int numThreads = 1;
        for (int numOfQps = 1; numOfQps <= commands.peerNum; numOfQps++) {
            int sockfd;

            printf("starting round with %d QPs on size:%d\n", numOfQps,varsize);

            pthread = malloc(numThreads * sizeof(pthread_t));

            Commands *newCommands;
            newCommands = calloc(0, sizeof(Commands));
            if (is_server) {
                int thread = 0;
                sockfd = createServerConnection(commands.port);
                printf("started server bind\n");
                (*newCommands).port = commands.port;
                (*newCommands).servername = commands.servername;
                (*newCommands).size = commands.size;
                (*newCommands).peerNum = numOfQps;
                (*newCommands).connfd = acceptClientConnection(sockfd);
                printf("new connection fd - %d\n", (*newCommands).connfd);
                if ((*newCommands).connfd >= 0) {
                    pthread_create(&pthread[thread], NULL, runPingPong,
                                   newCommands);
                } else {
                    fprintf(stderr, "Couldn't create server socket");
                    break;
                }
                printf("Close FD\n");
                close(sockfd); //TODO:


            } else {
                int thread = 0;
                sockfd = createClientSocketConnection(commands.port,
                                                      commands.servername);
                if (sockfd == -1) {
                    fprintf(stderr, "Error creating socket\n");
                    return 1;
                }
                Commands *newCommands;
                newCommands = calloc(0, sizeof(Commands));
                (*newCommands).port = commands.port;
                (*newCommands).servername = commands.servername;
                (*newCommands).size = commands.size;
                (*newCommands).peerNum = numOfQps;
                (*newCommands).connfd = sockfd;
                printf("new connection fd - %d\n", (*newCommands).connfd);

                pthread_create(&pthread[thread], NULL, runPingPong,
                               newCommands);


            }


            void *result;
            double curTime = 0.0;
            int thread = 0;
            pthread_join(pthread[thread], &result);
            curTime = *((double *) result);
            printf("Time: %f\n", curTime);
            free(newCommands);
            free(pthread);
            double rtt = calcAverageRTT(numOfQps,DEFAULT_NUM_OF_MSGS, curTime);
            double packetRate = calcAveragePacketRate(DEFAULT_NUM_OF_MSGS,curTime)/numOfQps;
            double throughput = calcAverageThroughput(DEFAULT_NUM_OF_MSGS,varsize,curTime)/numOfQps;
            double numOfSockets = numOfQps;
            printf("avgRTT: %g\n", rtt);
            printf("avgPacketRate: %g\n", packetRate);
            printf("avgThroughput: %g\n", throughput);
            resultIndex = saveResults(rtt,throughput,packetRate,resultIndex,results,numOfSockets,varsize,DEFAULT_NUM_OF_MSGS);


        }
    }
    char* filename = "InfinbandMultiStream.csv";
    createCSV(filename,results,3000);
}