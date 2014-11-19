#pragma once

#define PAGE_SIZE 4096

enum packet_cmd
{
		DS_PKT_OBJ_PUT;
		DS_PKT_OBJ_GET 
		DS_PKT_OBJ_CREATE, 
		DS_PKT_OBJ_DELETE
};
/*
struct DS_PKT_OBJ_PUT {
		int put_cmd;
}
struct DS_PKT_OBJ_GET {
		int get_cmd;
}
struct DS_PKT_OBJ_CREATE {
		int create_cmd;
}
struct DS_PKT_OBJ_DELETE {
		int del_cmd;
}	
*/
struct ds_packet {
		enum packet_cmd  cmd;   
		struct ds_obj_id *obj_id;
		void 			 *data;
		uint64_t 		 data_size;
		uint64_t 		 data_off;
		int              error;
};
