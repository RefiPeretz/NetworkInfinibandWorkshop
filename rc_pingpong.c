#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdlib.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include "MetricsIBV.h"


#include "pingpong.h"
#include "stack.h"

#define MAX_KEY 10

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
    int size;
    int rx_depth;
    struct ibv_port_attr portinfo;

};

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
                    const struct pingpong_dest *my_dest)
{
    struct addrinfo *res, *t;
    struct addrinfo
            hints = {.ai_family   = AF_INET, .ai_socktype = SOCK_STREAM};
    char *service;
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    int sockfd = -1;
    struct pingpong_dest *rem_dest = NULL;
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
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn,
            gid);
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

    rem_dest = malloc(sizeof *rem_dest);
    if (!rem_dest)
    {
        goto out;
    }

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn,
           gid);
    wire_gid_to_gid(gid, &rem_dest->gid);

    out:
    close(sockfd);
    return rem_dest;
}

static struct pingpong_dest *
pp_server_exch_dest(struct pingpong_context *ctx, int ib_port, enum ibv_mtu mtu,
                    int port, int sl, const struct pingpong_dest *my_dest,
                    int sgid_idx)
{
    struct addrinfo *res, *t;
    struct addrinfo hints =
            {.ai_flags    = AI_PASSIVE, .ai_family   = AF_INET, .ai_socktype = SOCK_STREAM};
    char *service;
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    int sockfd = -1, connfd;
    struct pingpong_dest *rem_dest = NULL;
    char gid[33];

    if (asprintf(&service, "%d", port) < 0)
    {
        return NULL;
    }

    n = getaddrinfo(NULL, service, &hints, &res);

    if (n < 0)
    {
        fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
        free(service);
        return NULL;
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
        return NULL;
    }

    listen(sockfd, 1);
    connfd = accept(sockfd, NULL, 0);
    close(sockfd);
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

#include <sys/param.h>

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



    ctx->cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL, ctx->channel, 0);
    if (!ctx->cq)
    {
        fprintf(stderr, "Couldn't create CQ\n");
        return NULL;
    }

    {
        struct ibv_qp_init_attr attr = {.send_cq = ctx->cq, .recv_cq = ctx
                ->cq, .cap     = {.max_send_wr  = rx_depth, .max_recv_wr  = rx_depth, .max_send_sge = 1, .max_recv_sge = 1}, .qp_type = IBV_QPT_RC};

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

    free(ctx);

    return 0;
}

static int
cstm_post_send(struct ibv_pd *pd, struct ibv_qp *qp, char *buf, int length)
{
    struct ibv_mr *mr = ibv_reg_mr(pd, buf, length, IBV_ACCESS_LOCAL_WRITE);
    if (!mr)
    {
        fprintf(stderr, "Couldn't register MR\n");
        return 1;
    }
    struct ibv_sge list =
            {.addr    = (uintptr_t) buf, .length = length, .lkey    = mr->lkey};
    struct ibv_send_wr wr =
            {.wr_id        = PINGPONG_SEND_WRID, .sg_list    = &list, .num_sge    = 1, .opcode = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED,};
    struct ibv_send_wr *bad_wr;

    return ibv_post_send(qp, &wr, &bad_wr);
}

static int
cstm_post_recv(struct ibv_pd *pd, struct ibv_qp *qp, char *buf, int length)
{
    struct ibv_mr *mr = ibv_reg_mr(pd, buf, length, IBV_ACCESS_LOCAL_WRITE);
    if (!mr)
    {
        fprintf(stderr, "Couldn't register MR\n");
        return 1;
    }
    struct ibv_sge list =
            {.addr    = (uintptr_t) buf, .length = length, .lkey    = mr->lkey};
    struct ibv_recv_wr wr =
            {.wr_id        = PINGPONG_RECV_WRID, .sg_list    = &list, .num_sge    = 1,};
    struct ibv_recv_wr *bad_wr;

    return ibv_post_recv(qp, &wr, &bad_wr);
}

static void usage(const char *argv0)
{
    printf("Usage:\n");
    printf("  %s            start a server and wait for connection\n", argv0);
    printf("  %s <host>     connect to server at <host>\n", argv0);
    printf("\n");
    printf("Options:\n");
    printf("  -p, --port=<port>      listen on/connect to port <port> (default 18515)\n");
    printf("  -d, --ib-dev=<dev>     use IB device <dev> (default first device found)\n");
    printf("  -i, --ib-port=<port>   use port <port> of IB device (default 1)\n");
    printf("  -s, --size=<size>      size of message to exchange (default 4096)\n");
    printf("  -m, --mtu=<size>       path MTU (default 1024)\n");
    printf("  -r, --rx-depth=<dep>   number of receives to post at a time (default 500)\n");
    printf("  -n, --iters=<iters>    number of exchanges (default 1000)\n");
    printf("  -l, --sl=<sl>          service level value\n");
    printf("  -e, --events           sleep on CQ events (default poll)\n");
    printf("  -g, --gid-idx=<gid index> local port gid index\n");
}

typedef enum kv_cmd
{
    SET_CMD = 3, GET_CMD = 4,
} kv_cmd;

typedef struct kvMsg
{
     char *key;
     char *value;
} kvMsg;

typedef struct handle
{
    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    struct pingpong_context *ctx;
    int defMsgSize;
    int kvListSize;
    int ib_port;
    int rx_depth;
    struct pingpong_dest my_dest;
    struct pingpong_dest *rem_dest;
    char gid[33];
    kvMsg **kvMsgDict;
} handle;

int getFromStore(handle *store, const char *key, char **value)
{
    int listSize = store->kvListSize;
    for(int i=0; i < listSize; i++){
        if(strcmp(store->kvMsgDict[i]->key, key) == 0){
            *value = malloc(strlen(store->kvMsgDict[i]->value)+1);
            memcpy(*value, store->kvMsgDict[i]->value, strlen
                    (store->kvMsgDict[i]->value)+1);
            return 0;
        }
    }

    return 1; // Didn't find any suitable key
}

void addElement(const char* key, char* value,struct handle* curHandle)
{

    if (!curHandle->kvListSize)
    {
        struct kvMsg *curMsg = calloc(1, sizeof(kvMsg));
        curMsg->key = malloc(strlen(key) + 1);
        curMsg->value = malloc(strlen(value) + 1);

        strcpy(curMsg->key, key);
        strcpy(curMsg->value, value);
        curHandle->kvMsgDict = malloc(sizeof(kvMsg) * 1);
        curHandle->kvMsgDict[0] = curMsg;
        curHandle->kvListSize++;
    } else
    {
        for (int i = 0; i < curHandle->kvListSize; i++)
        {
            if (strcmp(curHandle->kvMsgDict[i]->key, key) == 0)
            {
                strcpy(curHandle->kvMsgDict[i]->value, value);
                return;
            }
        }
        struct kvMsg *curMsg = calloc(1, sizeof(kvMsg));
        curMsg->key = malloc(strlen(key) + 1);
        curMsg->value = malloc(strlen(value) + 1);
        strcpy(curMsg->key, key);
        strcpy(curMsg->value, value);
        curHandle->kvListSize++;


        struct kvMsg **newList = malloc(sizeof(kvMsg) * curHandle->kvListSize);
        for (int i = 0; i < curHandle->kvListSize - 1; i++)
        {
            newList[i] = curHandle->kvMsgDict[i];
        }
        newList[curHandle->kvListSize - 1] = curMsg;
        free(curHandle->kvMsgDict);
        curHandle->kvMsgDict = newList;
    }
}

int kv_open(char *servername, void **kv_handle)
{
    handle *kvHandle = *kv_handle;
    kvHandle->dev_list = ibv_get_device_list(NULL);
    if (!kvHandle->dev_list)
    {
        perror("Failed to get IB devices list");
        return 1;
    }

    kvHandle->ib_dev = *kvHandle->dev_list;
    if (!kvHandle->ib_dev)
    {
        fprintf(stderr, "No IB devices found\n");
        return 1;
    }


    kvHandle->ctx = pp_init_ctx(kvHandle->ib_dev, kvHandle->defMsgSize, 500,
                                kvHandle->ib_port, kvHandle->rx_depth,
                                !servername);
    if (!kvHandle->ctx)
    {
        return 1;
    }
    return 0;
};


int kv_set(void *kv_handle, const char *key, const char *value)
{
    handle *kvHandle = kv_handle;
    kv_cmd cmd = SET_CMD;
    char *msg = malloc(roundup(kvHandle->defMsgSize, page_size));
    sprintf(msg, "%d:%s:%s", cmd, key, value);
    printf("Sending set msg: %s with size %d\n", msg, strlen(msg) + 1);

    if (cstm_post_send(kvHandle->ctx->pd, kvHandle->ctx->qp, msg,
                       strlen(msg) + 1))
    {
        perror("Couldn't post send: ");
        return 1;
    }


    int scnt = 0;
    while (scnt == 0)
    {
        struct ibv_wc wc[2];
        int ne;
        do
        {
            ne = ibv_poll_cq(kvHandle->ctx->cq, 2, wc);
            if (ne < 0)
            {
                fprintf(stderr, "poll CQ failed %d\n", ne);
                return 1;
            }

        } while (ne < 1);

        for (int i = 0; i < ne; ++i)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                        ibv_wc_status_str(wc[i].status), wc[i].status,
                        (int) wc[i].wr_id);
                return 1;
            }
            if (wc[i].wr_id == PINGPONG_SEND_WRID)
            {
                scnt++;
                break;
            }
        }
    }


    return 0;
};


int kv_get(void *kv_handle, const char *key, char **value)
{
    handle *kvHandle = kv_handle;
    kv_cmd cmd = GET_CMD;
    char *vmsg = malloc(roundup(kvHandle->defMsgSize, page_size));
    sprintf(vmsg, "%d:%s:%s", cmd, key, "");
    printf("Sending get msg: %s\n", vmsg);
    if (cstm_post_send(kvHandle->ctx->pd, kvHandle->ctx->qp, vmsg,
                       strlen(vmsg) + 1))
    {
        perror("Couldn't post send: ");
        return 1;
    }
    char *recv2Msg1;
    recv2Msg1 = malloc(roundup(kvHandle->defMsgSize,
                             page_size));
    if ((cstm_post_recv(kvHandle->ctx->pd, kvHandle->ctx->qp, recv2Msg1, roundup(kvHandle->defMsgSize, page_size))) <
        0) {
        perror("Couldn't post receive:");
        return 1;
    }
    printf("Pooling for result value \n");
    char *recv2Msg;
    int scnt = 1, recved = 1;
    while (scnt || recved)
    {
        struct ibv_wc wc[2];
        int ne;
        do
        {
            ne = ibv_poll_cq(kvHandle->ctx->cq, 2, wc);
            if (ne < 0)
            {
                fprintf(stderr, "poll CQ failed %d\n", ne);
                return 1;
            }

        } while (ne < 1);

        for (int i = 0; i < ne; ++i)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                        ibv_wc_status_str(wc[i].status), wc[i].status,
                        (int) wc[i].wr_id);
                return 1;
            }

            recv2Msg = malloc(roundup(kvHandle->defMsgSize, page_size));
            switch ((int) wc[i].wr_id)
            {
                case PINGPONG_SEND_WRID:
                    scnt--;
                    break;

                case PINGPONG_RECV_WRID:

                    printf("Got msg: %s\n", recv2Msg1);

                    recved--;
                    break;

                default:
                    fprintf(stderr, "Completion for unknown wr_id %d\n",
                            (int) wc[i].wr_id);
                    return 1;
            }

        }

    }

    memcpy(*value, recv2Msg, strlen(recv2Msg) + 1);
    free(recv2Msg);
    return 0;
};

void kv_release(char *value)
{
    if(value != NULL){
        free(value);
    }
};

int kv_close(void *kv_handle)
{
    if (pp_close_ctx(((handle *) kv_handle)->ctx))
    {
        return 1;
    }

    ibv_free_device_list(((handle *) kv_handle)->dev_list);
    free(((handle *) kv_handle)->rem_dest);
    return 0;
};

int processClientCmd(handle *kv_handle, char *msg)
{
    printf("Processing message %s\n", msg);
    if(strlen(msg) == 0){
        fprintf(stderr, "Msg is empty!: %s\n",msg);
        return  0;
    }
    int cmd = 0;
    char *key;
    char *value;
    const char *delim = ":";
    cmd = atoi(strtok(msg, delim));
    key = strtok(NULL, delim);
    value = strtok(NULL, delim);



    if (cmd == SET_CMD)
    {
        addElement(key, value, kv_handle);

    } else if (cmd == GET_CMD)
    {
        char **retValue = malloc(sizeof(char*));

        int ret =  getFromStore(kv_handle, key, retValue);
        if(ret){
            fprintf(stderr, "Error in fetching value!\n");
            return 1;
        }
        printf("Sending value after 'get' msg: %s\n", *retValue);
        if (cstm_post_send(kv_handle->ctx->pd, kv_handle->ctx->qp, *retValue,
                           strlen(*retValue) + 1))
        {
            perror("Couldn't post send: ");
            return 1;
        }
    } else
    {
        fprintf(stderr, "Coudln't decide what's the msg! MsgCmd - %d\n", cmd);
    }

    return 0;
}


int main(int argc, char *argv[])
{
    struct timeval start, end;
    char *servername = NULL;
    int port = 65433;
    int ib_port = 1;
    int size = 4096;
    enum ibv_mtu mtu = IBV_MTU_1024;
    int iters = 1000;
    int use_event = 0;
    int rcnt, scnt;
    int num_cq_events = 0;
    int sl = 0;
    int gidx;
    srand48(getpid() * time(NULL));

    while (1)
    {
        int c;

        static struct option long_options[] =
                {{.name = "port", .has_arg = 1, .val = 'p'},
                 {.name = "ib-dev", .has_arg = 1, .val = 'd'},
                 {.name = "ib-port", .has_arg = 1, .val = 'i'},
                 {.name = "size", .has_arg = 1, .val = 's'},
                 {.name = "mtu", .has_arg = 1, .val = 'm'},
                 {.name = "iters", .has_arg = 1, .val = 'n'},
                 {.name = "sl", .has_arg = 1, .val = 'l'},
                 {.name = "events", .has_arg = 0, .val = 'e'},
                 {.name = "gid-idx", .has_arg = 1, .val = 'g'},
                 {0}};

        c = getopt_long(argc, argv, "p:d:i:s:m:r:n:l:eg:", long_options, NULL);
        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'p':
                port = strtol(optarg, NULL, 0);
                if (port < 0 || port > 65535)
                {
                    usage(argv[0]);
                    return 1;
                }
                break;

            case 'i':
                ib_port = strtol(optarg, NULL, 0);
                if (ib_port < 0)
                {
                    usage(argv[0]);
                    return 1;
                }
                break;

            case 's':
                size = strtol(optarg, NULL, 0);
                break;

            case 'm':
                mtu = pp_mtu_to_enum(strtol(optarg, NULL, 0));
                if (mtu < 0)
                {
                    usage(argv[0]);
                    return 1;
                }
                break;

            case 'n':
                iters = strtol(optarg, NULL, 0);
                break;

            case 'l':
                sl = strtol(optarg, NULL, 0);
                break;

            case 'e':
                ++use_event;
                break;

            default:
                usage(argv[0]);
                return 1;
        }
    }

    if (optind == argc - 1)
    {
        servername = strdup(argv[optind]);
    } else if (optind < argc)
    {
        usage(argv[0]);
        return 1;
    }

    handle *kvHandle = calloc(1, sizeof *kvHandle);
    handle **p_kvHandle = &kvHandle;
    kvHandle->defMsgSize = size;
    kvHandle->ib_port = ib_port;
    kvHandle->rx_depth = 500;
    gidx = -1;

    page_size = sysconf(_SC_PAGESIZE);
    kv_open(servername, (void **) p_kvHandle);



    if (pp_get_port_info(kvHandle->ctx->context, ib_port,
                         &kvHandle->ctx->portinfo))
    {
        fprintf(stderr, "Couldn't get port info\n");
        return 1;
    }

    kvHandle->my_dest.lid = kvHandle->ctx->portinfo.lid;
    if (kvHandle->ctx->portinfo.link_layer == IBV_LINK_LAYER_INFINIBAND &&
        !kvHandle->my_dest.lid)
    {
        fprintf(stderr, "Couldn't get local LID\n");
        return 1;
    }

    if (gidx >= 0)
    {
        if (ibv_query_gid(kvHandle->ctx->context, kvHandle->ib_port, gidx,
                          &kvHandle->my_dest.gid))
        {
            fprintf(stderr, "Could not get local gid for gid index %d\n", gidx);
            return 1;
        }
    } else
    {
        memset(&kvHandle->my_dest.gid, 0, sizeof kvHandle->my_dest.gid);
    }

    kvHandle->my_dest.qpn = kvHandle->ctx->qp->qp_num;
    kvHandle->my_dest.psn = lrand48() & 0xffffff;
    inet_ntop(AF_INET6, &kvHandle->my_dest.gid, kvHandle->gid,
              sizeof kvHandle->gid);
    printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
           kvHandle->my_dest.lid, kvHandle->my_dest.qpn, kvHandle->my_dest.psn,
           kvHandle->gid);


    if (servername)
    {
        kvHandle->rem_dest =
                pp_client_exch_dest(servername, port, &kvHandle->my_dest);
    } else
    {
        kvHandle->rem_dest =
                pp_server_exch_dest(kvHandle->ctx, ib_port, mtu, port, sl,
                                    &kvHandle->my_dest, gidx);
    }

    if (!kvHandle->rem_dest)
    {
        return 1;
    }

    inet_ntop(AF_INET6, &kvHandle->rem_dest->gid, kvHandle->gid,
              sizeof kvHandle->gid);
    printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
           kvHandle->rem_dest->lid, kvHandle->rem_dest->qpn,
           kvHandle->rem_dest->psn, kvHandle->gid);

    if (servername)
    {
        if (pp_connect_ctx(kvHandle->ctx, ib_port, kvHandle->my_dest.psn, mtu,
                           sl, kvHandle->rem_dest, gidx))
        {
            return 1;
        }
    }



    if (servername)
    {
        printf("Start test\n");
        char key[MAX_KEY];
        for(int i = 0; i < 2;i++){
            //first Test
            char* msg = malloc((1024 * sizeof(char)) + 1);
            memset(msg,'w', 1023);
            msg[1023] = '\0';
            sprintf(key, "test%d", i);
            if (kv_set(kvHandle, key, msg))
            {
                fprintf(stderr, "Couldn't post send\n");
                return 1;
            }
            free(msg);
        }
        for(int i = 0; i < 2;i++){
            char *returnedVal = malloc(kvHandle->defMsgSize);
            sprintf(key, "test%d", i);
            if (kv_get(kvHandle, key, &returnedVal))
            {
                fprintf(stderr, "Couldn't kv get the requested key\n");
                return 1;
            }
            kv_release(returnedVal);
        }


//        char *returnedVal = malloc(roundup(kvHandle->defMsgSize, page_size));
//        if (kv_get(kvHandle, key, &returnedVal))
//        {
//            fprintf(stderr, "Couldn't kv get the requested key\n");
//            return 1;
//        }
//        free(returnedVal);
//        kv_release(returnedVal);


//        //second Test
//        char key2[5] = "blue";
//        char value2[10] = "wedding2";
//
//        if (kv_set(kvHandle, key2, value2))
//        {
//            fprintf(stderr, "Couldn't post send\n");
//            return 1;
//        }
//
//
//        char *recvMsg1 = malloc(roundup(kvHandle->defMsgSize, page_size));
//        if (kv_get(kvHandle, key2, &recvMsg1))
//        {
//            fprintf(stderr, "Couldn't kv get the requested key\n");
//            return 1;
//        }

//        kv_release(recvMsg1);

    }

    if (!servername)
    {
        char *recvMsg = NULL;
        if (gettimeofday(&start, NULL))
        {
            perror("gettimeofday");
            return 1;
        }
        recvMsg = malloc(roundup(kvHandle->defMsgSize,
                                 page_size));

        if ((cstm_post_recv(kvHandle->ctx->pd, kvHandle->ctx->qp, recvMsg, roundup(kvHandle->defMsgSize, page_size))) <
            0) {
            perror("Couldn't post receive:");
            return 1;
        }


        rcnt = scnt = 0;
        while (rcnt < iters || scnt < iters)
        {

                struct ibv_wc wc[5];
                int ne, i;

                do
                {
                    ne = ibv_poll_cq(kvHandle->ctx->cq, 5, wc);
                    if (ne < 0)
                    {
                        fprintf(stderr, "poll CQ failed %d\n", ne);
                        return 1;
                    }

                } while (ne < 1);

                for (i = 0; i < ne; ++i)
                {
                    if (wc[i].status != IBV_WC_SUCCESS)
                    {
                        fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                                ibv_wc_status_str(wc[i].status), wc[i].status,
                                (int) wc[i].wr_id);
                        return 1;
                    }
                    switch ((int) wc[i].wr_id)
                    {
                        case PINGPONG_SEND_WRID:
                            ++scnt;
                            break;

                        case PINGPONG_RECV_WRID:
                            printf("Got msg: %s\n", recvMsg);
                            processClientCmd(kvHandle, recvMsg);
                            free(recvMsg);
                            recvMsg = malloc(roundup(kvHandle->defMsgSize,
                                                     page_size));

                            if ((cstm_post_recv(kvHandle->ctx->pd,
                                                kvHandle->ctx->qp, recvMsg,
                                                roundup(kvHandle->defMsgSize,
                                                        page_size))) < 0)
                            {
                                perror("Couldn't post receive:");
                                return 1;
                            }
                            ++rcnt;
                            break;

                        default:
                            fprintf(stderr, "Completion for unknown wr_id %d\n",
                                    (int) wc[i].wr_id);
                            return 1;
                    }
                }
        }

        if (gettimeofday(&end, NULL))
        {
            perror("gettimeofday");
            return 1;
        }

        {
            float usec = (end.tv_sec - start.tv_sec) * 1000000 +
                         (end.tv_usec - start.tv_usec);
            long long bytes = (long long) size * iters * 2;

            printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n", bytes,
                   usec / 1000000., bytes * 8. / usec);
            printf("%d iters in %.2f seconds = %.2f usec/iter\n", iters,
                   usec / 1000000., usec / iters);
        }
    }
    ibv_ack_cq_events(kvHandle->ctx->cq, num_cq_events);

    kv_close(kvHandle);

    return 0;
}


