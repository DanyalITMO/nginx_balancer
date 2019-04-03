//
// Created by mugutdinov on 31.03.19.
//
#include <ngx_config.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "list/list.h"

#ifndef NGINX_BALANCER_UTILS_H
#define NGINX_BALANCER_UTILS_H

#endif //NGINX_BALANCER_UTILS_H
char *getPath(u_char *args, char *buf);
list_node_t *find(list_t *list, const char *file);