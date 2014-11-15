#include "xcat.h"

struct ds_packet  {
    int              cmd; 
    struct ds_obj_id obj_id;
    int              obj_chunk_seq_no;
    char             obj_chunk[PAGE_SIZE];
};

static int count = 0;


void con_handle_init(struct con_handle *connection)
{
	count++;
	
	connection->sock = socket(AF_INET,SOCK_STREAM,0);
	
	if (connection->sock == -1)
		perror("ds_init()");
		
	connection->con_id = count;
	connection->state=1; 
}

struct con_handle ds_connect(char *ip,int port)
{
	struct con_handle con;
	struct sockaddr_in serv_addr;
	short ret;
	
	con_handle_init(&con);
	
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(port);
	
	ret=inet_aton(ip,&(serv_addr.sin_addr.s_addr));
	
	if(!ret) 
		perror("IP address is not valid");
		
	
	memset(&(serv_addr.sin_zero),0,8);
	
	ret = connect(con.sock,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
	
	if (ret==-1) 
		perror("connect()");
		
	
	close(con.sock);
	
	return con;
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
