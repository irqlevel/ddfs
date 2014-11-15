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
#include "crtlib/include/ds_obj_id.h"

struct object {
		struct ds_obj_id id;
		unsigned char    *data;
		uint64_t         size;
};
 
struct con_handle {
		int	sock;
		int con_id;
};

struct host {
		char *address;
		int  port;
};

int  con_handle_init(struct con_handle *connection)
int  ds_connect(struct con_handle *con,char *ip,int port);
int  ds_disconnect(struct con_handle *con);
