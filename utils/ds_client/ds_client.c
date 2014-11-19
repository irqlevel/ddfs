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
#include <include/ds_client.h> /* client lib */

#define CON_NUM  3

int main(int argc, const char *argv[])
{
		int err = DS_E_BUF_SMALL;
		int i,j;
		struct object obj;
		/*
		 * Create an array for connections 
		 * In future there will be function for dynamic allocation
		 * every time computer connects to the network and becomes neighbour
		 * con_handle struct will be added */
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
		
		/* Connect to two neighbours in network group */
		ds_connect(&con[0],"192.168.1.200",9999);
		ds_connect(&con[1],"192.168.1.245",8700);
		
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
		
		/* Fill object data | lenght is 50*/
		obj.data = "teststringteststringteststringteststringteststring";
		obj.size = sizeof(obj.data);
		obj.data_off = 0;
		
		 /*
		  * Not implemented
		if(ds_create_object(&con[0],obj.id,(sizeof(obj.data)/2)))
				CLOG(CL_ERR, "cant reserve space for object on storage");
		*/										
		/* Send object to first node */
		ds_put_object(&con[0],obj.id,obj.data,obj.size,&obj.data_off);
		
		crt_free(obj.id);
		/* Disconnect from all hosts */
		for(i=0;i<CON_NUM;i++)
				ds_close(&con[i].sock);
		
		return 0;
}
