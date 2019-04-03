//
// Created by mugutdinov on 31.03.19.
//
#include "utils.h"

list_node_t *find(list_t *list, const char *file) {
    list_node_t *node = NULL;
    list_iterator_t *it = list_iterator_new(list, LIST_HEAD);
    while ((node = list_iterator_next(it))) {
        if (strcmp(node->file, file) == 0)
            return node;
    }
    return NULL;
}
char *getPath(u_char *args, char *buf) {
    strcpy(buf, (char *) args);

    char *file = strtok(buf, "=");//skip first ("path" word)
    file = strtok(NULL, "= ");
    return file;
}