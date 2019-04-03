
//
// node.c
//
// Copyright (c) 2010 TJ Holowaychuk <tj@vision-media.ca>
//

#include <string.h>
#include "list.h"

/*
 * Allocates a new list_node_t. NULL on failure.
 */

list_node_t *
list_node_new(long size, const char* file, int calculate_time) {
  list_node_t *self;
  if (!(self = LIST_MALLOC(sizeof(list_node_t))))
    return NULL;
  self->prev = NULL;
  self->next = NULL;
  self->size = size;
  self->calculta_time = calculate_time;
  strcpy(self->file, file);
  return self;
}