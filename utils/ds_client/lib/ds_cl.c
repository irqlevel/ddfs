#include "include/ds_client.h"
#include "include/ds_packet.h"

#define PAGE_SIZE      4096
#define PART_NUM       10
#define PART_LEN	   5

int con_handle_init(struct con_handle *connection)
{
		int sock;
		
		sock = socket(AF_INET,SOCK_STREAM,0);
		if (sock == -1) {
				CLOG(CL_ERR, "con_handle_init() -> socket() failed");
				return 1;
		}
		else {
				connection->sock = sock;
				return 0;
		}
}

int ds_connect(struct con_handle *con,char *ip,int port)
{
		struct sockaddr_in serv_addr;
		int32_t ret;
		
		if (con_handle_init(con)) {
				CLOG(CL_ERR, "ds_connect() -> create connection failed");
				return 1;
		}
		
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
	
		ret=inet_aton(ip,&(serv_addr.sin_addr.s_addr));
		if(!ret) { 
				CLOG(CL_ERR, "ds_connect() -> inet_aton() failed, invalid address");
				return -EFAULT;
		}
		
		crt_memset(&(serv_addr.sin_zero),0,8);
	
		ret = connect(con->sock,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
		if (ret == -1) {
				CLOG(CL_ERR, "ds_connect() -> connect() failed");
				return -ENOTCONN;
		}
		return 0;
} 
int  ds_put_object(struct con_handle *con,struct ds_obj_id id, void *data, uint64_t data_size, uint64_t *off)
{
		int i,j;
		/* Create struct for each part of object, for test default 5 */
		struct ds_packet parts[PART_NUM];
		/* 
		 * Devide object into parts according to data size and offset 
		 * Offset incremented each step by lenght of object part (5)
		 */
		for(i=0;*off<=data_size;i++,*off+=PART_LEN) {
				parts[i].cmd = DS_PKT_OBJ_PUT; /* We send data to server */
				parts[i].obj_id = &id;
				for(j=0;j<PART_LEN;j++) {
						parts[i].data = (char *)malloc(sizeof(char)*PART_LEN);
						parts[i].data[j+*off] = data[j+*off];
				}
				parts[i].data_size = sizeof(parts[i]);
				/* Each part holds place where data start in specific object */
				parts[i].data_off = *off;
		}
		/* Send each part of object to the server */
		for(j=0;j<i;j++)
				if((send(sock,parts[j],sizeof(parts[j]),0))<0)
							CLOG(CL_ERR, "ds_put_object() -> packet number %d failed to send to server",j);
		return 0;
}

int  ds_create_object(struct con_handle *con, struct ds_obj_id obj_id, uint64_t obj_size);
{		
		struct ds_packet packet;
		
		packet.cmd = DS_PKT_OBJ_CREATE;
		packet.obj_id = &obj_id;
		packet.data_off = 0;
		packet.data_size = obj_size;
		
		return -ENOSYS;
}
int  ds_delete_object(struct con_handle *con,struct ds_obj_id obj_id)
{
		/* Not implemented */
		return -ENOSYS;
}
int  ds_get_object(struct con_handle *con,struct ds_obj_id id, void *data, uint64_t data_size, uint64_t off)
{
		/* Not implemented */
		return -ENOSYS;
}
int ds_close(struct con_handle *con)
{
		close(con->sock);
}
