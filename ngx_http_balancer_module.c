#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdbool.h>
#include "utils.h"
#include "list/list.h"

typedef struct {
    struct sockaddr                *sockaddr;
    socklen_t                       socklen;
    ngx_str_t                       name;
    ngx_uint_t                      down;
    ngx_int_t                       weight;
    size_t                          total_memory;
    int                             process_time;
    list_t                          *list;
} ngx_http_upstrm_hash_peer_t;

typedef struct {
    ngx_uint_t                        number;
    ngx_uint_t                        total_weight;
    unsigned                          weighted:1;
    ngx_http_upstrm_hash_peer_t     peer[0];
} ngx_http_upstrm_hash_peers_t;

typedef struct {
    ngx_http_upstrm_hash_peers_t   *peers;
    char* file;
    long size;
//    uint32_t                          hash;
//    ngx_str_t                         current_key;
//    ngx_str_t                         original_key;
//    ngx_uint_t                        try_i;
//    uintptr_t                         tried[1];
} ngx_http_upstrm_hash_peer_data_t;


static char *
ngx_http_upstrm_hash(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t
ngx_http_upstream_init_hash(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_upstream_init_hash_peer(ngx_http_request_t *r,
                                                  ngx_http_upstream_srv_conf_t *us);
static ngx_int_t
ngx_http_upstream_get_hash_peer(ngx_peer_connection_t *pc, void *data);

static void
ngx_http_upstream_free_hash_peer(ngx_peer_connection_t *pc, void *data,
                                 ngx_uint_t state);

//static ngx_int_t ngx_http_hello_world_handler(ngx_http_request_t *r);

/**
 * This module provided directive: hello world.
 *
 */
static ngx_command_t ngx_http_upstrm_hash_commands[] = {

    { ngx_string("bum"), /* directive */
      NGX_HTTP_UPS_CONF|NGX_CONF_NOARGS, /* location context and takes
                                            no arguments*/
      ngx_http_upstrm_hash, /* configuration setup function */
      0, /* No offset. Only one context is supported. */
      0, /* No offset when storing the module configuration on struct. */
      NULL},

    ngx_null_command /* command termination */
};

/* The hello world string. */

/* The module context. */
static ngx_http_module_t ngx_http_upstrm_hash_module_ctx = {
    NULL, /* preconfiguration */
    NULL, /* postconfiguration */

    NULL, /* create main configuration */
    NULL, /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    NULL, /* create location configuration */
    NULL /* merge location configuration */
};

/* Module definition. */
ngx_module_t ngx_http_upstrm_hash_module = {
    NGX_MODULE_V1,
    &ngx_http_upstrm_hash_module_ctx, /* module context */
    ngx_http_upstrm_hash_commands, /* module directives */
    NGX_HTTP_MODULE, /* module type */
    NULL, /* init master */
    NULL, /* init module */
    NULL, /* init process */
    NULL, /* init thread */
    NULL, /* exit thread */
    NULL, /* exit process */
    NULL, /* exit master */
    NGX_MODULE_V1_PADDING
};
static char *
ngx_http_upstrm_hash(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    fprintf(stderr, "ngx_http_upstrm_hash 1\n");

    ngx_http_upstream_srv_conf_t   *uscf;
    ngx_http_script_compile_t       sc;
    ngx_str_t                      *value;
    ngx_array_t                    *vars_lengths, *vars_values;
//    ngx_http_upstrm_hash_conf_t  *uhcf;

    value = cf->args->elts;
    fprintf(stderr, "ngx_http_upstrm_hash 2\n");


    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

    vars_lengths = NULL;
    vars_values = NULL;

    sc.cf = cf;
    sc.source = &value[1];
    sc.lengths = &vars_lengths;
    sc.values = &vars_values;
    sc.complete_lengths = 1;
    sc.complete_values = 1;
    fprintf(stderr, "ngx_http_upstrm_hash 3\n");

    if (ngx_http_script_compile(&sc) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

    uscf->peer.init_upstream = ngx_http_upstream_init_hash;

    fprintf(stderr, "ngx_http_upstrm_hash 4\n");

    uscf->flags = NGX_HTTP_UPSTREAM_CREATE
                  |NGX_HTTP_UPSTREAM_WEIGHT
                  |NGX_HTTP_UPSTREAM_DOWN;

//    uhcf = ngx_http_conf_upstream_srv_conf(uscf, ngx_http_upstrm_hash_module);

    fprintf(stderr, "ngx_http_upstrm_hash 5\n");
//    uhcf->values = vars_values->elts;
//    uhcf->lengths = vars_lengths->elts;

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_upstream_init_hash(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    fprintf(stderr, "ngx_http_upstream_init_hash\n");

    ngx_uint_t                       i, j, n, w;
    ngx_http_upstream_server_t      *server;
    ngx_http_upstrm_hash_peers_t  *peers;
#if (NGX_HTTP_HEALTHCHECK)
    ngx_int_t                        health_index;
#endif
    us->peer.init = ngx_http_upstream_init_hash_peer;

    if (!us->servers) {
        return NGX_ERROR;
    }

    server = us->servers->elts;

    n = 0;
    w = 0;
    fprintf(stderr, "server_count %d", (int)us->servers->nelts);
    for (i = 0; i < us->servers->nelts; i++) {
        if (server[i].backup)
            continue;

        n += server[i].naddrs;
        w += server[i].naddrs * server[i].weight;
    }

    if (n == 0) {
        return NGX_ERROR;
    }

    peers = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstrm_hash_peers_t)
                                  + sizeof(ngx_http_upstrm_hash_peer_t) * n);

    if (peers == NULL) {
        return NGX_ERROR;
    }

    peers->number = n;
    peers->weighted = (w != n);
    peers->total_weight = w;

    n = 0;
    /* one hostname can have multiple IP addresses in DNS */
    for (i = 0; i < us->servers->nelts; i++) {
        for (j = 0; j < server[i].naddrs; j++) {
            if (server[i].backup)
                continue;
            peers->peer[n].sockaddr = server[i].addrs[j].sockaddr;
            peers->peer[n].socklen = server[i].addrs[j].socklen;
            peers->peer[n].name = server[i].addrs[j].name;
            fprintf(stderr, "peer name %s\n", (char*)peers->peer[n].name.data);
            peers->peer[n].down = server[i].down;
            peers->peer[n].weight = server[i].weight;
            fprintf(stderr, "peer weight %d\n", (int)server[i].weight);
            peers->peer[n].total_memory = 1000000000;
            peers->peer[n].list = list_new();

            n++;
        }
    }

    us->peer.data = peers;

    return NGX_OK;
}


inline long fsize(FILE* fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return size;
}

char buf_for_args[400];
/*
long getSize()
{

}*/
static ngx_int_t
ngx_http_upstream_init_hash_peer(ngx_http_request_t *r,
                                 ngx_http_upstream_srv_conf_t *us)
{
    fprintf(stderr, "ngx_http_upstream_init_hash_peer\n");

    ngx_http_upstrm_hash_peer_data_t     *uhpd;
//    fprintf(stderr, "1\n");

    uhpd = ngx_pcalloc(r->pool, sizeof(ngx_http_upstrm_hash_peer_data_t)
                                + sizeof(uintptr_t) *
                                  ((ngx_http_upstrm_hash_peers_t *)us->peer.data)->number /
                                  (8 * sizeof(uintptr_t)));
    if (uhpd == NULL) {
        return NGX_ERROR;
    }
//    fprintf(stderr, "2\n");

    r->upstream->peer.data = uhpd;
    uhpd->peers = us->peer.data;

    uhpd->file = getPath(r->args_start, buf_for_args);

    FILE *fp;
    fp = fopen(uhpd->file, "rb");

    if (fp == NULL) {
        fprintf(stderr, "can not read %s\n", uhpd->file);
        perror("system error");
        return NGX_ERROR;
    }
    uhpd->size = fsize(fp);
    fclose(fp);
    bool have_possible = false;
    for(int i = 0; i < (int)uhpd->peers->number; i++)
    {
        list_node_t *node = find(uhpd->peers->peer[i].list, uhpd->file);
        if (node != NULL) {
//            fprintf(stderr, "######hit the cache\n");
            uhpd->peers->peer[i].process_time = 0;
            have_possible = true;
        } else
        {
            if(uhpd->peers->peer[i].total_memory >= (size_t)uhpd->size)
                have_possible = true;
            uhpd->peers->peer[i].process_time = 100; //change to calculate time
        }
    }

    if(!have_possible)
        return NGX_ERROR;
    r->upstream->peer.free = ngx_http_upstream_free_hash_peer;
    r->upstream->peer.get = ngx_http_upstream_get_hash_peer;
//    fprintf(stderr, "3\n");

    return NGX_OK;
}


size_t getTotalMemoryUsage(list_t* list) {
    size_t total = 0;
    list_node_t *node;
    list_iterator_t *it = list_iterator_new(list, LIST_HEAD);
    while ((node = list_iterator_next(it))) {
        total += node->size;
    }
    return total;
}

void releaseMemory(ngx_http_upstrm_hash_peer_t peer, size_t need_memory) {
    list_node_t *node = NULL;
    size_t free_memory = peer.total_memory- getTotalMemoryUsage(peer.list);
    while (need_memory > free_memory) {
        node = list_lpop(peer.list);
        free_memory += node->size;
    }
}

uint8_t checkFreeMemoryEnough(ngx_http_upstrm_hash_peer_t peer, size_t file_size) {
    return file_size > (peer.total_memory - getTotalMemoryUsage(peer.list)) ? 0 : 1;
}

//разобраться с осовбождением из памяти нужного места
//разобраться , если памяти не хватает
static ngx_int_t
ngx_http_upstream_get_hash_peer(ngx_peer_connection_t *pc, void *data)
{
    fprintf(stderr, "ngx_http_upstream_get_hash_peer\n");

    ngx_http_upstrm_hash_peer_data_t  *uhpd = data;
    ngx_http_upstrm_hash_peer_t       *peer;

    int index = 0;
    for(int i = 1; i < (int)uhpd->peers->number; i++)
    {
        if(uhpd->peers->peer[index].process_time > uhpd->peers->peer[i].process_time)
            index = i;
        else if(uhpd->peers->peer[index].process_time == uhpd->peers->peer[i].process_time)
            if(uhpd->peers->peer[index].list->len > uhpd->peers->peer[i].list->len)
                index = i;
    }

    if (0 == checkFreeMemoryEnough(uhpd->peers->peer[index], uhpd->size)) {
        fprintf(stderr, "!have not need free memory\n");
        releaseMemory(uhpd->peers->peer[index], uhpd->size);
    }

    list_node_t *a = list_node_new(uhpd->size, uhpd->file , 0);
    list_rpush(uhpd->peers->peer[index].list, a);

    peer = &uhpd->peers->peer[index];
    fprintf(stderr, "choose %d\n server with name %s", index, peer->name.data);

    pc->sockaddr = peer->sockaddr;
    pc->socklen = peer->socklen;
    pc->name = &peer->name;

    return NGX_OK;
}

static void
ngx_http_upstream_free_hash_peer(ngx_peer_connection_t *pc, void *data,
                                 ngx_uint_t state)
{
    /*
    ngx_http_upstrm_hash_peer_data_t  *uhpd = data;
    ngx_uint_t                           current;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "upstrm_hash: free upstream hash peer try %ui", pc->tries);

    if (state & (NGX_PEER_FAILED|NGX_PEER_NEXT)
        && (ngx_int_t) pc->tries > 0) {
        current = ngx_http_upstream_get_hash_peer_index(uhpd);

        uhpd->tried[ngx_bitvector_index(current)] |= ngx_bitvector_bit(current);
        ngx_http_upstrm_hash_next_peer(uhpd, &pc->tries, pc->log);
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "upstrm_hash: Using %ui because %ui failed",
                       ngx_http_upstream_get_hash_peer_index(uhpd), current);
    } else {
        pc->tries = 0;
    }*/
}


