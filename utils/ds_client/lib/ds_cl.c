#include "ds_cl.h"

#define PAGE_SIZE      4096
#define PART_NUM       10
#define PART_LEN	   5
/* Packet commands */
#define CMD_PUT        1
#define CMD_GET        2
#define CMD_DELETE 	   3
#define CMD_DISCONNECT 4
/* Packet commands */

struct ds_packet {
		uint16_t         cmd; /* (PUT = 1, GET = 2, DELETE = 3, DISCONNECT = 4)  */  
		struct ds_obj_id obj_id;
		char 			 data[PAGE_SIZE];
		uint32_t 		 data_size;
		uint64_t 		 data_off
};
static int con_count = 0;

int con_handle_init(struct con_handle *connection)
{
		int sock;
		
		con_count++;
		sock = socket(AF_INET,SOCK_STREAM,0);
		if (sock == -1) {
				CLOG(CL_ERR, "con_handle_init() -> socket() failed");
				return 1;
		}
		else {
				connection->sock = sock;
				connection->con_id = con_count;
				return 0;
		}
}

int ds_connect(struct con_handle *con,char *ip,int port)
{
		struct sockaddr_in serv_addr;
		int32_t ret;
	
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
	
		ret=inet_aton(ip,&(serv_addr.sin_addr.s_addr));
		if(!ret) { 
				CLOG(CL_ERR, "ds_connect() -> inet_aton() failed, invalid address");
				return 1;
		}
		
		crt_memset(&(serv_addr.sin_zero),0,8);
	
		ret = connect(con->sock,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
		if (ret == -1) {
				CLOG(CL_ERR, "ds_connect() -> connect() failed");
				return -ENOTCONN;
		}
		return 0;
} 

/* Not reay yet */
int  ds_create_object(struct con_handle *con, struct ds_obj_id obj_id, uint64_t obj_size);
{
		return 0;
}

int  ds_put_object(struct con_handle *con,struct ds_obj_id id, void *data, uint32_t data_size, uint64_t *off)
{
		int i,j;
		/* Create struct for each part of object, for test default 5 */
		struct ds_packet parts[PART_NUM];
		/* 
		 * Devide object into parts according to data size and offset 
		 * Offset incremented each step by lenght of object part (5)
		 */
		for(i=0;*off<data_size;i++,*off+=PART_LEN) {
				parts[i].cmd = CMD_PUT; /* We send data to server */
				parts[i].obj_id = id;
				for(j=0;j<PART_LEN;j++) 
						parts[i].data[j] = data[j+*off];
				parts[i].data_size = sizeof(parts[i]);
				/* Each part holds place where data start in specific object */
				parts[i].data_off = *off;
		}
		/* Send each part of object to server */
		for(j=0;j<(i-1);j++)
				if((send(sock,parts[j],sizeof(parts[j]),0))<0)
							CLOG(CL_ERR, "ds_put_object() -> packet number %d failed to send to server",(j+1));
		return 0;
}

int ds_disconnect(struct con_handle *con)
{
		close(con->sock);
}
