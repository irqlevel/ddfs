#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <malloc.h>

#include <crtlib/include/crtlib.h>
#include <utils/ucrt/include/ucrt.h>
#include <include/ds_cl.h>

/* Temp defines */
#define CON_NUM  3
#define HOST_NUM 2

int main(int argc, const char *argv[])
{
		int err = DS_E_BUF_SMALL;
		int i;
		struct ds_obj_id *obj_id;
		/*
		 * Create an array of hosts
		 * In future there will be function for dynamic allocation
		 */
		struct host host_list[HOST_NUMBER];
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
		
		/* Connect to all computers in network */
		for(i=0;i<CON_NUMBER;i++)
				if (con_handle_init(&con[i]))
						CLOG(CL_ERR, "create connection number %d failed",i);
		
		ds_add_host("127.0.0.1",9999);
		host_list[0].address = ;
		host_list[0].port = ;
		host_list[0].address = "127.0.0.1";
		host_list[0].port = 9999;
		
		/* Connect to all computers in network */
		for(i=0;i<CON_NUMBER;i++)
				ds_connect(&con[i],host_list[i].address,host_list[i].port);
		
		/* Disconnect from all hosts */
		 
		for(i=0;i<CON_NUMBER;i++)
				ds_disconnect(&con[i].sock);
		
	
	
	/* run sha256 test from crtlib */
	__sha256_test();

	/* generate object id and output it */
	obj_id = ds_obj_id_gen();
	if (!obj_id) {
		CLOG(CL_ERR, "cant generate obj id");
	} else {
		char *obj_id_s = ds_obj_id_to_str(obj_id);
		if (!obj_id_s) {
			CLOG(CL_ERR, "cant convert obj id to str");
		} else {
			/* Log obj id */
			CLOG(CL_INF, "generated obj id %s", obj_id_s);
			crt_free(obj_id_s);
		}
		crt_free(obj_id);
	}

	return 0;
}
