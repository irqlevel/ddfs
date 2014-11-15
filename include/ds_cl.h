#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "ds_obj_id.h"

#define PAGE_SIZE 4096

struct object {
		struct ds_obj_id id;
		unsigned char    *data;
		uint64_t         size;
};
 
struct con_handle {
		int	sock;
		int con_id;
		int state; /* active/passiv */
};


void 			  con_handle_init(struct con_handle *connection);

struct con_handle ds_connect(char *ip,int port);

int               ds_object_put(struct con_handle handle, struct object *obj);
