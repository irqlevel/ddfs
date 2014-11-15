#include "ds_cl.h"

#define PAGE_SIZE 4096

struct ds_packet {
		uint16_t         cmd;     
		struct ds_obj_id obj_id;
		void 			 *data;
		uint32_t 		 data_size;
		uint64_t 		 data_off
};

static int count = 0;

int con_handle_init(struct con_handle *connection)
{
		int sock;
		
		count++;
		sock = socket(AF_INET,SOCK_STREAM,0);
		if (sock == -1) {
				CLOG(CL_ERR, "con_handle_init() -> socket() failed");
				return 1;
		}
		else {
				connection->sock = sock;
				connection->con_id = count;
				return 0;
		}
}

struct con_handle ds_connect(char *ip,int port)
{
		struct con_handle con;
		struct sockaddr_in serv_addr;
		int32_t ret;
	
		ret=con_handle_init(&con);
		if (ret) {
					CLOG(CL_ERR, "con_handle_init() failed");
					return 1;
		}
		
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
	
		ret=inet_aton(ip,&(serv_addr.sin_addr.s_addr));
		if(!ret) { 
				CLOG(CL_ERR, "ds_connect() -> inet_aton() failed");
				goto out;
		}
		
		crt_memset(&(serv_addr.sin_zero),0,8);
	
		ret = connect(con.sock,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
		if (ret == -1) {
				CLOG(CL_ERR, "ds_connect() -> connect() failed");
			return -ENOTCONN;
		}
		
		return 0;
} 

int ds_disconnect(struct con_handle *con)
{
		close(con->sock);
}
//int ds_object_put(struct con_handle handle, struct ds_obj_id object_id, void *object_body, uint64_t object_size)
int ds_object_put(struct con_handle *hanlde, struct object *obj) 
{
	ssize_t bytes_sent;
	struct ds_packet packet[];
	
	split(object_body
	/*
	if ((bytes_sent=send(handle.socket,msg,len,0))==-1) {
		perror("send()");
		return 0;
	}
	*/
	return 0;
}
