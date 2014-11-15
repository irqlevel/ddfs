#include "ds_cl.h"

#define PAGE_SIZE      4096
#define PART_NUM       10
#define CMD_PUT        1
#define CMD_GET        2
#define CMD_DELETE 	   3
#define CMD_DISCONNECT 4

struct ds_packet {
		uint16_t         cmd; /* (PUT = 1, GET = 2, DELETE = 3, DISCONNECT = 4)  */  
		struct ds_obj_id obj_id;
		void 			 *data;
		uint32_t 		 data_size;
		uint64_t 		 data_off
};
static int con_count = 0;

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
		int i;
		struct ds_packet parts[PART_NUM];
		
		for(i=0;i<PART_NUM;i++) {
				parts[i].cmd = CMD_PUT; 
				parts[i].obj_id = id;
				parts[i].data[i] = data[i+*off];
				parts[i].data_size = data_size;
				parts[i].data_off = off;
		}
		/* Devide object data into 10 parts | part lenght is 5*/
		off = 0;
		for(i=0;i<PART_NUM;i++)
				for(j=0;j<strlen(obj_parts[i]);j++) {
						obj_parts[i][j] = data[j+off];
						off+=(j+1);
				}
				
		bytes_sent=send(sock,msg,len,0);
			
		return 0;
}

int ds_disconnect(struct con_handle *con)
{
		close(con->sock);
}
