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

struct object {
		struct ds_obj_id id;
		char             *data;
		uint64_t		 data_off;
		uint32_t         size;
};
 
struct con_handle {
		int	sock;
		int con_id;
};


int  con_handle_init(struct con_handle *connection)
int  ds_connect(struct con_handle *con,char *ip,int port);
int  ds_disconnect(struct con_handle *con);
int  ds_create_object(struct con_handle *con, struct ds_obj_id obj_id, uint64_t obj_size); 
int  ds_put_object(struct con_handle *con,struct ds_obj_id id, void *data, uint32_t data_size, uint64_t off);
