#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <malloc.h>

#include <crtlib/include/crtlib.h>
#include <utils/ucrt/include/ucrt.h>
#include <include/ds_cl.h> /* client lib */

/* Temp defines */
#define CON_NUM  3
#define PART_NUM 10
#define PART_LEN 5

int main(int argc, const char *argv[])
{
		int err = DS_E_BUF_SMALL;
		int i,j,off;
		uint64_t size;
		struct object obj;
		/* Temp. Object will hold char string and be devided in 10 parts */
		char obj_parts[PART_NUM][PART_LEN];
		/*
		 * Create an array of connections 
		 * In future there will be function for dynamic allocation
		 */
		struct con_handle con[CON_NUMBER]; 
		
		/* 
	     * set CLOG log path : NULL and log name ds_client.log
	     * NULL means use current working dir as path
	     */
		ucrt_log_set_path(NULL, "ds_client.log");
		/* set CLOG log level */
		ucrt_log_set_level(CL_DBG);

		CLOG(CL_INF, "Hello from ds client");
		/* translate error code to string description */
		CLOG(CL_INF, "err %x - %s", err, ds_error(err));
		
		/* Create maximum available connections */
		for(i=0;i<CON_NUM;i++)
				if (con_handle_init(&con[i]))
						CLOG(CL_ERR, "create connection number %d failed",i);
		
		/* Connect to all computers in network */
		for(i=0;i<CON_NUM;i++)
				ds_connect(&con[i],ip,port);
		
		/* generate object id and output it */
		obj.id = ds_obj_id_gen();
		if (!obj.id) {
				CLOG(CL_ERR, "cant generate obj id");
		} else {
				char *obj_id_s = ds_obj_id_to_str(obj.id);
				if (!obj_id_s) {
						CLOG(CL_ERR, "cant convert obj id to str");
		        } else {
						/* Log obj id */
						CLOG(CL_INF, "generated obj id %s", obj_id_s);
						crt_free(obj_id_s);
		        }
		}
		
		size=30;
		/* Allocate space for our object on server using first connection*/
		if(ds_create_object(&con[0],obj.id,size))
				CLOG(CL_ERR, "cant reserve space for object on storage");
				
		/* Fill object data | len 50*/
		obj.data="teststringteststringteststringteststringteststring";
		
		/* Devide object data into 10 parts */
		off = 0;
		for(i=0;i<PART_NUM;i++)
				for(j=0;j<PART_LEN;j++) {
						obj_parts[i][j] = obj.data[j+off];
						off+=(j+1);
				}	
				
		/* Send object parts to server */
		ds_put_object(&con[i],obj.id,obj.data,obj.size,obj.data_off);
			
		crt_free(obj.id);
		/* Disconnect from all hosts */
		for(i=0;i<CON_NUM;i++)
				ds_disconnect(&con[i].sock);
		
		return 0;
}
