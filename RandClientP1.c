#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/mman.h>



#include "pingpong.h"

#define MAX_KEY 10
#define MAX_MSG_TEST 4096
#define LOOP_ITER 50
#define NUMBER_OF_BUFFERS 2
#define EAGER_PROTOCOL_LIMIT (1 << 22) /* 4KB limit */
#define USED_BUFFER 1
#define UN_USED_BUFFER 0
#define VERSION 23
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404


struct {
    char *ext;
    char *filetype;
} extensions [] = {
        {"gif", "image/gif" },
        {"jpg", "image/jpg" },
        {"jpeg","image/jpeg"},
        {"png", "image/png" },
        {"ico", "image/ico" },
        {"zip", "image/zip" },
        {"gz",  "image/gz"  },
        {"tar", "image/tar" },
        {"htm", "text/html" },
        {"html","text/html" },
        {0,0} };

void logger(int type, char *s1, char *s2, int socket_fd)
{
    int fd ;
    char logbuffer[BUFSIZE*2];

    switch (type) {
        case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid());
            break;
        case FORBIDDEN:
            (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
            (void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2);
            break;
        case NOTFOUND:
            (void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
            (void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2);
            break;
        case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,socket_fd); break;
    }
    /* No checks here, nothing can be done with a failure anyway */
    if((fd = open("nweb.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
        (void)write(fd,logbuffer,strlen(logbuffer));
        (void)write(fd,"\n",1);
        (void)close(fd);
    }
    if(type == ERROR || type == NOTFOUND || type == FORBIDDEN) exit(3);
}

/* this is a child web server process, so we can exit on errors */
void web(int fd, int hit)
{
    int j, file_fd, buflen;
    long i, ret, len;
    char * fstr;
    static char buffer[BUFSIZE+1]; /* static so zero filled */

    ret =read(fd,buffer,BUFSIZE); 	/* read Web request in one go */
    if(ret == 0 || ret == -1) {	/* read failure stop now */
        logger(FORBIDDEN,"failed to read browser request","",fd);
    }
    if(ret > 0 && ret < BUFSIZE)	/* return code is valid chars */
        buffer[ret]=0;		/* terminate the buffer */
    else buffer[0]=0;
    for(i=0;i<ret;i++)	/* remove CF and LF characters */
        if(buffer[i] == '\r' || buffer[i] == '\n')
            buffer[i]='*';
    logger(LOG,"request",buffer,hit);
    if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
        logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd);
    }
    for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
        if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
            buffer[i] = 0;
            break;
        }
    }
    for(j=0;j<i-1;j++) 	/* check for illegal parent directory use .. */
        if(buffer[j] == '.' && buffer[j+1] == '.') {
            logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,fd);
        }
    if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) /* convert no filename to index file */
        (void)strcpy(buffer,"GET /index.html");

    /* work out the file type and check we support it */
    buflen=strlen(buffer);
    fstr = (char *)0;
    for(i=0;extensions[i].ext != 0;i++) {
        len = strlen(extensions[i].ext);
        if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
            fstr =extensions[i].filetype;
            break;
        }
    }
    if(fstr == 0) logger(FORBIDDEN,"file extension type not supported",buffer,fd);

    if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {  /* open the file for reading */
        logger(NOTFOUND, "failed to open file",&buffer[5],fd);
    }
    logger(LOG,"SEND",&buffer[5],hit);
    len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
    (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
    (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */
    logger(LOG,"Header",buffer,hit);
    (void)write(fd,buffer,strlen(buffer));

    /* send file in 8KB block - last block may be smaller */
    while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
        (void)write(fd,buffer,ret);
    }
    sleep(1);	/* allow socket to drain before signalling the socket is closed */
    close(fd);
    exit(1);
}

/**
 * allocate free buffer
 * @param kv_id
 * @param usedBuffer
 * @return
 */
typedef struct Message {
    uint32_t mr_rkey;
    long unsigned addr;
    int valueSize;
} Message;

typedef struct kvMsg {
    char *key;
    char *value;
} kvMsg;
typedef struct pingpong_dest {
    int lid;
    int qpn;
    int psn;
    union ibv_gid gid;
} pingpong_dest;
typedef struct handle {
    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    struct pingpong_context *ctx;
    long defMsgSize;
    int kvListSize;
    int ib_port;
    int rx_depth;
    int usedBuffers;
    struct pingpong_dest my_dest;
    struct pingpong_dest *rem_dest;
    char gid[33];
    int isUsed[NUMBER_OF_BUFFERS];
    kvMsg **kvMsgDict;
} handle;

typedef struct mkv_handle {
    unsigned num_servers;
    struct handle *kv_handle[0];
    int defMsgSize;
    int kvListSize;
    int ib_port;
    int rx_depth;
    struct pingpong_dest my_dest;
    struct pingpong_dest *rem_dest;
    char gid[33];
    kvMsg **kvMsgDict;
    char *clientBuffers;
    int clientBuffersNum;
} mkv_handle;

struct dkv_ctx {
    struct mkv_handle *mkv;
    struct handle *indexer;
};
struct kv_server_address {
    char *servername; /* In the last item of an array this is NULL */
    unsigned int port; /* This is useful for multiple servers on a host */
};

enum {
    PINGPONG_RECV_WRID = 1, PINGPONG_SEND_WRID = 2,
};
int g_argc;
char **g_argv;

static int page_size;

struct pingpong_context {
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


#define EAGER_BUFFER_LIMIT (10)

struct kv_client_eager_buffer {
    unsigned kv_id; /* which connection does this buffer belong to */
    char data[EAGER_PROTOCOL_LIMIT]; /* give only this to ibv_post_recv() or the user */
};

static int
pp_connect_ctx(struct pingpong_context *ctx, int port, int my_psn, enum ibv_mtu mtu, int sl, struct pingpong_dest *dest,
               int sgid_idx) {
    struct ibv_qp_attr attr = {.qp_state        = IBV_QPS_RTR, .path_mtu        = mtu, .dest_qp_num        = dest
            ->qpn, .rq_psn            = dest
            ->psn, .max_dest_rd_atomic    = 1, .min_rnr_timer        = 12, .ah_attr        = {.is_global    = 0, .dlid        = dest
            ->lid, .sl        = sl, .src_path_bits    = 0, .port_num    = port}};

    if (dest->gid.global.interface_id) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = dest->gid;
        attr.ah_attr.grh.sgid_index = sgid_idx;
    }
    if (ibv_modify_qp(ctx->qp, &attr, IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                                      IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER)) {
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
                      IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |
                      IBV_QP_MAX_QP_RD_ATOMIC)) {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }

    return 0;
}

static struct pingpong_dest *
pp_client_exch_dest(const char *servername, int port, const struct pingpong_dest *my_dest) {
    struct addrinfo *res, *t;
    struct addrinfo hints = {.ai_family   = AF_INET, .ai_socktype = SOCK_STREAM};
    char *service;
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    int sockfd = -1;
    struct pingpong_dest *rem_dest = NULL;
    char gid[33];

    if (asprintf(&service, "%d", port) < 0) {
        return NULL;
    }

    n = getaddrinfo(servername, service, &hints, &res);

    if (n < 0) {
        fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername, port);
        free(service);
        return NULL;
    }

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

    freeaddrinfo(res);
    free(service);

    if (sockfd < 0) {
        fprintf(stderr, "Couldn't connect to %s:%d\n", servername, port);
        return NULL;
    }

    gid_to_wire_gid(&my_dest->gid, gid);
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn, gid);
    if (write(sockfd, msg, sizeof msg) != sizeof msg) {
        fprintf(stderr, "Couldn't send local address\n");
        goto out;
    }

    if (read(sockfd, msg, sizeof msg) != sizeof msg) {
        perror("client read");
        fprintf(stderr, "Couldn't read remote address\n");
        goto out;
    }

    write(sockfd, "done", sizeof "done");

    rem_dest = malloc(sizeof *rem_dest);
    if (!rem_dest) {
        goto out;
    }

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn, gid);
    wire_gid_to_gid(gid, &rem_dest->gid);

    out:
    close(sockfd);
    return rem_dest;
}

static struct pingpong_dest *
pp_server_exch_dest(struct pingpong_context *ctx, int ib_port, enum ibv_mtu mtu, int port, int sl,
                    const struct pingpong_dest *my_dest, int sgid_idx) {
    struct addrinfo *res, *t;
    struct addrinfo hints = {.ai_flags    = AI_PASSIVE, .ai_family   = AF_INET, .ai_socktype = SOCK_STREAM};
    char *service;
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    int sockfd = -1, connfd;
    struct pingpong_dest *rem_dest = NULL;
    char gid[33];

    if (asprintf(&service, "%d", port) < 0) {
        return NULL;
    }

    n = getaddrinfo(NULL, service, &hints, &res);

    if (n < 0) {
        fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
        free(service);
        return NULL;
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
        return NULL;
    }

    listen(sockfd, 1);
    connfd = accept(sockfd, NULL, 0);
    close(sockfd);
    if (connfd < 0) {
        fprintf(stderr, "accept() failed\n");
        return NULL;
    }

    n = read(connfd, msg, sizeof msg);
    if (n != sizeof msg) {
        perror("server read");
        fprintf(stderr, "%d/%d: Couldn't read remote address\n", n, (int) sizeof msg);
        goto out;
    }

    rem_dest = malloc(sizeof *rem_dest);
    if (!rem_dest) {
        goto out;
    }

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn, gid);
    wire_gid_to_gid(gid, &rem_dest->gid);

    if (pp_connect_ctx(ctx, ib_port, my_dest->psn, mtu, sl, rem_dest, sgid_idx)) {
        fprintf(stderr, "Couldn't connect to remote QP\n");
        free(rem_dest);
        rem_dest = NULL;
        goto out;
    }


    gid_to_wire_gid(&my_dest->gid, gid);
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn, gid);
    if (write(connfd, msg, sizeof msg) != sizeof msg) {
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

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>

static struct pingpong_context *
pp_init_ctx(struct ibv_device *ib_dev, int size, int rx_depth, int port, int use_event, int is_server) {
    struct pingpong_context *ctx;

    ctx = calloc(1, sizeof *ctx);
    if (!ctx) {
        return NULL;
    }

    ctx->size = size;
    ctx->rx_depth = rx_depth;


    ctx->context = ibv_open_device(ib_dev);
    if (!ctx->context) {
        fprintf(stderr, "Couldn't get context for %s\n", ibv_get_device_name(ib_dev));
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


    ctx->cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL, ctx->channel, 0);
    if (!ctx->cq) {
        fprintf(stderr, "Couldn't create CQ\n");
        return NULL;
    }

    {
        struct ibv_qp_init_attr attr = {.send_cq = ctx->cq, .recv_cq = ctx
                ->cq, .cap     = {.max_send_wr  = rx_depth, .max_recv_wr  = rx_depth, .max_send_sge = 1, .max_recv_sge = 1}, .qp_type = IBV_QPT_RC};

        ctx->qp = ibv_create_qp(ctx->pd, &attr);
        if (!ctx->qp) {
            fprintf(stderr, "Couldn't create QP\n");
            return NULL;
        }
    }

    {
        struct ibv_qp_attr attr = {.qp_state        = IBV_QPS_INIT, .pkey_index      = 0, .port_num        = port, .qp_access_flags = 0};

        if (ibv_modify_qp(ctx->qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS)) {
            fprintf(stderr, "Failed to modify QP to INIT\n");
            return NULL;
        }
    }

    return ctx;
}


int pp_close_ctx(struct pingpong_context *ctx) {
    if (ibv_destroy_qp(ctx->qp)) {
        fprintf(stderr, "Couldn't destroy QP\n");
        return 1;
    }

    if (ibv_destroy_cq(ctx->cq)) {
        fprintf(stderr, "Couldn't destroy CQ\n");
        return 1;
    }

    //    if (ibv_dealloc_pd(ctx->pd))
    //    {
    //        fprintf(stderr, "Couldn't deallocate PD\n");
    //        return 1;
    //    }

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

    free(ctx);

    return 0;
}

static int cstm_post_send(struct ibv_pd *pd, struct ibv_qp *qp, char *buf, int length) {
    struct ibv_mr *mr = ibv_reg_mr(pd, buf, length, IBV_ACCESS_LOCAL_WRITE);
    if (!mr) {
        fprintf(stderr, "Couldn't register MR\n");
        return 1;
    }
    struct ibv_sge list = {.addr    = (uintptr_t) buf, .length = length, .lkey    = mr->lkey};
    struct ibv_send_wr wr = {.wr_id        = PINGPONG_SEND_WRID, .sg_list    = &list, .num_sge    = 1, .opcode = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED,};
    struct ibv_send_wr *bad_wr;

    return ibv_post_send(qp, &wr, &bad_wr);
}

static int cstm_post_recv(struct ibv_pd *pd, struct ibv_qp *qp, char *buf, int length) {
    struct ibv_mr *mr = ibv_reg_mr(pd, buf, length, IBV_ACCESS_LOCAL_WRITE);
    if (!mr) {
        fprintf(stderr, "Couldn't register MR\n");
        return 1;
    }
    struct ibv_sge list = {.addr    = (uintptr_t) buf, .length = length, .lkey    = mr->lkey};
    struct ibv_recv_wr wr = {.wr_id        = PINGPONG_RECV_WRID, .sg_list    = &list, .num_sge    = 1,};
    struct ibv_recv_wr *bad_wr;

    return ibv_post_recv(qp, &wr, &bad_wr);
}

static void usage(const char *argv0) {
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


int getFromStore(handle *store, const char *key, char **value) {
    int listSize = store->kvListSize;
    for (int i = 0; i < listSize; i++) {
        if (strcmp(store->kvMsgDict[i]->key, key) == 0) {
            *value = malloc(strlen(store->kvMsgDict[i]->value) + 1);
            memcpy(*value, store->kvMsgDict[i]->value, strlen(store->kvMsgDict[i]->value) + 1);
            return 0;
        }
    }

    return 1; // Didn't find any suitable key
}

void addElement(const char *key, char *value, struct handle *curHandle) {

    if (!curHandle->kvListSize) {
        struct kvMsg *curMsg = calloc(1, sizeof(kvMsg));
        curMsg->key = malloc(strlen(key) + 1);
        curMsg->value = malloc(strlen(value) + 1);

        strcpy(curMsg->key, key);
        strcpy(curMsg->value, value);
        curHandle->kvMsgDict = malloc(sizeof(kvMsg) * 1);
        curHandle->kvMsgDict[0] = curMsg;
        curHandle->kvListSize++;
    } else {
        for (int i = 0; i < curHandle->kvListSize; i++) {
            if (strcmp(curHandle->kvMsgDict[i]->key, key) == 0) {
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
        for (int i = 0; i < curHandle->kvListSize - 1; i++) {
            newList[i] = curHandle->kvMsgDict[i];
        }
        newList[curHandle->kvListSize - 1] = curMsg;
        free(curHandle->kvMsgDict);
        curHandle->kvMsgDict = newList;
    }
}

int kv_open(char *servername, void **kv_handle) {
    handle *kvHandle = *kv_handle;
    kvHandle->dev_list = ibv_get_device_list(NULL);
    if (!kvHandle->dev_list) {
        perror("Failed to get IB devices list");
        return 1;
    }

    kvHandle->ib_dev = *kvHandle->dev_list;
    if (!kvHandle->ib_dev) {
        fprintf(stderr, "No IB devices found\n");
        return 1;
    }


    kvHandle->ctx = pp_init_ctx(kvHandle->ib_dev, kvHandle->defMsgSize, 500, kvHandle->ib_port, kvHandle->rx_depth,
                                !servername);
    if (!kvHandle->ctx) {
        return 1;
    }
    return 0;
};

int mkv_set(void *mkv_h, unsigned kv_id, const char *key, const char *value, unsigned int length) {
    struct mkv_handle *m_handle = mkv_h;
    return kv_set(m_handle->kv_handle[kv_id], key, value, length);
}


int sendMsgLogic(handle *kv_handle, char *msg) {
    if (cstm_post_send(kv_handle->ctx->pd, kv_handle->ctx->qp, msg, strlen(msg) + 1)) {
        perror("Couldn't post send: ");
        return 1;
    }

    int scnt = 0;
    int iterations = 1000;
    while (scnt == 0 || iterations > 0) {
        struct ibv_wc wc[101];
        int ne;
        int pollIterations = 100;
        do {
            ne = ibv_poll_cq(kv_handle->ctx->cq, 101, wc);
            if (ne < 0) {
                fprintf(stderr, "poll CQ failed %d\n", ne);
                return 1;
            }
            pollIterations--;
        } while (ne < 1 && pollIterations > 0);

        for (int i = 0; i < ne; ++i) {
            if (wc[i].status != IBV_WC_SUCCESS) {
                fprintf(stderr, "Failed status %s (%d) for wr_id %d\n", ibv_wc_status_str(wc[i].status), wc[i].status,
                        (int) wc[i].wr_id);
                return 1;
            }
            if (wc[i].wr_id == PINGPONG_SEND_WRID) {
                scnt++;
                break;
            }
        }

        iterations--;
    }
    return 0;

}

int processServerRdmaWriteResponseCmd(handle *kv_handle, char *msg, char *actualMessage) {
    printf("Processing server message: %s\n", msg);
    if (strlen(msg) == 0) {
        fprintf(stderr, "Msg is empty!\n");
        return 0;
    }
    struct ibv_mr *sendMr = ibv_reg_mr(kv_handle->ctx->pd, actualMessage, strlen(actualMessage) + 1,
                                       IBV_ACCESS_LOCAL_WRITE);


    struct Message *mr_msg = (struct Message *) calloc(1, sizeof(struct Message));
    char *delim = ":";
    mr_msg->addr = strtoul(strtok(msg, delim), NULL, 10);
    mr_msg->mr_rkey = (uint32_t) atoi(strtok(NULL, delim));
    printf("Parsed server message - addr: %lu, rkey: %d\n", mr_msg->addr, mr_msg->mr_rkey);

    struct ibv_sge list = {.addr    = (uintptr_t) actualMessage, .length = strlen(actualMessage) + 1, .lkey = sendMr
            ->lkey};

    struct ibv_send_wr wr = {.wr_id        = PINGPONG_SEND_WRID, .sg_list    = &list, .num_sge    = 1, .opcode = IBV_WR_RDMA_WRITE, .send_flags = IBV_SEND_SIGNALED, .wr.rdma.remote_addr = (uintptr_t) mr_msg
            ->addr, .wr.rdma.rkey = mr_msg->mr_rkey};
    struct ibv_send_wr *bad_wr;

    if (ibv_post_send(kv_handle->ctx->qp, &wr, &bad_wr)) {
        printf("Error in processServerRdmaWriteResponseCmd");
    };

    return 0;
}


int kv_set(void *kv_handle, const char *key, const char *value, unsigned int length) {
    handle *kvHandle = kv_handle;
    kv_cmd cmd = SET_CMD;
    //first send msg to server with size and MR to read from.
    size_t actualMsgSize = roundup(strlen(value) + 1, page_size);
    char *actualMsg = (char *) malloc(actualMsgSize);
    sprintf(actualMsg, "%d:%s:%s", cmd, key, value);

    //first send msg to server with size and MR to read from.
    char *msg = (char *) malloc(roundup(kvHandle->defMsgSize, page_size));
    sprintf(msg, "%d:%s:%d", cmd, key, length);
    printf("Sending set msg: %s with size %d\n", msg, strlen(msg) + 1);

    if (cstm_post_send(kvHandle->ctx->pd, kvHandle->ctx->qp, msg, strlen(msg) + 1)) {
        perror("Couldn't post send: ");
        return 1;
    }

    char *remoteMrMsg = malloc(roundup(kvHandle->defMsgSize, page_size));
    if ((cstm_post_recv(kvHandle->ctx->pd, kvHandle->ctx->qp, remoteMrMsg, roundup(kvHandle->defMsgSize, page_size))) <
        0) {
        perror("Couldn't post receive:");
        return 1;
    }
    int scnt = 2, rcvd = 1;
    while (scnt || rcvd) {
        struct ibv_wc wc[5];
        int ne;
        do {
            ne = ibv_poll_cq(kvHandle->ctx->cq, 5, wc);
            if (ne < 0) {
                fprintf(stderr, "poll CQ failed %d\n", ne);
                return 1;
            }

        } while (ne < 1);

        for (int i = 0; i < ne; ++i) {
            if (wc[i].status != IBV_WC_SUCCESS) {
                fprintf(stderr, "Failed status %s (%d) for wr_id %d\n", ibv_wc_status_str(wc[i].status), wc[i].status,
                        (int) wc[i].wr_id);
                return 1;
            }
            if (wc[i].wr_id == PINGPONG_SEND_WRID) {

                scnt--;
            } else if (wc[i].wr_id == PINGPONG_RECV_WRID) {
                //now we should have gotten his the server MR and we should
                // write our actual message
                printf("Got server set result msg: %s\n", remoteMrMsg);
                processServerRdmaWriteResponseCmd(kv_handle, remoteMrMsg, value);
                rcvd--;
            } else {
                fprintf(stderr, "Wrong wr_id %d\n", (int) wc[i].wr_id);
                return 1;
            }
        }
    }

    return 0;
};

int processServerGetReqResponseCmd(handle *kv_handle, struct Message *msg, char **value) {
    struct ibv_mr *sendMr = ibv_reg_mr(kv_handle->ctx->pd, *value, msg->valueSize, IBV_ACCESS_LOCAL_WRITE);

    struct ibv_sge list = {.addr    = (uintptr_t) (*value), .length = msg->valueSize, .lkey = sendMr->lkey};
    struct ibv_send_wr wr = {.wr_id        = PINGPONG_SEND_WRID, .sg_list    = &list, .num_sge    = 1, .opcode = IBV_WR_RDMA_READ, .send_flags = IBV_SEND_SIGNALED, .wr.rdma.remote_addr = (uintptr_t) msg
            ->addr, .wr.rdma.rkey = msg->mr_rkey};
    struct ibv_send_wr *bad_wr;

    if (ibv_post_send(kv_handle->ctx->qp, &wr, &bad_wr)) {
        printf("processServerGetReqResponseCmd: Err during RDMA READ");
    };
    return 0;
}

int kv_get(void *kv_handle, const char *key, char **value, char *clientBuffers, int kv_id, unsigned int length) {
    handle *kvHandle = kv_handle;
    kv_cmd cmd = GET_CMD;
    struct Message *mr_msg;
    mr_msg = (struct Message *) calloc(1, sizeof(struct Message));
    char *vmsg = malloc(roundup(kvHandle->defMsgSize, page_size));
    sprintf(vmsg, "%d:%s:%s", cmd, key, "");
    printf("Sending get msg: %s\n", vmsg);
    if (cstm_post_send(kvHandle->ctx->pd, kvHandle->ctx->qp, vmsg, strlen(vmsg) + 1)) {
        perror("Couldn't post send: ");
        return 1;
    }
    char *recv2Msg1;
    if (kvHandle->usedBuffers < NUMBER_OF_BUFFERS) {
        int freeIndex = getFreeBufferIndex(kv_handle);
        recv2Msg1 = clientBuffers + (kv_id * MAX_MSG_TEST + freeIndex * MAX_MSG_TEST);
        kvHandle->usedBuffers++;
    } else {
        recv2Msg1 = calloc(1, 4096);
    }


    if ((cstm_post_recv(kvHandle->ctx->pd, kvHandle->ctx->qp, recv2Msg1, roundup(kvHandle->defMsgSize, page_size))) <
        0) {
        perror("Couldn't post receive:");
        return 1;
    }
    printf("Pooling for result value \n");
    char *recv2Msg;
    int scnt = 1, recved = 1;
    int iterations = 10000;
    while ((scnt || recved) && iterations > 0) {
        struct ibv_wc wc[2];
        int ne;
        int pollIterations = 10000;
        do {
            ne = ibv_poll_cq(kvHandle->ctx->cq, 2, wc);
            if (ne < 0) {
                fprintf(stderr, "poll CQ failed %d\n", ne);
                return 1;
            }
            pollIterations--;
        } while (ne < 1 && pollIterations > 0);

        for (int i = 0; i < ne; ++i) {
            if (wc[i].status != IBV_WC_SUCCESS) {
                fprintf(stderr, "Failed status %s (%d) for wr_id %d\n", ibv_wc_status_str(wc[i].status), wc[i].status,
                        (int) wc[i].wr_id);
                return 1;
            }

            recv2Msg = malloc(roundup(kvHandle->defMsgSize, page_size));
            switch ((int) wc[i].wr_id) {
                case PINGPONG_SEND_WRID:
                    scnt--;
                    break;

                case PINGPONG_RECV_WRID:
                    printf("Got server get prep msg: %s\n", recv2Msg1);
                    printf("Processing server Message: %s\n", recv2Msg1);
                    if (strlen(recv2Msg1) == 0) {
                        fprintf(stderr, "Msg is empty!\n");
                        return 0;
                    };
                    char *delim = ":";
                    mr_msg->addr = strtoul(strtok(recv2Msg1, delim), NULL, 10);
                    mr_msg->mr_rkey = atoi(strtok(NULL, delim));
                    mr_msg->valueSize = atoi(strtok(NULL, delim));
                    recved--;
                    break;


                default:
                    fprintf(stderr, "Completion for unknown wr_id %d\n", (int) wc[i].wr_id);
                    return 1;
            }

        }
        iterations--;

    }
    if (iterations != 0) {
        *value = calloc(1, mr_msg->valueSize * sizeof(char));
        processServerGetReqResponseCmd(kv_handle, mr_msg, value);
        int scntGetReqres = 1;
        while (scntGetReqres) {
            struct ibv_wc wc[2];
            int ne;
            do {
                ne = ibv_poll_cq(kvHandle->ctx->cq, 2, wc);
                if (ne < 0) {
                    fprintf(stderr, "poll CQ failed %d\n", ne);
                    return 1;
                }

            } while (ne < 1);

            for (int i = 0; i < ne; ++i) {
                if (wc[i].status != IBV_WC_SUCCESS) {
                    fprintf(stderr, "Failed status %s (%d) for wr_id %d\n", ibv_wc_status_str(wc[i].status),
                            wc[i].status, (int) wc[i].wr_id);
                    return 1;
                }

                switch ((int) wc[i].wr_id) {
                    case PINGPONG_SEND_WRID:
                        scntGetReqres--;
                        break;

                    default:
                        fprintf(stderr, "Completion for unknown wr_id %d\n", (int) wc[i].wr_id);
                        return 1;
                }

            }
        }
    }
    free(mr_msg);
    return 0;
};

int mkv_get(void *mkv_h, unsigned kv_id, const char *key, char **value, unsigned *length) {
    struct mkv_handle *m_handle = mkv_h;
    return kv_get(m_handle->kv_handle[kv_id], key, value, m_handle->clientBuffers, kv_id, length);
}

int getFreeBufferIndex(handle *kvHandle) {
    for (int i = 0; i < NUMBER_OF_BUFFERS; i++) {
        if (kvHandle->isUsed[i] == UN_USED_BUFFER) {
            kvHandle->isUsed[i] = USED_BUFFER;
            return i;
        }
    }

}




void kv_release(char *value) {
    if (value != NULL) {
        free(value);
    }
};

void mkv_send_credit(void *mkv_h, unsigned kv_id, unsigned how_many_credits) {
    struct mkv_handle *m_handle = mkv_h;
    kv_cmd cmd = SET_CREDIT;
    char *msg = malloc(sizeof(char) * 20);
    sprintf(msg, "%d:%d", cmd, how_many_credits);
    printf("Sending set credit msg: %s with size %d\n", msg, strlen(msg) + 1);

    sendMsgLogic(m_handle->kv_handle[kv_id], msg);
}

void mkv_close(void *mkv_h) {
    unsigned count;
    struct mkv_handle *master_handle = mkv_h;
    for (count = 0; count < master_handle->num_servers; count++) {
        ibv_ack_cq_events(master_handle->kv_handle[count]->ctx->cq, 0);
        pp_close_ctx(master_handle->kv_handle[count]->ctx);
        ibv_free_device_list(master_handle->kv_handle[count]->dev_list);
        free(master_handle->kv_handle[count]->rem_dest);
        free(master_handle->kv_handle[count]);
    }
    //ibv_free_device_list(master_handle->dev_list);

    free(master_handle);
}

void mkv_release(char *value, int kv_id, void *mkv_h) {
    struct mkv_handle *m_handle = mkv_h;
    struct handle *cur_handle = m_handle->kv_handle[kv_id];
    for (int i = 0; i < NUMBER_OF_BUFFERS; i++) {
        if (cur_handle->isUsed[i] == USED_BUFFER) {
            char *temp = m_handle->clientBuffers + (kv_id * MAX_MSG_TEST + i * MAX_MSG_TEST);
            if (strcmp(value, temp)) {
                //clear bufffer
                //memset(temp, 0, MAX_MSG_TEST);
                cur_handle->isUsed[i] = UN_USED_BUFFER;
                cur_handle->usedBuffers--;
            }
        }
    }
    mkv_send_credit(mkv_h, kv_id, 1);

}


int kv_close(void *kv_handle) {

    if (pp_close_ctx(((handle *) kv_handle)->ctx)) {
        return 1;
    }

    ibv_free_device_list(((handle *) kv_handle)->dev_list);
    free(((handle *) kv_handle)->rem_dest);
    return 0;
};

int processClientCmd(handle *kv_handle, char *msg) {
    printf("Processing Message %s\n", msg);
    if (strlen(msg) == 0) {
        fprintf(stderr, "Msg is empty!: %s\n", msg);
        return 0;
    }
    int cmd = 0;
    char *key;
    char *value;
    const char *delim = ":";
    cmd = atoi(strtok(msg, delim));
    key = strtok(NULL, delim);
    value = strtok(NULL, delim);


    if (cmd == SET_CMD) {
        addElement(key, value, kv_handle);

    } else if (cmd == GET_CMD) {
        char **retValue = malloc(sizeof(char *));

        int ret = getFromStore(kv_handle, key, retValue);
        if (ret) {
            fprintf(stderr, "Error in fetching value!\n");
            return 1;
        }
        printf("Sending value after 'get' msg: %s\n", *retValue);
        if (cstm_post_send(kv_handle->ctx->pd, kv_handle->ctx->qp, *retValue, strlen(*retValue) + 1)) {
            perror("Couldn't post send: ");
            return 1;
        }
    } else {
        fprintf(stderr, "Coudln't decide what's the msg! MsgCmd - %d\n", cmd);
    }

    return 0;
}


int mkv_open(struct kv_server_address *servers, void **mkv_h) {
    struct mkv_handle *ctx;
    //unsigned total_buffers_per_kv = sizeof(struct kv_client_eager_buffer) * EAGER_BUFFER_LIMIT;
    //unsigned total_buffers_per_kv = EAGER_BUFFER_LIMIT;

    unsigned count = 0;
    while (servers[count++].servername); /* count servers */
    count--;
    ctx = malloc(sizeof(*ctx) + count * sizeof(void *));
    if (!ctx) {
        return 1;
    }

    ctx->num_servers = count;
    //TODO maby change to limit number of buffers
    ctx->clientBuffersNum = NUMBER_OF_BUFFERS;
    ctx->clientBuffers = calloc(0, MAX_MSG_TEST * ctx->num_servers * ctx->clientBuffersNum);

    for (count = 0; count < ctx->num_servers; count++) {
        if (kvHandleFactory(&servers[count], MAX_MSG_TEST, g_argc, g_argv, &ctx->kv_handle[count])) {
            return 1;
        }
    }

    *mkv_h = ctx;
    return 0;
}

int
kvHandleFactory(struct kv_server_address *server, unsigned size, int argc, char *argv[], struct handle **p_kvHandle) {
    struct timeval start, end;
    char *servername = server->servername;
    int port = server->port;
    int ib_port = 1;
    enum ibv_mtu mtu = IBV_MTU_1024;
    int iters = 1000;
    int use_event = 0;
    int rcnt, scnt;
    int num_cq_events = 0;
    int sl = 0;
    int gidx;
    srand48(getpid() * time(NULL));

    while (1) {
        int c;

        static struct option long_options[] = {{.name = "port", .has_arg = 1, .val = 'p'},
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
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'p':
                port = strtol(optarg, NULL, 0);
                if (port < 0 || port > 65535) {
                    usage(argv[0]);
                    return 1;
                }
                break;

            case 'i':
                ib_port = strtol(optarg, NULL, 0);
                if (ib_port < 0) {
                    usage(argv[0]);
                    return 1;
                }
                break;

            case 's':
                size = strtol(optarg, NULL, 0);
                break;

            case 'm':
                mtu = pp_mtu_to_enum(strtol(optarg, NULL, 0));
                if (mtu < 0) {
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
                +use_event;
                break;

            default:
                usage(argv[0]);
                return 1;
        }
    }

    if (optind == argc - 1) {
        servername = strdup(argv[optind]);
    } else if (optind < argc) {
        usage(argv[0]);
        return 1;
    }

    handle *kvHandle = calloc(1, sizeof *kvHandle);
    *p_kvHandle = kvHandle;
    kvHandle->defMsgSize = size;
    kvHandle->ib_port = ib_port;
    kvHandle->rx_depth = 500;
    gidx = -1;

    page_size = sysconf(_SC_PAGESIZE);
    kv_open(servername, (void **) p_kvHandle);


    if (pp_get_port_info(kvHandle->ctx->context, ib_port, &kvHandle->ctx->portinfo)) {
        fprintf(stderr, "Couldn't get port info\n");
        return 1;
    }

    kvHandle->my_dest.lid = kvHandle->ctx->portinfo.lid;
    if (kvHandle->ctx->portinfo.link_layer == IBV_LINK_LAYER_INFINIBAND && !kvHandle->my_dest.lid) {
        fprintf(stderr, "Couldn't get local LID\n");
        return 1;
    }

    if (gidx >= 0) {
        if (ibv_query_gid(kvHandle->ctx->context, kvHandle->ib_port, gidx, &kvHandle->my_dest.gid)) {
            fprintf(stderr, "Could not get local gid for gid index %d\n", gidx);
            return 1;
        }
    } else {
        memset(&kvHandle->my_dest.gid, 0, sizeof kvHandle->my_dest.gid);
    }

    kvHandle->my_dest.qpn = kvHandle->ctx->qp->qp_num;
    kvHandle->my_dest.psn = lrand48() & 0xffffff;
    inet_ntop(AF_INET6, &kvHandle->my_dest.gid, kvHandle->gid, sizeof kvHandle->gid);
    printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n", kvHandle->my_dest.lid,
           kvHandle->my_dest.qpn, kvHandle->my_dest.psn, kvHandle->gid);


    if (servername) {
        kvHandle->rem_dest = pp_client_exch_dest(servername, port, &kvHandle->my_dest);
    } else {
        kvHandle->rem_dest = pp_server_exch_dest(kvHandle->ctx, ib_port, mtu, port, sl, &kvHandle->my_dest, gidx);
    }

    if (!kvHandle->rem_dest) {
        return 1;
    }

    inet_ntop(AF_INET6, &kvHandle->rem_dest->gid, kvHandle->gid, sizeof kvHandle->gid);
    printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n", kvHandle->rem_dest->lid,
           kvHandle->rem_dest->qpn, kvHandle->rem_dest->psn, kvHandle->gid);

    if (servername) {
        if (pp_connect_ctx(kvHandle->ctx, ib_port, kvHandle->my_dest.psn, mtu, sl, kvHandle->rem_dest, gidx)) {
            return 1;
        }
    }
    ibv_free_device_list(kvHandle->dev_list);
    free(kvHandle->rem_dest);

    return 0;
}


int dkv_open(struct kv_server_address *servers, /* array of servers */
             struct kv_server_address *indexer, /* single indexer */
             void **dkv_h) {

    struct dkv_ctx *ctx = malloc(sizeof(*ctx));
    if (kvHandleFactory(indexer, MAX_MSG_TEST, g_argc, g_argv, &ctx->indexer)) {
        return 1;
    }
    if (mkv_open(servers, (void **) &ctx->mkv)) {
        return 1;
    }
    *dkv_h = ctx;
    return 0;
}

int find_key_server(void *dkv_h, const char *key, char **findResultMsg, bool new_val) {
    struct dkv_ctx *m_handle = dkv_h;
    kv_cmd cmd = FIND_KEY_SERVER;
    if (new_val) {
        cmd = SET_FIND_KEY_SERVER;
    }
    char *msg = malloc(sizeof(char) * 20);
    sprintf(msg, "%d:%s:%d", cmd, key, m_handle->mkv->num_servers);
    printf("Sending FIND key - server for key: %s -  msg: %s with size %d\n", key, msg, strlen(msg) + 1);

    cstm_post_recv(m_handle->indexer->ctx->pd, m_handle->indexer->ctx->qp, *findResultMsg,
                   roundup(m_handle->indexer->defMsgSize, page_size));
    cstm_post_send(m_handle->indexer->ctx->pd, m_handle->indexer->ctx->qp, msg, strlen(msg) + 1);
    printf("Pooling for result value \n");
    int scnt = 1, recved = 1;
    int iterations = 10000;

    while ((scnt || recved) && iterations > 0) {
        struct ibv_wc wc[2];
        int ne;
        int pollIterations = 10000;
        do {
            ne = ibv_poll_cq(m_handle->indexer->ctx->cq, 2, wc);
            if (ne < 0) {
                fprintf(stderr, "poll CQ failed %d\n", ne);
                return 1;
            }
            pollIterations--;
        } while (ne < 1 && pollIterations > 0);

        for (int i = 0; i < ne; ++i) {
            if (wc[i].status != IBV_WC_SUCCESS) {
                fprintf(stderr, "Failed status %s (%d) for wr_id %d\n", ibv_wc_status_str(wc[i].status), wc[i].status,
                        (int) wc[i].wr_id);
                return 1;
            }

            switch ((int) wc[i].wr_id) {
                case PINGPONG_SEND_WRID:
                    scnt--;
                    break;

                case PINGPONG_RECV_WRID:
                    printf("Got server get prep msg: %s\n", *findResultMsg);


                    recved--;
                    break;


                default:
                    fprintf(stderr, "Completion for unknown wr_id %d\n", (int) wc[i].wr_id);
                    return 1;
            }

        }
        iterations--;

    }

    if (iterations == 0) {
        printf("Failed getting response from server for location of a proper key server for key: %s\n", key);
        return -1;
    }

}

int dkv_set(void *dkv_h, const char *key, const char *value, unsigned length) {
    struct dkv_ctx *ctx = dkv_h;

    /* Step #1: The client sends the Index server FIND(key, #kv-servers) */

    char *findResultMsg = malloc(roundup(ctx->mkv->defMsgSize, page_size));
    int foundKey = find_key_server(dkv_h, key, &findResultMsg, true);
    if (foundKey == -1) {
        return -1;
    }

    /* Step #2: The Index server responds with LOCATION(#kv-server-id) */

    printf("Processing server Message: %s\n", findResultMsg);
    if (strlen(findResultMsg) == 0) {
        fprintf(stderr, "Msg is empty!\n");
        return 0;
    };
    char *delim = ":";
    int CMD = atoi(strtok(findResultMsg, delim));
    int keyServerLocationID = atoi(strtok(NULL, delim));

    free(findResultMsg);

    if (CMD != KEY_SERVER_LOCATION) {
        printf("Didn't get correct message after sending request for key server location, got CMD: %d\n", CMD);
        return 1;
    }

    /* Step #3: The client contacts KV-server with the ID returned in LOCATION, using SET/GET messages. */
    return mkv_set(ctx->mkv, keyServerLocationID, key, value, length);
}


int dkv_get(void *dkv_h, const char *key, char **value, unsigned *length) {
    struct dkv_ctx *ctx = dkv_h;

    /* Step #1: The client sends the Index server FIND(key, #kv-servers) */

    char *findResultMsg = malloc(roundup(ctx->mkv->defMsgSize, page_size));
    int foundKey = find_key_server(dkv_h, key, &findResultMsg, false);
    if (foundKey == -1) {
        return -1;
    }

    /* Step #2: The Index server responds with LOCATION(#kv-server-id) */

    printf("Processing server Message: %s\n", findResultMsg);
    if (strlen(findResultMsg) == 0) {
        fprintf(stderr, "Msg is empty!\n");
        return 0;
    };
    char *delim = ":";
    int CMD = atoi(strtok(findResultMsg, delim));
    int keyServerLocationID = atoi(strtok(NULL, delim));

    free(findResultMsg);

    if (CMD != KEY_SERVER_LOCATION) {
        printf("Didn't get correct message after sending request for key server location, got CMD: %d\n", CMD);
        return 1;
    }

    /* Step #3: The client contacts KV-server with the ID returned in LOCATION, using SET/GET messages. */
    return mkv_get(ctx->mkv, keyServerLocationID, key, value, length);
}

void dkv_release(const char *key, char *value, int kv_id, void *dkv_h) {
    struct dkv_ctx *ctx = dkv_h;
    char *findResultMsg = malloc(roundup(ctx->mkv->defMsgSize, page_size));
    int foundKey = find_key_server(dkv_h, key, &findResultMsg, false);
    if (foundKey == -1) {
        return;
    }

    /* Step #2: The Index server responds with LOCATION(#kv-server-id) */

    printf("Processing server Message: %s\n", findResultMsg);
    if (strlen(findResultMsg) == 0) {
        fprintf(stderr, "Msg is empty!\n");
        return;
    };
    char *delim = ":";
    int CMD = atoi(strtok(findResultMsg, delim));
    int keyServerLocationID = atoi(strtok(NULL, delim));

    free(findResultMsg);

    if (CMD != KEY_SERVER_LOCATION) {
        printf("Didn't get correct message after sending request for key server location, got CMD: %d\n", CMD);
        return;
    }

    mkv_release(value, kv_id, ctx->mkv);
}

int dkv_close(void *dkv_h) {
    struct dkv_ctx *ctx = dkv_h;
    pp_close_ctx(ctx->indexer);
    mkv_close(ctx->mkv);
    free(ctx);
    return 0;
}

void recursive_fill_kv(char const *dirname, void *dkv_h) {
    struct dirent *curr_ent;
    DIR *dirp = opendir(dirname);
    if (dirp == NULL) {
        return;
    }

    while ((curr_ent = readdir(dirp)) != NULL) {
        if (!((strcmp(curr_ent->d_name, ".") == 0) || (strcmp(curr_ent->d_name, "..") == 0))) {
            char *path = malloc(strlen(dirname) + strlen(curr_ent->d_name) + 2);
            strcpy(path, dirname);
            strcat(path, "/");
            strcat(path, curr_ent->d_name);
            if (curr_ent->d_type == DT_DIR) {
                recursive_fill_kv(path, dkv_h);
            } else if (curr_ent->d_type == DT_REG) {
                int fd = open(path, O_RDONLY);
                size_t fsize = lseek(fd, (size_t) 0, SEEK_END);
                void *p = mmap(0, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
                /* TODO (1LOC): Add a print here to see you found the full paths... */
                dkv_set(dkv_h, path, p, fsize);
                munmap(p, fsize);
                close(fd);
            }
            free(path);
        }
    }
    closedir(dirp);
}


int main(int argc, char *argv[]) {
    void *kv_ctx; /* handle to internal KV-client context */

    g_argc = argc;
    g_argv = argv;

    struct kv_server_address indexer[2] = {{.servername = "mlx-stud-03", .port = 63335},
                                           {.servername = NULL, .port = 0}};

    struct kv_server_address servers[2] = {{.servername = "mlx-stud-02", .port = 65433},
                                           {.servername = NULL, .port = 0}};
    //assert(0 == mkv_open(servers, &kv_ctx));
    assert(0 == dkv_open(servers, indexer, &kv_ctx));

    char key[4] = "red";
    char value[10] = "wedding";
    char key2[5] = "red2";
    char value2[11] = "wedding2";
    struct dkv_ctx *ctx = kv_ctx;

    mkv_send_credit(ctx->mkv, 0, 50);

    if (dkv_set(kv_ctx, key, value, strlen(value) + 1)) {
        fprintf(stderr, "Couldn't post send\n");
        return 1;
    }
    char *retVal = malloc(4096);
    if (dkv_get(kv_ctx, key, &retVal, strlen(retVal) + 1)) {
        fprintf(stderr, "Couldn't post send\n");
        return 1;
    }
    retVal = malloc(4096);
    if (dkv_get(kv_ctx, key, &retVal, strlen(retVal) + 1)) {
        fprintf(stderr, "Couldn't post send\n");
        return 1;
    }
    //dkv_release(retVal, key, kv_ctx);

    if (dkv_set(kv_ctx, key2, value2, strlen(value2) + 1)) {
        fprintf(stderr, "Couldn't post send\n");
        return 1;
    }
    retVal = malloc(4096);
    if (dkv_get(kv_ctx, key2, &retVal, strlen(retVal) + 1)) {
        fprintf(stderr, "Couldn't post send\n");
        return 1;
    }
    retVal = malloc(4096);
    if (dkv_get(kv_ctx, key2, &retVal, strlen(retVal) + 1)) {
        fprintf(stderr, "Couldn't post send\n");
        return 1;
    }
    retVal = malloc(4096);
    if (dkv_get(kv_ctx, key, &retVal, strlen(retVal) + 1)) {
        fprintf(stderr, "Couldn't post send\n");
        return 1;
    }
    //dkv_release(retVal, 0, kv_ctx);

    //dkv_send_credit(kv_ctx, 0, 50);
    //    struct dkv_handle *m_handle = kv_ctx;
    //dkv_close(kv_ctx);
    //    mkv_send_credit(kv_ctx, 0, 2);

    //    //Complicated Test:
    //    char* msg = malloc((MAX_MSG_TEST * sizeof(char)) + 1);
    //    memset(msg,'w', MAX_MSG_TEST);
    //    msg[MAX_MSG_TEST] = '\0';
    //    printf("Start test\n");
    //
    //    char key[MAX_KEY];
    //    for(int i = 0; i < LOOP_ITER;i++){
    //        sprintf(key, "test%d", i);
    //        if (kv_set(kvHandle, key, msg))
    //        {
    //            fprintf(stderr, "Couldn't post send\n");
    //            return 1;
    //        }
    //    }
    //    for(int i = 0; i < LOOP_ITER;i++){
    //        char *returnedVal = malloc(MAX_MSG_TEST + 1);
    //        sprintf(key, "test%d", i);
    //        if (kv_get(kvHandle, key, &returnedVal))
    //        {
    //            fprintf(stderr, "Couldn't kv get the requested key\n");
    //            return 1;
    //        }
    //        //printf("Got value: %s", returnedVal);
    //        kv_release(returnedVal);
    //    }
    //    for(int i = 0; i < LOOP_ITER;i++){
    //        char *returnedVal = malloc(MAX_MSG_TEST + 1);
    //        sprintf(key, "test%d", i);
    //        if (kv_get(kvHandle, key, &returnedVal))
    //        {
    //            fprintf(stderr, "Couldn't kv get the requested key\n");
    //            return 1;
    //        }
    //        //printf("Got value: %s", returnedVal);
    //        kv_release(returnedVal);
    //    }
    //
    //    printf("Stop test\n");


//
//    int i, port, pid, listenfd, socketfd, hit;
//    socklen_t length;
//    static struct sockaddr_in cli_addr; /* static = initialised to zeros */
//    static struct sockaddr_in serv_addr; /* static = initialised to zeros */
//
//    if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
//        (void)printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
//                             "\tnweb is a small and very safe mini web server\n"
//                             "\tnweb only servers out file/web pages with extensions named below\n"
//                             "\t and only from the named directory or its sub-directories.\n"
//                             "\tThere is no fancy features = safe and secure.\n\n"
//                             "\tExample: nweb 8181 /home/nwebdir &\n\n"
//                             "\tOnly Supports:", VERSION);
//        for(i=0;extensions[i].ext != 0;i++)
//            (void)printf(" %s",extensions[i].ext);
//
//        (void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
//                             "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
//                             "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n"  );
//        exit(0);
//    }
//    if( !strncmp(argv[2],"/"   ,2 ) || !strncmp(argv[2],"/etc", 5 ) ||
//        !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) ||
//        !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) ||
//        !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){
//        (void)printf("ERROR: Bad top directory %s, see nweb -?\n",argv[2]);
//        exit(3);
//    }
//    if(chdir(argv[2]) == -1){
//        (void)printf("ERROR: Can't Change to directory %s\n",argv[2]);
//        exit(4);
//    }
//    /* Become deamon + unstopable and no zombies children (= no wait()) */
//    if(fork() != 0)
//        return 0; /* parent returns OK to shell */
//    (void)signal(SIGCLD, SIG_IGN); /* ignore child death */
//    (void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
//    for(i=0;i<32;i++)
//        (void)close(i);		/* close open files */
//    (void)setpgrp();		/* break away from process group */
//    logger(LOG,"nweb starting",argv[1],getpid());
//    /* setup the network socket */
//    if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
//        logger(ERROR, "system call","socket",0);
//    port = atoi(argv[1]);
//    if(port < 0 || port >60000)
//        logger(ERROR,"Invalid port number (try 1->60000)",argv[1],0);
//    serv_addr.sin_family = AF_INET;
//    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
//    serv_addr.sin_port = htons(port);
//    if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
//        logger(ERROR,"system call","bind",0);
//    if( listen(listenfd,64) <0)
//        logger(ERROR,"system call","listen",0);
//    for(hit=1; ;hit++) {
//        length = sizeof(cli_addr);
//        if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
//            logger(ERROR,"system call","accept",0);
//        if((pid = fork()) < 0) {
//            logger(ERROR,"system call","fork",0);
//        }
//        else {
//            if(pid == 0) { 	/* child */
//                (void)close(listenfd);
//                web(socketfd,hit); /* never returns */
//            } else { 	/* parent */
//                (void)close(socketfd);
//            }
//        }
//    }
//


    return 0;
}

